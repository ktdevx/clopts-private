/*
 * Suite env: environment lookup contexts (creation, release, the failure
 * record and the buffers) and clopts_env_lookup on every OS (getenv outside
 * Windows, the UTF-16 environment and UTF-8 conversion on Windows).
 *
 * It also checks the code unit array helper of test_util.
 */

/*
 * Declares setenv / unsetenv outside Windows even in C99 without language
 * extensions (-std=c99 -Werror). Must come before every include.
 */
#define _POSIX_C_SOURCE 200112L

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

NAOS_TEST(u16_from_units_copies_any_unit_and_terminates)
{
    /* a lone high surrogate, a pair, an embedded 0 and the largest unit */
    static const uint16_t units[] = {0xD800, 0xD83D, 0xDE00, 0x0000, 0xFFFF, 0x0041};
    platform_test_u16_t s;
    clopts_platform_u16_t *out;
    size_t i;

    out = platform_test_u16_from_units(&s, units, sizeof(units) / sizeof(units[0]));
    NAOS_CHECK_PTR_EQ(out, s.units);
    NAOS_CHECK_UINT_EQ(s.len, 6);
    for (i = 0; i < s.len; ++i)
    {
        NAOS_CHECK_UINT_EQ(s.units[i], units[i]);
    }
    NAOS_CHECK_UINT_EQ(s.units[6], 0);
}

NAOS_TEST(u16_from_units_accepts_an_empty_sequence)
{
    platform_test_u16_t s;

    platform_test_u16_from_units(&s, NULL, 0);
    NAOS_CHECK_UINT_EQ(s.len, 0);
    NAOS_CHECK_UINT_EQ(s.units[0], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, ""));
}

NAOS_TEST(u16_eq_ascii_does_not_match_non_ascii_units)
{
    /* U+3041 has the low byte 0x41 ('A'); it must not compare equal */
    static const uint16_t units[] = {0x3041};
    platform_test_u16_t s;

    platform_test_u16_from_units(&s, units, 1);
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "A"));
}

/* ---- contexts on every OS -------------------------------------------------- */

/* Checks that p is not NULL and ends the test when it is, so that nothing reads through it. */
#define PLATFORM_ENV_REQUIRE(p) \
    do \
    { \
        NAOS_CHECK_NOT_NULL(p); \
        if ((p) == NULL) \
        { \
            return; \
        } \
    } while (0)

NAOS_TEST(env_new_starts_without_failure_and_free_releases_it)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);
    NAOS_CHECK_PTR_EQ(env->probe, &probe);
    NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_error_name(env));
    NAOS_CHECK_NULL(env->value);
    NAOS_CHECK_UINT_EQ(env->value_cap, 0);
    NAOS_CHECK_NULL(env->wide);
    NAOS_CHECK_UINT_EQ(env->wide_cap, 0);
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(public_env_new_has_no_probe)
{
    clopts_env_t *env;

    env = clopts_env_new();
    PLATFORM_ENV_REQUIRE(env);
    NAOS_CHECK_NULL(env->probe);
    NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_error_name(env));
    clopts_env_free(env);
}

NAOS_TEST(env_free_ignores_null)
{
    clopts_env_free(NULL);
    NAOS_CHECK_TRUE(1);
}

NAOS_TEST(env_accessors_of_null_read_as_out_of_memory)
{
    NAOS_CHECK_INT_EQ(clopts_env_error(NULL), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_env_error_name(NULL));
}

NAOS_TEST(env_new_returns_null_when_the_allocation_fails)
{
    clopts_platform_probe_t probe = {1, 0, 0};

    NAOS_CHECK_NULL(clopts_platform_env_new(&probe));
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(env_fail_records_kind_and_a_copy_of_name_and_clear_drops_it)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;
    char name[] = "CLOPTS_NAME";

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);

    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_UTF16, name);
    NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_INVALID_UTF16);
    PLATFORM_ENV_REQUIRE(clopts_env_error_name(env));
    NAOS_CHECK_TRUE(clopts_env_error_name(env) != name);
    NAOS_CHECK_STR_EQ(clopts_env_error_name(env), "CLOPTS_NAME");
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    /* the record is a copy: changing the caller's string does not change it */
    name[0] = 'X';
    NAOS_CHECK_STR_EQ(clopts_env_error_name(env), "CLOPTS_NAME");

    /* a second failure replaces the first without leaking it */
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_NAME, "");
    NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
    NAOS_CHECK_STR_EQ(clopts_env_error_name(env), "");
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    clopts_platform_env_clear(env);
    NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_error_name(env));
    NAOS_CHECK_UINT_EQ(probe.live, 1);

    /* clearing a clear record changes nothing */
    clopts_platform_env_clear(env);
    NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_error_name(env));

    /* free releases a recorded name */
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_OUT_OF_MEMORY, "N");
    NAOS_CHECK_UINT_EQ(probe.live, 2);
    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(env_fail_records_out_of_memory_without_name_when_the_copy_fails)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_UTF16, "OLD");
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    probe.fail_at = probe.calls + 1;
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_NAME, "CLOPTS_NAME");
    NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_env_error_name(env));
    /* the earlier name is released, not leaked */
    NAOS_CHECK_UINT_EQ(probe.live, 1);

    probe.fail_at = 0;
    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(env_reserve_value_grows_and_never_shrinks)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;
    char *first;

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 0), 0);
    NAOS_CHECK_NOT_NULL(env->value);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 16), 0);
    PLATFORM_ENV_REQUIRE(env->value);
    NAOS_CHECK_UINT_GE(env->value_cap, 16);
    memset(env->value, 'v', 16);
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    /* a smaller or equal request keeps the buffer and allocates nothing */
    first = env->value;
    probe.calls = 0;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 4), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 16), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 0), 0);
    NAOS_CHECK_PTR_EQ(env->value, first);
    NAOS_CHECK_UINT_GE(env->value_cap, 16);
    NAOS_CHECK_UINT_EQ(probe.calls, 0);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 4096), 0);
    PLATFORM_ENV_REQUIRE(env->value);
    NAOS_CHECK_UINT_GE(env->value_cap, 4096);
    memset(env->value, 'w', 4096);
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(env_reserve_value_keeps_the_old_buffer_when_the_allocation_fails)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;
    char *first;
    size_t cap;

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);

    /* no buffer yet */
    probe.fail_at = probe.calls + 1;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 8), -1);
    NAOS_CHECK_NULL(env->value);
    NAOS_CHECK_UINT_EQ(env->value_cap, 0);
    NAOS_CHECK_UINT_EQ(probe.live, 1);

    probe.fail_at = 0;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 8), 0);
    first = env->value;
    cap = env->value_cap;
    probe.fail_at = probe.calls + 1;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 64), -1);
    NAOS_CHECK_PTR_EQ(env->value, first);
    NAOS_CHECK_UINT_EQ(env->value_cap, cap);
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    probe.fail_at = 0;
    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(env_reserve_wide_grows_and_never_shrinks)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;
    clopts_platform_u16_t *units;
    void *first;
    size_t i;

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 0), 0);
    NAOS_CHECK_NOT_NULL(env->wide);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 32), 0);
    PLATFORM_ENV_REQUIRE(env->wide);
    NAOS_CHECK_UINT_GE(env->wide_cap, 32);
    units = (clopts_platform_u16_t *)env->wide;
    for (i = 0; i < 32; ++i)
    {
        units[i] = (clopts_platform_u16_t)0xFFFF;
    }
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    first = env->wide;
    probe.calls = 0;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 1), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 32), 0);
    NAOS_CHECK_PTR_EQ(env->wide, first);
    NAOS_CHECK_UINT_GE(env->wide_cap, 32);
    NAOS_CHECK_UINT_EQ(probe.calls, 0);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 2048), 0);
    PLATFORM_ENV_REQUIRE(env->wide);
    NAOS_CHECK_UINT_GE(env->wide_cap, 2048);
    units = (clopts_platform_u16_t *)env->wide;
    for (i = 0; i < 2048; ++i)
    {
        units[i] = (clopts_platform_u16_t)i;
    }
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(env_reserve_wide_fails_on_size_overflow_and_keeps_the_old_buffer)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;
    void *first;
    size_t cap;
    size_t calls;

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);

    /* SIZE_MAX units do not fit in size_t bytes */
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, SIZE_MAX), -1);
    NAOS_CHECK_NULL(env->wide);
    NAOS_CHECK_UINT_EQ(env->wide_cap, 0);
    NAOS_CHECK_UINT_EQ(probe.live, 1);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 4), 0);
    first = env->wide;
    cap = env->wide_cap;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, SIZE_MAX / 2 + 1), -1);
    NAOS_CHECK_PTR_EQ(env->wide, first);
    NAOS_CHECK_UINT_EQ(env->wide_cap, cap);
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    /* an allocation failure keeps it too */
    calls = probe.calls;
    probe.fail_at = calls + 1;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 64), -1);
    NAOS_CHECK_PTR_EQ(env->wide, first);
    NAOS_CHECK_UINT_EQ(env->wide_cap, cap);
    NAOS_CHECK_UINT_EQ(probe.live, 2);

    probe.fail_at = 0;
    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(env_free_releases_name_and_both_buffers)
{
    clopts_platform_probe_t probe = {0, 0, 0};
    clopts_env_t *env;

    env = clopts_platform_env_new(&probe);
    PLATFORM_ENV_REQUIRE(env);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 10), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 10), 0);
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_UTF16, "CLOPTS_NAME");
    NAOS_CHECK_UINT_EQ(probe.live, 4);
    clopts_env_free(env);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/* Every fault point of a create / reserve / fail sequence leaves live == 0 after the release. */
NAOS_TEST(env_every_fault_point_leaves_no_live_block)
{
    size_t fail_at;

    for (fail_at = 1; fail_at <= 5; ++fail_at)
    {
        clopts_platform_probe_t probe = {0, 0, 0};
        clopts_env_t *env;

        probe.fail_at = fail_at;
        env = clopts_platform_env_new(&probe);
        if (env == NULL)
        {
            NAOS_CHECK_UINT_EQ(fail_at, 1);
            NAOS_CHECK_UINT_EQ(probe.live, 0);
            continue;
        }
        (void)clopts_platform_env_reserve_value(env, 8);
        (void)clopts_platform_env_reserve_wide(env, 8);
        clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_NAME, "N");
        if (clopts_env_error_name(env) == NULL)
        {
            NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
        }
        else
        {
            NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
        }
        clopts_env_free(env);
        NAOS_CHECK_UINT_EQ(probe.live, 0);
    }
}

NAOS_SUITE(env,
    NAOS_ADD_TEST(u16_from_units_copies_any_unit_and_terminates),
    NAOS_ADD_TEST(u16_from_units_accepts_an_empty_sequence),
    NAOS_ADD_TEST(u16_eq_ascii_does_not_match_non_ascii_units),
    NAOS_ADD_TEST(env_new_starts_without_failure_and_free_releases_it),
    NAOS_ADD_TEST(public_env_new_has_no_probe),
    NAOS_ADD_TEST(env_free_ignores_null),
    NAOS_ADD_TEST(env_accessors_of_null_read_as_out_of_memory),
    NAOS_ADD_TEST(env_new_returns_null_when_the_allocation_fails),
    NAOS_ADD_TEST(env_fail_records_kind_and_a_copy_of_name_and_clear_drops_it),
    NAOS_ADD_TEST(env_fail_records_out_of_memory_without_name_when_the_copy_fails),
    NAOS_ADD_TEST(env_reserve_value_grows_and_never_shrinks),
    NAOS_ADD_TEST(env_reserve_value_keeps_the_old_buffer_when_the_allocation_fails),
    NAOS_ADD_TEST(env_reserve_wide_grows_and_never_shrinks),
    NAOS_ADD_TEST(env_reserve_wide_fails_on_size_overflow_and_keeps_the_old_buffer),
    NAOS_ADD_TEST(env_free_releases_name_and_both_buffers),
    NAOS_ADD_TEST(env_every_fault_point_leaves_no_live_block)
)
