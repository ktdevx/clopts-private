/*
 * Suite oom: the allocation gateway and the allocation probe, and sweeps
 * that move the failing fault point over handle creation, the Windows
 * conversions and environment lookups (the sentinel or a recorded
 * OUT_OF_MEMORY, and live == 0 after the release).
 *
 * It also checks that the helpers of test_util hold the longest array they
 * accept, which the long inputs of the sweeps rely on.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

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

/* Without a probe the gateway allocates and releases usable blocks. */
NAOS_TEST(no_probe_malloc_and_free)
{
    unsigned char *p;
    uint16_t *a;

    p = (unsigned char *)clopts_platform_malloc(NULL, 16);
    NAOS_CHECK_NOT_NULL(p);
    if (p != NULL)
    {
        memset(p, 0xab, 16);
        NAOS_CHECK_UINT_EQ(p[15], 0xab);
    }
    clopts_platform_free(NULL, p);

    a = (uint16_t *)clopts_platform_malloc_array(NULL, 8, sizeof(uint16_t));
    NAOS_CHECK_NOT_NULL(a);
    if (a != NULL)
    {
        a[7] = 0x1234;
        NAOS_CHECK_UINT_EQ(a[7], 0x1234);
    }
    clopts_platform_free(NULL, a);

    clopts_platform_free(NULL, NULL);
}

/* A size of 0 (or a count of 0) is a success, not NULL, with or without a probe. */
NAOS_TEST(zero_size_succeeds)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *p;
    void *q;
    void *r;

    p = clopts_platform_malloc(NULL, 0);
    NAOS_CHECK_NOT_NULL(p);
    clopts_platform_free(NULL, p);

    q = clopts_platform_malloc(&probe, 0);
    NAOS_CHECK_NOT_NULL(q);
    r = clopts_platform_malloc_array(&probe, 0, 4);
    NAOS_CHECK_NOT_NULL(r);
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    NAOS_CHECK_UINT_EQ(probe.live, 2);
    clopts_platform_free(&probe, q);
    clopts_platform_free(&probe, r);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/* fail_at == 0 never fails; calls counts every attempt and live every block. */
NAOS_TEST(probe_counts_calls_and_live)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *blocks[5];
    size_t i;

    for (i = 0; i < 5; ++i)
    {
        blocks[i] = (i % 2 == 0) ? clopts_platform_malloc(&probe, 8)
                                 : clopts_platform_malloc_array(&probe, 3, 4);
        NAOS_CHECK_NOT_NULL(blocks[i]);
        NAOS_CHECK_UINT_EQ(probe.calls, i + 1);
        NAOS_CHECK_UINT_EQ(probe.live, i + 1);
    }
    for (i = 0; i < 5; ++i)
    {
        clopts_platform_free(&probe, blocks[i]);
        NAOS_CHECK_UINT_EQ(probe.live, 4 - i);
    }
    NAOS_CHECK_UINT_EQ(probe.calls, 5);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/* The fail_at-th attempt and every later one fail; earlier ones succeed. */
NAOS_TEST(probe_fails_nth_and_later)
{
    size_t n;

    for (n = 1; n <= 4; ++n)
    {
        clopts_platform_probe_t probe = { 0, 0, 0 };
        void *blocks[6];
        size_t i;

        probe.fail_at = n;
        for (i = 0; i < 6; ++i)
        {
            blocks[i] = (i % 2 == 0) ? clopts_platform_malloc(&probe, 4)
                                     : clopts_platform_malloc_array(&probe, 2, 2);
            if (i + 1 < n)
            {
                NAOS_CHECK_NOT_NULL(blocks[i]);
            }
            else
            {
                NAOS_CHECK_NULL(blocks[i]);
            }
            NAOS_CHECK_UINT_EQ(probe.calls, i + 1);
        }
        NAOS_CHECK_UINT_EQ(probe.live, n - 1);
        for (i = 0; i < 6; ++i)
        {
            clopts_platform_free(&probe, blocks[i]);
        }
        NAOS_CHECK_UINT_EQ(probe.live, 0);
    }
}

/* Releasing NULL is ignored and never moves live. */
NAOS_TEST(probe_free_null_keeps_live)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *p;

    p = clopts_platform_malloc(&probe, 1);
    NAOS_CHECK_NOT_NULL(p);
    clopts_platform_free(&probe, NULL);
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    clopts_platform_free(&probe, p);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/*
 * count * size overflow returns NULL without allocating. Like the parser's
 * gateway, the attempt still passes the fault point (calls grows by one).
 */
NAOS_TEST(malloc_array_overflow_does_not_allocate)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *p;

    NAOS_CHECK_NULL(clopts_platform_malloc_array(NULL, SIZE_MAX, 2));
    NAOS_CHECK_NULL(clopts_platform_malloc_array(NULL, (SIZE_MAX / 4) + 1, 4));

    NAOS_CHECK_NULL(clopts_platform_malloc_array(&probe, SIZE_MAX, 2));
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
    NAOS_CHECK_NULL(clopts_platform_malloc_array(&probe, 2, SIZE_MAX));
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    NAOS_CHECK_UINT_EQ(probe.live, 0);

    /* A product that fits still allocates after the overflows. */
    p = clopts_platform_malloc_array(&probe, 1, 32);
    NAOS_CHECK_NOT_NULL(p);
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    clopts_platform_free(&probe, p);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/* ---- sweeps over the fault points ------------------------------------------ */

/* Upper bound of the fault points a sweep moves over; every sweep ends well before it. */
#define OOM_SWEEP_LIMIT 64

/* Creates a handle with the probe; input is the sweep's own data. */
typedef clopts_argv_t *(*oom_argv_make_fn)(clopts_platform_probe_t *probe, const void *input);

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
 * Moves fail_at from 1 until no fault point fails. Each run gives the
 * sentinel (only when it reached a failing fault point) or the expected
 * handle, and live returns to 0 after the release.
 */
static void oom_sweep_argv(oom_argv_make_fn make, const void *input, const oom_argv_expect_t *expect,
                           oom_sweep_stats_t *stats)
{
    size_t fail_at;

    memset(stats, 0, sizeof(*stats));
    for (fail_at = 1; (fail_at <= OOM_SWEEP_LIMIT) && (!stats->succeeded); ++fail_at)
    {
        clopts_platform_probe_t probe = {0, 0, 0};
        clopts_argv_t *args;

        NAOS_NOTE("argv fault point %u", (unsigned)fail_at);
        probe.fail_at = fail_at;
        args = make(&probe, input);
        stats->runs++;
        NAOS_CHECK_NOT_NULL(args);
        if (args == clopts_platform_argv_sentinel())
        {
            stats->oom_runs++;
            NAOS_CHECK_UINT_GE(probe.calls, fail_at);
            NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OUT_OF_MEMORY);
            NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
            NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
        }
        else
        {
            oom_check_argv(args, expect);
        }
        if (probe.calls < fail_at)
        {
            stats->succeeded = true;
            NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
        }
        clopts_platform_argv_free(&probe, args);
        NAOS_CHECK_UINT_EQ(probe.live, 0);
    }
}

static clopts_argv_t *oom_make_passthrough(clopts_platform_probe_t *probe, const void *input)
{
    return clopts_platform_argv_passthrough(probe, 3, (char *const *)input);
}

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
    NAOS_CHECK_UINT_GE(stats.oom_runs, 1);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

/* Creating a context: NULL at every failing fault point, never a leak. */
NAOS_TEST(sweep_env_new)
{
    size_t fail_at;
    size_t oom_runs = 0;
    bool succeeded = false;

    for (fail_at = 1; (fail_at <= OOM_SWEEP_LIMIT) && (!succeeded); ++fail_at)
    {
        clopts_platform_probe_t probe = {0, 0, 0};
        clopts_env_t *env;

        NAOS_NOTE("env_new fault point %u", (unsigned)fail_at);
        probe.fail_at = fail_at;
        env = clopts_platform_env_new(&probe);
        if (env == NULL)
        {
            oom_runs++;
            NAOS_CHECK_UINT_GE(probe.calls, fail_at);
            NAOS_CHECK_UINT_EQ(probe.live, 0);
        }
        else
        {
            NAOS_CHECK_INT_EQ(clopts_env_error(env), CLOPTS_ENV_ERROR_NONE);
            NAOS_CHECK_NULL(clopts_env_error_name(env));
        }
        if (probe.calls < fail_at)
        {
            succeeded = true;
            NAOS_CHECK_NOT_NULL(env);
        }
        clopts_env_free(env);
        NAOS_CHECK_UINT_EQ(probe.live, 0);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GE(oom_runs, 1);
}

#ifdef _WIN32
static clopts_argv_t *oom_make_from_cmdline(clopts_platform_probe_t *probe, const void *input)
{
    return clopts_platform_argv_from_cmdline(probe, (const wchar_t *)input);
}

static clopts_argv_t *oom_make_convert(clopts_platform_probe_t *probe, const void *input)
{
    return clopts_platform_argv_convert(probe, 3, (const wchar_t *const *)input);
}

/* The command line: the work block, then the handle block. */
NAOS_TEST(sweep_argv_from_cmdline)
{
    static const wchar_t cmdline[] = L"tool --name \"\x3042 \x3044\" \"\"";
    static const char *const expected[] = {"tool", "--name", "\xe3\x81\x82 \xe3\x81\x84", ""};
    oom_argv_expect_t expect = {CLOPTS_ARGV_OK, 4, -1, expected};
    oom_sweep_stats_t stats;

    oom_sweep_argv(oom_make_from_cmdline, cmdline, &expect, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    /* both the work block and the handle block are fault points */
    NAOS_CHECK_UINT_GE(stats.oom_runs, 2);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

/* A lone surrogate in the second element: the work block, then the failure handle. */
NAOS_TEST(sweep_argv_from_cmdline_invalid_utf16)
{
    static const wchar_t cmdline[] = {L't', L'o', L'o', L'l', L' ', L'a', 0xD800, L' ', L'b', 0};
    oom_argv_expect_t expect = {CLOPTS_ARGV_INVALID_UTF16, 0, 1, NULL};
    oom_sweep_stats_t stats;

    oom_sweep_argv(oom_make_from_cmdline, cmdline, &expect, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_GE(stats.oom_runs, 2);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

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
    NAOS_CHECK_UINT_GE(stats.oom_runs, 1);
    NAOS_CHECK_UINT_EQ(stats.runs, stats.oom_runs + 1);
}

/*
 * A lookup of a value several thousand code units long in a new context:
 * the first work buffer is too small, so the read is retried after it
 * grows, and the value buffer follows. Every failing fault point gives
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
        clopts_platform_probe_t probe = {0, 0, 0};
        clopts_env_t *env;
        const char *value = NULL;
        clopts_env_status_t status;
        size_t start;

        NAOS_NOTE("env lookup fault point %u", (unsigned)offset);
        env = clopts_platform_env_new(&probe);
        NAOS_CHECK_NOT_NULL(env);
        if (env == NULL)
        {
            break;
        }
        start = probe.calls;
        probe.fail_at = start + offset;
        status = clopts_env_lookup(env, "CLOPTS_TEST_OOM_LONG", &value);
        if (status == CLOPTS_ENV_FAILED)
        {
            oom_runs++;
            NAOS_CHECK_UINT_GE(probe.calls, start + offset);
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
        if (probe.calls < start + offset)
        {
            succeeded = true;
            NAOS_CHECK_INT_EQ(status, CLOPTS_ENV_FOUND);
        }
        probe.fail_at = 0;
        clopts_env_free(env);
        NAOS_CHECK_UINT_EQ(probe.live, 0);
    }
    SetEnvironmentVariableW(L"CLOPTS_TEST_OOM_LONG", NULL);
    NAOS_CHECK_TRUE(succeeded);
    /* the first work buffer, the grown one for the retry, and the value buffer */
    NAOS_CHECK_UINT_GE(oom_runs, 3);
}
#endif /* _WIN32 */

/* Preprocessor directives cannot appear inside the macro arguments, so each OS has its own list. */
#ifndef _WIN32
NAOS_SUITE(oom,
    NAOS_ADD_TEST(u16_helpers_fill_the_whole_capacity),
    NAOS_ADD_TEST(no_probe_malloc_and_free),
    NAOS_ADD_TEST(zero_size_succeeds),
    NAOS_ADD_TEST(probe_counts_calls_and_live),
    NAOS_ADD_TEST(probe_fails_nth_and_later),
    NAOS_ADD_TEST(probe_free_null_keeps_live),
    NAOS_ADD_TEST(malloc_array_overflow_does_not_allocate),
    NAOS_ADD_TEST(sweep_passthrough),
    NAOS_ADD_TEST(sweep_env_new)
)
#else
NAOS_SUITE(oom,
    NAOS_ADD_TEST(u16_helpers_fill_the_whole_capacity),
    NAOS_ADD_TEST(no_probe_malloc_and_free),
    NAOS_ADD_TEST(zero_size_succeeds),
    NAOS_ADD_TEST(probe_counts_calls_and_live),
    NAOS_ADD_TEST(probe_fails_nth_and_later),
    NAOS_ADD_TEST(probe_free_null_keeps_live),
    NAOS_ADD_TEST(malloc_array_overflow_does_not_allocate),
    NAOS_ADD_TEST(sweep_passthrough),
    NAOS_ADD_TEST(sweep_env_new),
    NAOS_ADD_TEST(sweep_argv_from_cmdline),
    NAOS_ADD_TEST(sweep_argv_from_cmdline_invalid_utf16),
    NAOS_ADD_TEST(sweep_argv_convert),
    NAOS_ADD_TEST(sweep_env_lookup_long_value)
)
#endif
