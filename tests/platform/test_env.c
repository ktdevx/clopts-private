/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite env: environment lookup contexts (creation, release, the failure
 * record and the buffers) and clopts_env_lookup on every OS (getenv outside
 * Windows, the UTF-16 environment and UTF-8 conversion on Windows), and
 * clopts_parse_with_env() with clopts_env_lookup and a context: the value of
 * an environment fallback and a failed lookup (OUT_OF_MEMORY, the reason
 * kept in the context). The allocations are counted (and made to fail) with
 * the test allocator (tests/support/test_alloc.h).
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

#include "test_alloc.h"

#include "clopts/decl.h"
#include "clopts/parse.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   /* SetEnvironmentVariableW */
#endif

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
    clopts_env_t *env;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    NAOS_CHECK_NULL(env->value);
    NAOS_CHECK_UINT_EQ(env->value_cap, 0);
    NAOS_CHECK_NULL(env->wide);
    NAOS_CHECK_UINT_EQ(env->wide_cap, 0);
    /* only the context is allocated; the buffers come with the first lookup that needs them */
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_free_ignores_null)
{
    test_alloc_begin(0);
    clopts_env_destroy(NULL);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_accessors_of_null_read_as_out_of_memory)
{
    NAOS_CHECK_INT_EQ(clopts_env_get_error(NULL), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_env_get_error_name(NULL));
}

NAOS_TEST(env_new_returns_null_when_the_allocation_fails)
{
    test_alloc_begin(1);
    NAOS_CHECK_NULL(clopts_env_create());
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_fail_records_kind_and_a_copy_of_name_and_clear_drops_it)
{
    clopts_env_t *env;
    char name[] = "CLOPTS_NAME";

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);

    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_UTF16, name);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_UTF16);
    PLATFORM_ENV_REQUIRE(clopts_env_get_error_name(env));
    NAOS_CHECK_TRUE(clopts_env_get_error_name(env) != name);
    NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "CLOPTS_NAME");
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    /* the record is a copy: changing the caller's string does not change it */
    name[0] = 'X';
    NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "CLOPTS_NAME");

    /* a second failure replaces the first without leaking it */
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_NAME, "");
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
    NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "");
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    clopts_platform_env_clear(env);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    /* clearing a clear record changes nothing */
    clopts_platform_env_clear(env);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));

    /* free releases a recorded name */
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_OUT_OF_MEMORY, "N");
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_fail_records_out_of_memory_without_name_when_the_copy_fails)
{
    clopts_env_t *env;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_UTF16, "OLD");
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    test_alloc_set_fail_at(test_alloc.calls + 1);
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_NAME, "CLOPTS_NAME");
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    /* the earlier name is released, not leaked */
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    test_alloc_set_fail_at(0);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_reserve_value_grows_and_never_shrinks)
{
    clopts_env_t *env;
    char *first;
    size_t calls;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 0), 0);
    NAOS_CHECK_NOT_NULL(env->value);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 16), 0);
    PLATFORM_ENV_REQUIRE(env->value);
    NAOS_CHECK_UINT_GE(env->value_cap, 16);
    memset(env->value, 'v', 16);
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    /* a smaller or equal request keeps the buffer and allocates nothing */
    first = env->value;
    calls = test_alloc.calls;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 4), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 16), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 0), 0);
    NAOS_CHECK_PTR_EQ(env->value, first);
    NAOS_CHECK_UINT_GE(env->value_cap, 16);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 4096), 0);
    PLATFORM_ENV_REQUIRE(env->value);
    NAOS_CHECK_UINT_GE(env->value_cap, 4096);
    memset(env->value, 'w', 4096);
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_reserve_value_keeps_the_old_buffer_when_the_allocation_fails)
{
    clopts_env_t *env;
    char *first;
    size_t cap;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);

    /* no buffer yet */
    test_alloc_set_fail_at(test_alloc.calls + 1);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 8), -1);
    NAOS_CHECK_NULL(env->value);
    NAOS_CHECK_UINT_EQ(env->value_cap, 0);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    test_alloc_set_fail_at(0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 8), 0);
    first = env->value;
    cap = env->value_cap;
    test_alloc_set_fail_at(test_alloc.calls + 1);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 64), -1);
    NAOS_CHECK_PTR_EQ(env->value, first);
    NAOS_CHECK_UINT_EQ(env->value_cap, cap);
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    test_alloc_set_fail_at(0);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_reserve_wide_grows_and_never_shrinks)
{
    clopts_env_t *env;
    clopts_platform_u16_t *units;
    void *first;
    size_t calls;
    size_t i;

    test_alloc_begin(0);
    env = clopts_env_create();
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
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    first = env->wide;
    calls = test_alloc.calls;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 1), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 32), 0);
    NAOS_CHECK_PTR_EQ(env->wide, first);
    NAOS_CHECK_UINT_GE(env->wide_cap, 32);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 2048), 0);
    PLATFORM_ENV_REQUIRE(env->wide);
    NAOS_CHECK_UINT_GE(env->wide_cap, 2048);
    units = (clopts_platform_u16_t *)env->wide;
    for (i = 0; i < 2048; ++i)
    {
        units[i] = (clopts_platform_u16_t)i;
    }
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_reserve_wide_fails_on_size_overflow_and_keeps_the_old_buffer)
{
    clopts_env_t *env;
    void *first;
    size_t cap;
    size_t calls;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);

    /* SIZE_MAX units do not fit in size_t bytes; nothing reaches the allocator */
    calls = test_alloc.calls;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, SIZE_MAX), -1);
    NAOS_CHECK_NULL(env->wide);
    NAOS_CHECK_UINT_EQ(env->wide_cap, 0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 4), 0);
    first = env->wide;
    cap = env->wide_cap;
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, SIZE_MAX / 2 + 1), -1);
    NAOS_CHECK_PTR_EQ(env->wide, first);
    NAOS_CHECK_UINT_EQ(env->wide_cap, cap);
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    /* an allocation failure keeps it too */
    test_alloc_set_fail_at(test_alloc.calls + 1);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 64), -1);
    NAOS_CHECK_PTR_EQ(env->wide, first);
    NAOS_CHECK_UINT_EQ(env->wide_cap, cap);
    NAOS_CHECK_INT_EQ(test_alloc.live, 2);

    test_alloc_set_fail_at(0);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_free_releases_name_and_both_buffers)
{
    clopts_env_t *env;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_value(env, 10), 0);
    NAOS_CHECK_INT_EQ(clopts_platform_env_reserve_wide(env, 10), 0);
    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_UTF16, "CLOPTS_NAME");
    NAOS_CHECK_INT_EQ(test_alloc.live, 4);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Every fault point of a create / reserve / fail sequence leaves no block
 * alive after the release. The sequence has four fault points: the context,
 * the value buffer, the work buffer and the copy of the name.
 */
NAOS_TEST(env_every_fault_point_leaves_no_live_block)
{
    size_t fail_at;
    size_t oom_runs = 0;
    int succeeded = 0;

    for (fail_at = 1; (fail_at <= 16) && (!succeeded); ++fail_at)
    {
        clopts_env_t *env;
        int value_rc;
        int wide_rc;

        NAOS_NOTE("env fault point %u", (unsigned)fail_at);
        test_alloc_begin(fail_at);
        env = clopts_env_create();
        if (env == NULL)
        {
            oom_runs++;
            NAOS_CHECK_UINT_EQ(fail_at, 1);
            NAOS_CHECK_TRUE(test_alloc_end());
            continue;
        }
        value_rc = clopts_platform_env_reserve_value(env, 8);
        wide_rc = clopts_platform_env_reserve_wide(env, 8);
        clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_NAME, "N");
        if (clopts_env_get_error_name(env) == NULL)
        {
            NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
        }
        else
        {
            NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
        }
        if (test_alloc.calls < fail_at)
        {
            succeeded = 1;
            NAOS_CHECK_INT_EQ(value_rc, 0);
            NAOS_CHECK_INT_EQ(wide_rc, 0);
            NAOS_CHECK_NOT_NULL(clopts_env_get_error_name(env));
        }
        else
        {
            oom_runs++;
        }
        clopts_env_destroy(env);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_EQ(oom_runs, 4);
}

/* ---- the parser with clopts_env_lookup, on every OS ---------------------- */

/*
 * Sets name to the UTF-8 value (NULL deletes it) in the process environment:
 * setenv() outside Windows, the UTF-16 environment on Windows. 0 on success.
 */
static int platform_env_put(const char *name, const char *value)
{
#ifdef _WIN32
    wchar_t wname[256];
    wchar_t wvalue[1024];

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wname, 256) <= 0)
    {
        return -1;
    }
    if (value == NULL)
    {
        return (SetEnvironmentVariableW(wname, NULL) != 0) ? 0 : -1;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, wvalue, 1024) <= 0)
    {
        return -1;
    }
    return (SetEnvironmentVariableW(wname, wvalue) != 0) ? 0 : -1;
#else
    return (value == NULL) ? unsetenv(name) : setenv(name, value, 1);
#endif
}

/* The fictional program tool with one VALUE option --output whose fallback is env_name (NULL handles on failure). */
typedef struct platform_env_tool
{
    clopts_program_t *program;
    clopts_option_t *output;
} platform_env_tool_t;

static void platform_env_tool_init(platform_env_tool_t *tool, const char *env_name)
{
    clopts_option_t *output = NULL;

    tool->program = NULL;
    tool->output = NULL;
    if (clopts_program_create("tool", &tool->program) != CLOPTS_DECL_ERROR_NONE)
    {
        return;
    }
    if ((clopts_option_create(CLOPTS_OPTION_VALUE, "output", 'o', &output) != CLOPTS_DECL_ERROR_NONE)
        || (clopts_option_set_env(output, env_name) != CLOPTS_DECL_ERROR_NONE)
        || (clopts_program_add_option(tool->program, output) != CLOPTS_DECL_ERROR_NONE))
    {
        clopts_option_destroy(output);
        output = NULL;
    }
    tool->output = output;
}

/* "tool" with nothing else: --output can only come from the environment. */
static clopts_result_t *platform_env_tool_parse(platform_env_tool_t *tool, void *env_ctx)
{
    static char arg0[] = "tool";
    static char *argv[] = {arg0, NULL};

    return clopts_parse_with_env(tool->program, 1, argv, clopts_env_lookup, env_ctx);
}

NAOS_TEST(parse_with_env_takes_a_utf8_value_from_the_environment)
{
    /* "out テスト.txt" */
    static const char expected[] = "out \xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt";
    platform_env_tool_t tool;
    clopts_env_t *env;
    clopts_result_t *result;

    NAOS_REQUIRE_INT_EQ(platform_env_put("CLOPTS_TEST_TOOL_OUTPUT", expected), 0);
    test_alloc_begin(0);
    platform_env_tool_init(&tool, "CLOPTS_TEST_TOOL_OUTPUT");
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_REQUIRE_NOT_NULL(tool.output);
    env = clopts_env_create();
    NAOS_REQUIRE_NOT_NULL(env);

    result = platform_env_tool_parse(&tool, env);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, tool.output), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_NOT_NULL(clopts_result_get_value(result, tool.output));
    if (clopts_result_get_value(result, tool.output) != NULL)
    {
        NAOS_CHECK_STR_EQ(clopts_result_get_value(result, tool.output), expected);
    }
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);

    /* the parser copied the value: it outlives the context */
    clopts_env_destroy(env);
    if (clopts_result_get_value(result, tool.output) != NULL)
    {
        NAOS_CHECK_STR_EQ(clopts_result_get_value(result, tool.output), expected);
    }

    clopts_result_destroy(result);
    clopts_program_destroy(tool.program);
    NAOS_CHECK_TRUE(test_alloc_end());
    platform_env_put("CLOPTS_TEST_TOOL_OUTPUT", NULL);
}

NAOS_TEST(parse_with_env_leaves_an_unset_variable_unset)
{
    platform_env_tool_t tool;
    clopts_env_t *env;
    clopts_result_t *result;

    NAOS_REQUIRE_INT_EQ(platform_env_put("CLOPTS_TEST_TOOL_UNSET", "x"), 0);
    NAOS_REQUIRE_INT_EQ(platform_env_put("CLOPTS_TEST_TOOL_UNSET", NULL), 0);
    test_alloc_begin(0);
    platform_env_tool_init(&tool, "CLOPTS_TEST_TOOL_UNSET");
    NAOS_REQUIRE_NOT_NULL(tool.program);
    env = clopts_env_create();
    NAOS_REQUIRE_NOT_NULL(env);

    result = platform_env_tool_parse(&tool, env);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, tool.output), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(clopts_result_get_value(result, tool.output));
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);

    clopts_result_destroy(result);
    clopts_env_destroy(env);
    clopts_program_destroy(tool.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* A NULL context makes the lookup fail, so the parse ends with OUT_OF_MEMORY. */
NAOS_TEST(parse_with_env_null_context_ends_with_out_of_memory)
{
    platform_env_tool_t tool;
    clopts_result_t *result;

    NAOS_REQUIRE_INT_EQ(platform_env_put("CLOPTS_TEST_TOOL_OUTPUT", "v"), 0);
    test_alloc_begin(0);
    platform_env_tool_init(&tool, "CLOPTS_TEST_TOOL_OUTPUT");
    NAOS_REQUIRE_NOT_NULL(tool.program);

    result = platform_env_tool_parse(&tool, NULL);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_get_value(result, tool.output));

    clopts_result_destroy(result);
    clopts_program_destroy(tool.program);
    NAOS_CHECK_TRUE(test_alloc_end());
    platform_env_put("CLOPTS_TEST_TOOL_OUTPUT", NULL);
}

#ifndef _WIN32
/* ---- outside Windows: clopts_env_lookup reads getenv() ------------------- */

/* Sets name to value in the process environment; ends the test when that fails. */
#define PLATFORM_ENV_SET(name, value) \
    do \
    { \
        int platform_env_rc_ = setenv((name), (value), 1); \
        NAOS_CHECK_INT_EQ(platform_env_rc_, 0); \
        if (platform_env_rc_ != 0) \
        { \
            return; \
        } \
    } while (0)

NAOS_TEST(env_lookup_finds_a_set_variable_and_borrows_getenvs_pointer)
{
    clopts_env_t *env;
    const char *value = NULL;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET("CLOPTS_TEST_FOUND", "value \xe3\x83\x86");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_STR_EQ(value, "value \xe3\x83\x86");
    /* not copied or converted: the very pointer getenv returns */
    NAOS_CHECK_PTR_EQ(value, getenv("CLOPTS_TEST_FOUND"));
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    /* nothing is allocated in the context: the only allocation is the context itself */
    NAOS_CHECK_NULL(env->value);
    NAOS_CHECK_NULL(env->wide);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
    unsetenv("CLOPTS_TEST_FOUND");
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_lookup_reports_an_unset_variable)
{
    clopts_env_t *env;
    const char *value = NULL;

    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET("CLOPTS_TEST_UNSET", "x");
    NAOS_CHECK_INT_EQ(unsetenv("CLOPTS_TEST_UNSET"), 0);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_UNSET", &value), CLOPTS_ENV_UNSET);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    clopts_env_destroy(env);
}

NAOS_TEST(env_lookup_finds_an_empty_value)
{
    clopts_env_t *env;
    const char *value = NULL;

    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET("CLOPTS_TEST_EMPTY", "");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_EMPTY", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_STR_EQ(value, "");
    NAOS_CHECK_PTR_EQ(value, getenv("CLOPTS_TEST_EMPTY"));
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    unsetenv("CLOPTS_TEST_EMPTY");
    clopts_env_destroy(env);
}

/* A successful lookup clears a failure recorded earlier in the same context. */
NAOS_TEST(env_lookup_clears_an_earlier_failure)
{
    clopts_env_t *env;
    const char *value = NULL;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);

    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_INVALID_UTF16, "CLOPTS_TEST_OLD");
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_UTF16);
    PLATFORM_ENV_SET("CLOPTS_TEST_FOUND", "v");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_FOUND);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    /* the recorded name is released, not leaked */
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    clopts_platform_env_fail(env, CLOPTS_ENV_ERROR_OUT_OF_MEMORY, "CLOPTS_TEST_OLD");
    NAOS_CHECK_INT_EQ(unsetenv("CLOPTS_TEST_FOUND"), 0);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_UNSET);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_lookup_with_null_ctx_fails)
{
    const char *value = NULL;

    PLATFORM_ENV_SET("CLOPTS_TEST_FOUND", "v");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(NULL, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_NULL(value);
    /* name and value are not read either */
    NAOS_CHECK_INT_EQ(clopts_env_lookup(NULL, NULL, NULL), CLOPTS_ENV_FAILED);
    unsetenv("CLOPTS_TEST_FOUND");
}
#else /* _WIN32 */
/* ---- Windows: clopts_env_lookup reads the UTF-16 environment ------------- */

/* Sets name to value (NULL deletes it) in the process environment; ends the test when that fails. */
#define PLATFORM_ENV_SET_W(name, value) \
    do \
    { \
        BOOL platform_env_ok_ = SetEnvironmentVariableW((name), (value)); \
        NAOS_CHECK_TRUE(platform_env_ok_ != 0); \
        if (platform_env_ok_ == 0) \
        { \
            return; \
        } \
    } while (0)

NAOS_TEST(env_lookup_finds_a_set_variable_in_the_context_buffer)
{
    clopts_env_t *env;
    const char *value = NULL;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_FOUND", L"value");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_STR_EQ(value, "value");
    /* converted into the context's value buffer */
    NAOS_CHECK_PTR_EQ(value, env->value);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    SetEnvironmentVariableW(L"CLOPTS_TEST_FOUND", NULL);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_lookup_reports_an_unset_variable)
{
    clopts_env_t *env;
    const char *value = NULL;

    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_UNSET", L"x");
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_UNSET", NULL);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_UNSET", &value), CLOPTS_ENV_UNSET);
    NAOS_CHECK_NULL(value);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    clopts_env_destroy(env);
}

NAOS_TEST(env_lookup_finds_an_empty_value)
{
    clopts_env_t *env;
    const char *value = NULL;

    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    /* first a non-empty value, so that the buffer holds stale bytes */
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_EMPTY", L"stale");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_EMPTY", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_EMPTY", L"");
    value = NULL;
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_EMPTY", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_STR_EQ(value, "");
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));
    SetEnvironmentVariableW(L"CLOPTS_TEST_EMPTY", NULL);
    clopts_env_destroy(env);
}

NAOS_TEST(env_lookup_converts_a_japanese_value_to_utf8)
{
    /* "日本語" and U+1F600 (a surrogate pair) */
    static const wchar_t wide[] = {0x65E5, 0x672C, 0x8A9E, 0xD83D, 0xDE00, 0};
    static const char utf8[] = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xf0\x9f\x98\x80";
    clopts_env_t *env;
    const char *value = NULL;

    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_JA", wide);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_JA", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_UINT_EQ(strlen(value), sizeof(utf8) - 1);
    NAOS_CHECK_INT_EQ(memcmp(value, utf8, sizeof(utf8)), 0);
    SetEnvironmentVariableW(L"CLOPTS_TEST_JA", NULL);
    clopts_env_destroy(env);
}

/* A non-ASCII UTF-8 name is converted to UTF-16 before the lookup. */
NAOS_TEST(env_lookup_converts_a_non_ascii_name)
{
    static const wchar_t wname[] = {L'C', L'L', L'O', L'P', L'T', L'S', L'_', L'T', L'E', L'S', L'T', L'_',
                                    0x540D, 0};
    clopts_env_t *env;
    const char *value = NULL;

    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET_W(wname, L"named");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_\xe5\x90\x8d", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_STR_EQ(value, "named");
    SetEnvironmentVariableW(wname, NULL);
    clopts_env_destroy(env);
}

/*
 * A value of several thousand code units, read after a short one in the
 * same context, does not fit the buffers sized by the first lookup: they
 * grow and the read is retried, with the name converted again.
 */
NAOS_TEST(env_lookup_grows_the_buffers_for_a_long_value)
{
    enum { LONG_UNITS = 6000 };
    clopts_env_t *env;
    const char *value = NULL;
    wchar_t *wide;
    size_t wide_cap;
    size_t calls;
    size_t i;
    size_t mismatch;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_LONG", L"short");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_LONG", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_STR_EQ(value, "short");
    wide_cap = env->wide_cap;
    NAOS_CHECK_UINT_LT(wide_cap, (size_t)LONG_UNITS);

    /* the even units are ASCII, the odd ones U+3042 (3 UTF-8 bytes) */
    wide = (wchar_t *)malloc((LONG_UNITS + 1) * sizeof(wchar_t));
    PLATFORM_ENV_REQUIRE(wide);
    for (i = 0; i < LONG_UNITS; ++i)
    {
        wide[i] = ((i % 2) == 0) ? (wchar_t)(L'a' + (i / 2) % 26) : (wchar_t)0x3042;
    }
    wide[LONG_UNITS] = 0;
    if (SetEnvironmentVariableW(L"CLOPTS_TEST_LONG", wide) == 0)
    {
        NAOS_CHECK_TRUE(0);
        free(wide);
        clopts_env_destroy(env);
        return;
    }
    free(wide);

    value = NULL;
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_LONG", &value), CLOPTS_ENV_FOUND);
    PLATFORM_ENV_REQUIRE(value);
    NAOS_CHECK_UINT_GT(env->wide_cap, wide_cap);
    NAOS_CHECK_UINT_EQ(strlen(value), (LONG_UNITS / 2) * 4);
    mismatch = (size_t)-1;
    for (i = 0; (i < LONG_UNITS / 2) && (mismatch == (size_t)-1); ++i)
    {
        const char *p = value + i * 4;

        if ((p[0] != (char)('a' + i % 26)) || (memcmp(p + 1, "\xe3\x81\x82", 3) != 0))
        {
            mismatch = i;
        }
    }
    NAOS_CHECK_UINT_EQ(mismatch, (size_t)-1);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);

    /* a short value again reuses the grown buffers */
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_LONG", L"x");
    calls = test_alloc.calls;
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_LONG", &value), CLOPTS_ENV_FOUND);
    NAOS_CHECK_STR_EQ(value, "x");
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);

    SetEnvironmentVariableW(L"CLOPTS_TEST_LONG", NULL);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_lookup_reports_invalid_utf16_with_the_name)
{
    /* a lone low surrogate, then a lone high surrogate at the end */
    static const wchar_t bad_low[] = {L'a', 0xDC00, L'b', 0};
    static const wchar_t bad_high[] = {L'a', 0xD800, 0};
    clopts_env_t *env;
    const char *value = NULL;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_BAD", bad_low);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_BAD", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_NULL(value);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_UTF16);
    PLATFORM_ENV_REQUIRE(clopts_env_get_error_name(env));
    NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "CLOPTS_TEST_BAD");

    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_BAD", bad_high);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_BAD", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_UTF16);
    PLATFORM_ENV_REQUIRE(clopts_env_get_error_name(env));
    NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "CLOPTS_TEST_BAD");

    SetEnvironmentVariableW(L"CLOPTS_TEST_BAD", NULL);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_lookup_reports_an_invalid_utf8_name)
{
    clopts_env_t *env;
    const char *value = NULL;

    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "\xff", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_NULL(value);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
    PLATFORM_ENV_REQUIRE(clopts_env_get_error_name(env));
    NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "\xff");
    /* a truncated sequence after valid bytes too */
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_\xe3\x81", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
    PLATFORM_ENV_REQUIRE(clopts_env_get_error_name(env));
    NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "CLOPTS_TEST_\xe3\x81");
    clopts_env_destroy(env);
}

/* A successful lookup clears a failure recorded by an earlier lookup in the same context. */
NAOS_TEST(env_lookup_clears_an_earlier_failure)
{
    static const wchar_t bad[] = {0xDFFF, 0};
    clopts_env_t *env;
    const char *value = NULL;

    test_alloc_begin(0);
    env = clopts_env_create();
    PLATFORM_ENV_REQUIRE(env);

    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_BAD", bad);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_BAD", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_UTF16);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_FOUND", L"v");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_FOUND);
    NAOS_CHECK_STR_EQ(value, "v");
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));

    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "\xff", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_FOUND", NULL);
    NAOS_CHECK_INT_EQ(clopts_env_lookup(env, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_UNSET);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_NONE);
    NAOS_CHECK_NULL(clopts_env_get_error_name(env));

    SetEnvironmentVariableW(L"CLOPTS_TEST_BAD", NULL);
    clopts_env_destroy(env);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(env_lookup_with_null_ctx_fails)
{
    const char *value = NULL;

    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_FOUND", L"v");
    NAOS_CHECK_INT_EQ(clopts_env_lookup(NULL, "CLOPTS_TEST_FOUND", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_NULL(value);
    /* name and value are not read either */
    NAOS_CHECK_INT_EQ(clopts_env_lookup(NULL, NULL, NULL), CLOPTS_ENV_FAILED);
    SetEnvironmentVariableW(L"CLOPTS_TEST_FOUND", NULL);
}

/*
 * Sanity check of the allocation failures (the exhaustive sweep is task
 * 4.1): every failing allocation of a lookup in a new context gives FAILED
 * with OUT_OF_MEMORY and leaks nothing. The lookup has two fault points: the
 * work buffer (the name and 256 code units, which the value fits) and the
 * value buffer.
 */
NAOS_TEST(env_lookup_reports_out_of_memory_when_an_allocation_fails)
{
    clopts_env_t *env;
    const char *value;
    size_t offset;
    size_t start;
    size_t oom_runs = 0;
    int succeeded = 0;
    clopts_env_status_t status;

    PLATFORM_ENV_SET_W(L"CLOPTS_TEST_FOUND", L"\x3042\x3044\x3046");
    for (offset = 1; (offset <= 16) && (!succeeded); ++offset)
    {
        NAOS_NOTE("env lookup fault point %u", (unsigned)offset);
        /* The context is created without failures; the sweep starts after it. */
        test_alloc_begin(0);
        env = clopts_env_create();
        PLATFORM_ENV_REQUIRE(env);
        start = test_alloc.calls;
        test_alloc_set_fail_at(start + offset);
        value = NULL;
        status = clopts_env_lookup(env, "CLOPTS_TEST_FOUND", &value);
        if (status == CLOPTS_ENV_FAILED)
        {
            oom_runs++;
            NAOS_CHECK_UINT_GE(test_alloc.calls, start + offset);
            NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_NULL(clopts_env_get_error_name(env));
            NAOS_CHECK_NULL(value);
        }
        else
        {
            NAOS_CHECK_INT_EQ(status, CLOPTS_ENV_FOUND);
            NAOS_CHECK_STR_EQ(value, "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86");
        }
        if (test_alloc.calls < start + offset)
        {
            succeeded = 1;
            NAOS_CHECK_INT_EQ(status, CLOPTS_ENV_FOUND);
        }
        test_alloc_set_fail_at(0);
        clopts_env_destroy(env);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_EQ(oom_runs, 2);
    SetEnvironmentVariableW(L"CLOPTS_TEST_FOUND", NULL);
}

/*
 * The option's variable name is not valid UTF-8, so the lookup fails: the
 * parse ends with OUT_OF_MEMORY, and the context keeps the real reason.
 */
NAOS_TEST(parse_with_env_invalid_name_ends_with_out_of_memory_and_keeps_the_reason)
{
    platform_env_tool_t tool;
    clopts_env_t *env;
    clopts_result_t *result;

    test_alloc_begin(0);
    platform_env_tool_init(&tool, "\xff");
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_REQUIRE_NOT_NULL(tool.output);
    env = clopts_env_create();
    NAOS_REQUIRE_NOT_NULL(env);

    result = platform_env_tool_parse(&tool, env);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_env_get_error(env), CLOPTS_ENV_ERROR_INVALID_NAME);
    NAOS_CHECK_NOT_NULL(clopts_env_get_error_name(env));
    if (clopts_env_get_error_name(env) != NULL)
    {
        NAOS_CHECK_STR_EQ(clopts_env_get_error_name(env), "\xff");
    }

    clopts_result_destroy(result);
    clopts_env_destroy(env);
    clopts_program_destroy(tool.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}
#endif /* _WIN32 */

/* ---- default lookup of the parser ------------------------------------------ */

NAOS_TEST(getenv_default_lends_the_getenv_pointer)
{
    const char *value = NULL;
    const char *expected = getenv("PATH");

    if (expected != NULL)
    {
        NAOS_CHECK_INT_EQ(clopts_platform_getenv_default(NULL, "PATH", &value), CLOPTS_ENV_FOUND);
        NAOS_CHECK_PTR_EQ(value, expected);
    }
    else
    {
        NAOS_CHECK_INT_EQ(clopts_platform_getenv_default(NULL, "PATH", &value), CLOPTS_ENV_UNSET);
    }
    NAOS_CHECK_INT_EQ(clopts_platform_getenv_default(NULL, "CLOPTS_TEST_SURELY_UNSET_VARIABLE_6_3", &value),
                      CLOPTS_ENV_UNSET);
}

/* Preprocessor directives cannot appear inside the macro arguments, so each OS has its own list. */
#ifndef _WIN32
NAOS_SUITE(env,
    NAOS_ADD_TEST(u16_from_units_copies_any_unit_and_terminates),
    NAOS_ADD_TEST(u16_from_units_accepts_an_empty_sequence),
    NAOS_ADD_TEST(u16_eq_ascii_does_not_match_non_ascii_units),
    NAOS_ADD_TEST(env_new_starts_without_failure_and_free_releases_it),
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
    NAOS_ADD_TEST(env_every_fault_point_leaves_no_live_block),
    NAOS_ADD_TEST(parse_with_env_takes_a_utf8_value_from_the_environment),
    NAOS_ADD_TEST(parse_with_env_leaves_an_unset_variable_unset),
    NAOS_ADD_TEST(parse_with_env_null_context_ends_with_out_of_memory),
    NAOS_ADD_TEST(env_lookup_finds_a_set_variable_and_borrows_getenvs_pointer),
    NAOS_ADD_TEST(env_lookup_reports_an_unset_variable),
    NAOS_ADD_TEST(env_lookup_finds_an_empty_value),
    NAOS_ADD_TEST(env_lookup_clears_an_earlier_failure),
    NAOS_ADD_TEST(env_lookup_with_null_ctx_fails),
    NAOS_ADD_TEST(getenv_default_lends_the_getenv_pointer)
)
#else
NAOS_SUITE(env,
    NAOS_ADD_TEST(u16_from_units_copies_any_unit_and_terminates),
    NAOS_ADD_TEST(u16_from_units_accepts_an_empty_sequence),
    NAOS_ADD_TEST(u16_eq_ascii_does_not_match_non_ascii_units),
    NAOS_ADD_TEST(env_new_starts_without_failure_and_free_releases_it),
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
    NAOS_ADD_TEST(env_every_fault_point_leaves_no_live_block),
    NAOS_ADD_TEST(parse_with_env_takes_a_utf8_value_from_the_environment),
    NAOS_ADD_TEST(parse_with_env_leaves_an_unset_variable_unset),
    NAOS_ADD_TEST(parse_with_env_null_context_ends_with_out_of_memory),
    NAOS_ADD_TEST(env_lookup_finds_a_set_variable_in_the_context_buffer),
    NAOS_ADD_TEST(env_lookup_reports_an_unset_variable),
    NAOS_ADD_TEST(env_lookup_finds_an_empty_value),
    NAOS_ADD_TEST(env_lookup_converts_a_japanese_value_to_utf8),
    NAOS_ADD_TEST(env_lookup_converts_a_non_ascii_name),
    NAOS_ADD_TEST(env_lookup_grows_the_buffers_for_a_long_value),
    NAOS_ADD_TEST(env_lookup_reports_invalid_utf16_with_the_name),
    NAOS_ADD_TEST(env_lookup_reports_an_invalid_utf8_name),
    NAOS_ADD_TEST(env_lookup_clears_an_earlier_failure),
    NAOS_ADD_TEST(env_lookup_with_null_ctx_fails),
    NAOS_ADD_TEST(env_lookup_reports_out_of_memory_when_an_allocation_fails),
    NAOS_ADD_TEST(parse_with_env_invalid_name_ends_with_out_of_memory_and_keeps_the_reason),
    NAOS_ADD_TEST(getenv_default_lends_the_getenv_pointer)
)
#endif
