/*
 * Suite oom: sweeps that move the failing fault point of the test allocator
 * (tests/support/test_alloc.h) over handle creation, context creation, the
 * Windows conversions and environment lookups. Each run gives the sentinel,
 * NULL or a recorded OUT_OF_MEMORY when a fault point failed and the expected
 * outcome otherwise, and leaves no block alive after the release. The
 * allocation gateway itself is tested by core (tests/core/test_alloc.c).
 *
 * It also checks that the helpers of test_util hold the longest array they
 * accept, which the long inputs of the sweeps rely on.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include "test_alloc.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   /* SetEnvironmentVariableW */
#endif

NAOS_TEST(u16_helpers_fill_the_whole_capacity)
{
    static char text[PLATFORM_TEST_U16_CAP];
    static uint16_t units[PLATFORM_TEST_U16_CAP - 1];
    static platform_test_u16_t s;
    size_t i;

    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    platform_test_u16_from_ascii(&s, text);
    NAOS_CHECK_UINT_EQ(s.len, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len - 1], 'x');
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, text));

    for (i = 0; i < PLATFORM_TEST_U16_CAP - 1; ++i)
    {
        units[i] = (uint16_t)(i + 1);
    }
    platform_test_u16_from_units(&s, units, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.len, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len - 1], PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
}

/* ---- sweeps over the fault points ------------------------------------------ */

/*
 * Upper bound of the fault points a sweep moves over; every sweep ends well
 * before it (it only stops a sweep that would never end).
 */
#define OOM_SWEEP_LIMIT 64

/* Creates a handle; input is the sweep's own data. */
typedef clopts_argv_t *(*oom_argv_make_fn)(const void *input);

/* What the handle must hold once no fault point fails. */
typedef struct oom_argv_expect
{
    clopts_argv_status_t status;
    int count;
    int error_index;
    const char *const *values;   /* count UTF-8 strings; NULL when count is 0 */
} oom_argv_expect_t;

/* Counts of one sweep. */
typedef struct oom_sweep_stats
{
    size_t runs;
    size_t oom_runs;
    bool succeeded;
} oom_sweep_stats_t;

/* Checks that args holds exactly what expect describes. */
static void oom_check_argv(const clopts_argv_t *args, const oom_argv_expect_t *expect)
{
    char *const *values;
    int i;

    NAOS_CHECK_INT_EQ(clopts_argv_status(args), expect->status);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), expect->count);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), expect->error_index);
    values = clopts_argv_values(args);
    NAOS_CHECK_NOT_NULL(values);
    if (values == NULL)
    {
        return;
    }
    for (i = 0; (i < expect->count) && (i < clopts_argv_count(args)); ++i)
    {
        NAOS_CHECK_STR_EQ(values[i], expect->values[i]);
    }
    NAOS_CHECK_NULL(values[clopts_argv_count(args)]);
}

/*
 * Moves fail_at from 1 until a run passes fewer fault points than fail_at
 * (no fault point failed). Each run gives the sentinel (only when it reached
 * a failing fault point) or the expected handle, and no block is alive after
 * the release.
 */
static void oom_sweep_argv(oom_argv_make_fn make, const void *input, const oom_argv_expect_t *expect,
                           oom_sweep_stats_t *stats)
{
    size_t fail_at;

    memset(stats, 0, sizeof(*stats));
    for (fail_at = 1; (fail_at <= OOM_SWEEP_LIMIT) && (!stats->succeeded); ++fail_at)
    {
        clopts_argv_t *args;

        NAOS_NOTE("argv fault point %u", (unsigned)fail_at);
        test_alloc_begin(fail_at);
        args = make(input);
        stats->runs++;
        NAOS_CHECK_NOT_NULL(args);
        if (args == clopts_platform_argv_sentinel())
        {
            stats->oom_runs++;
            NAOS_CHECK_UINT_GE(test_alloc.calls, fail_at);
            NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OUT_OF_MEMORY);
            NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
            NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
        }
        else
        {
            oom_check_argv(args, expect);
        }
        if (test_alloc.calls < fail_at)
        {
            stats->succeeded = true;
            NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
        }
        clopts_argv_free(args);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
}

static clopts_argv_t *oom_make_passthrough(const void *input)
{
    return clopts_platform_argv_passthrough(3, (char *const *)input);
}

/* One fault point: the handle (platform_argv_new). */
NAOS_TEST(sweep_passthrough)
{
    static char arg0[] = "tool";
    static char arg1[] = "--output";
    static char arg2[] = "out \xe3\x83\x86";
    static char *argv[] = {arg0, arg1, arg2, NULL};
    static const char *const expected[] = {"tool", "--output", "out \xe3\x83\x86"};
    oom_argv_expect_t expect = {CLOPTS_ARGV_OK, 3, -1, expected};
    oom_sweep_stats_t stats;

    oom_sweep_argv(oom_make_passthrough, argv, &expect, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_EQ(stats.oom_runs, 1);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

/* Creating a context: one fault point (the context); NULL when it fails, never a leak. */
NAOS_TEST(sweep_env_new)
{
    size_t fail_at;
    size_t oom_runs = 0;
    bool succeeded = false;

    for (fail_at = 1; (fail_at <= OOM_SWEEP_LIMIT) && (!succeeded); ++fail_at)
    {
        clopts_env_t *env;

        NAOS_NOTE("env_new fault point %u", (unsigned)fail_at);
        test_alloc_begin(fail_at);
        env = clopts_env_new();
        if (env == NULL)
        {
            oom_runs++;
            NAOS_CHECK_UINT_GE(test_alloc.calls, fail_at);
            NAOS_CHECK_INT_EQ(test_alloc.live, 0);
        }
        else
        {
            NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_NONE);
            NAOS_CHECK_NULL(clopts_env_error_name(env));
        }
        if (test_alloc.calls < fail_at)
        {
            succeeded = true;
            NAOS_CHECK_NOT_NULL(env);
        }
        clopts_env_free(env);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_EQ(oom_runs, 1);
}

#ifdef _WIN32
static clopts_argv_t *oom_make_from_cmdline(const void *input)
{
    return clopts_platform_argv_from_cmdline((const wchar_t *)input);
}

static clopts_argv_t *oom_make_convert(const void *input)
{
    return clopts_platform_argv_convert(3, (const wchar_t *const *)input);
}

/* The command line: two fault points, the work block and then the handle block. */
NAOS_TEST(sweep_argv_from_cmdline)
{
    static const wchar_t cmdline[] = L"tool --name \"\x3042 \x3044\" \"\"";
    static const char *const expected[] = {"tool", "--name", "\xe3\x81\x82 \xe3\x81\x84", ""};
    oom_argv_expect_t expect = {CLOPTS_ARGV_OK, 4, -1, expected};
    oom_sweep_stats_t stats;

    oom_sweep_argv(oom_make_from_cmdline, cmdline, &expect, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_EQ(stats.oom_runs, 2);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

/*
 * A lone surrogate in the second element: two fault points, the work block
 * and then the failure handle.
 */
NAOS_TEST(sweep_argv_from_cmdline_invalid_utf16)
{
    static const wchar_t cmdline[] = {L't', L'o', L'o', L'l', L' ', L'a', 0xD800, L' ', L'b', 0};
    oom_argv_expect_t expect = {CLOPTS_ARGV_INVALID_UTF16, 0, 1, NULL};
    oom_sweep_stats_t stats;

    oom_sweep_argv(oom_make_from_cmdline, cmdline, &expect, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_EQ(stats.oom_runs, 2);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

/* One fault point: the block holding the handle, the pointers and the strings. */
NAOS_TEST(sweep_argv_convert)
{
    static const wchar_t w0[] = L"tool";
    static const wchar_t w1[] = L"";
    static const wchar_t w2[] = L"\x65e5\x672c";
    static const wchar_t *const wargs[] = {w0, w1, w2};
    static const char *const expected[] = {"tool", "", "\xe6\x97\xa5\xe6\x9c\xac"};
    oom_argv_expect_t expect = {CLOPTS_ARGV_OK, 3, -1, expected};
    oom_sweep_stats_t stats;

    oom_sweep_argv(oom_make_convert, wargs, &expect, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_EQ(stats.oom_runs, 1);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

/*
 * A lookup of a value several thousand code units long in a new context.
 * The fault points of the lookup: the first work buffer (the name and 256
 * code units), the grown work buffer for the retry (the value does not fit
 * the first one) and the value buffer. Every failing fault point gives
 * FAILED with OUT_OF_MEMORY (the name copy fails too, so no name).
 */
NAOS_TEST(sweep_env_lookup_long_value)
{
    enum { LONG_UNITS = 3000 };
    wchar_t *wide;
    size_t i;
    size_t offset;
    size_t oom_runs = 0;
    bool succeeded = false;

    wide = (wchar_t *)malloc((LONG_UNITS + 1) * sizeof(wchar_t));
    NAOS_CHECK_TRUE(wide != NULL);
    if (wide == NULL)
    {
        return;
    }
    for (i = 0; i < LONG_UNITS; ++i)
    {
        wide[i] = ((i % 2) == 0) ? (wchar_t)L'v' : (wchar_t)0x3042;
    }
    wide[LONG_UNITS] = 0;
    if (SetEnvironmentVariableW(L"CLOPTS_TEST_OOM_LONG", wide) == 0)
    {
        free(wide);
        NAOS_CHECK_TRUE(0);
        return;
    }
    free(wide);

    for (offset = 1; (offset <= OOM_SWEEP_LIMIT) && (!succeeded); ++offset)
    {
        clopts_env_t *env;
        const char *value = NULL;
        clopts_env_status_t status;
        size_t start;

        NAOS_NOTE("env lookup fault point %u", (unsigned)offset);
        /* The context is created without failures; the sweep starts after it. */
        test_alloc_begin(0);
        env = clopts_env_new();
        NAOS_CHECK_NOT_NULL(env);
        if (env == NULL)
        {
            break;
        }
        start = test_alloc.calls;
        test_alloc_set_fail_at(start + offset);
        status = clopts_env_lookup(env, "CLOPTS_TEST_OOM_LONG", &value);
        if (status == CLOPTS_ENV_FAILED)
        {
            oom_runs++;
            NAOS_CHECK_UINT_GE(test_alloc.calls, start + offset);
            NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_NULL(clopts_env_error_name(env));
        }
        else
        {
            NAOS_CHECK_INT_EQ(status, CLOPTS_ENV_FOUND);
            NAOS_CHECK_NOT_NULL(value);
            if (value != NULL)
            {
                NAOS_CHECK_UINT_EQ(strlen(value), (LONG_UNITS / 2) * 4);
                NAOS_CHECK_INT_EQ(memcmp(value, "v\xe3\x81\x82", 4), 0);
            }
            NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_NONE);
        }
        if (test_alloc.calls < start + offset)
        {
            succeeded = true;
            NAOS_CHECK_INT_EQ(status, CLOPTS_ENV_FOUND);
        }
        test_alloc_set_fail_at(0);
        clopts_env_free(env);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
    test_alloc_set_fail_at(0);
    SetEnvironmentVariableW(L"CLOPTS_TEST_OOM_LONG", NULL);
    NAOS_CHECK_TRUE(succeeded);
    /* the first work buffer, the grown one for the retry, and the value buffer */
    NAOS_CHECK_UINT_EQ(oom_runs, 3);
}
#endif /* _WIN32 */

/* Preprocessor directives cannot appear inside the macro arguments, so each OS has its own list. */
#ifndef _WIN32
NAOS_SUITE(oom,
    NAOS_ADD_TEST(u16_helpers_fill_the_whole_capacity),
    NAOS_ADD_TEST(sweep_passthrough),
    NAOS_ADD_TEST(sweep_env_new)
)
#else
NAOS_SUITE(oom,
    NAOS_ADD_TEST(u16_helpers_fill_the_whole_capacity),
    NAOS_ADD_TEST(sweep_passthrough),
    NAOS_ADD_TEST(sweep_env_new),
    NAOS_ADD_TEST(sweep_argv_from_cmdline),
    NAOS_ADD_TEST(sweep_argv_from_cmdline_invalid_utf16),
    NAOS_ADD_TEST(sweep_argv_convert),
    NAOS_ADD_TEST(sweep_env_lookup_long_value)
)
#endif
