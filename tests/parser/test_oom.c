/*
 * Suite oom: sweeps that move the failing allocation over the assembly of the
 * fixture tool, the parse of "tool build -k a,b -o x src" and the resolution
 * of environment variables and LIST defaults (15.7).
 *
 * The allocations are counted and made to fail by the fault-injecting
 * allocator of tests/support/test_alloc.h, installed with
 * clopts_set_allocator(): each malloc, calloc or realloc call of the library
 * is one fault point (from the first clopts_command_new() of the assembly to
 * clopts_program_new() at its end), and every block must have been released
 * through it at the end of each run. The allocation
 * gateway itself (clopts_core_malloc and the others, the overflow checks and
 * the vec growth) is tested in the core suite.
 */

#include "parser/parser_internal.h"
#include "test_alloc.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <string.h>

/* ---- sweeps over the builder, the parse and the resolution -------------- */

/*
 * Each sweep moves the failing fault point n = 1, 2, ... until a run passes
 * no failing fault point: the run with n completes and fewer than n fault
 * points were passed in the swept part (test_alloc.calls < n, counted from
 * the start of that part). Every parse must end with OUT_OF_MEMORY or with
 * the same outcome as a run without failures (a fault point in the assembly
 * instead leaves no program at all, see scenario (a)), and test_alloc_end()
 * must report no live block and no bad release once the result and the
 * program are released. The sweeps stop at SWEEP_LIMIT runs at most.
 */
#define SWEEP_LIMIT 1000
#define SNAP_TEXT_SIZE 32
#define SNAP_MAX_ITEMS 4

/* Number of elements of a static array. */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* A copied string accessor value: present is false for NULL. */
typedef struct snap_text
{
    bool present;
    char text[SNAP_TEXT_SIZE];
} snap_text_t;

/* A copied LIST value. */
typedef struct snap_list
{
    size_t count;
    char items[SNAP_MAX_ITEMS][SNAP_TEXT_SIZE];
    clopts_source_t source;
} snap_list_t;

/*
 * The observable outcome of one parse of tool, copied so that it outlives
 * the result and can be compared between different fixture instances (the
 * command is identified by its role, not by its handle).
 */
typedef struct snapshot
{
    clopts_status_t status;
    clopts_usage_error_t usage_error;
    int command;                /* 0: none, 1: build, 2: copy, 3: cat, -1: other */
    unsigned verbose;
    bool color;
    snap_text_t config;
    clopts_source_t config_source;
    snap_text_t output;
    clopts_source_t output_source;
    snap_list_t keys;
    unsigned jobs;
    snap_list_t tags;
    size_t positional_count;
    char positionals[SNAP_MAX_ITEMS][SNAP_TEXT_SIZE];
} snapshot_t;

/* Copies s (which may be NULL) into dst; the text must fit. */
static void snap_copy_text(snap_text_t *dst, const char *s)
{
    memset(dst, 0, sizeof(*dst));
    if (s != NULL)
    {
        NAOS_REQUIRE_UINT_LT(strlen(s), SNAP_TEXT_SIZE);
        dst->present = true;
        strcpy(dst->text, s);
    }
}

static void snap_copy_list(snap_list_t *dst, const clopts_result_t *result, const clopts_option_t *option)
{
    size_t i;

    memset(dst, 0, sizeof(*dst));
    dst->count = clopts_result_list_count(result, option);
    dst->source = clopts_result_source(result, option);
    NAOS_REQUIRE_UINT_LE(dst->count, SNAP_MAX_ITEMS);
    for (i = 0; i < dst->count; i++)
    {
        const char *item = clopts_result_list_at(result, option, i);

        NAOS_REQUIRE_NOT_NULL(item);
        NAOS_REQUIRE_UINT_LT(strlen(item), SNAP_TEXT_SIZE);
        strcpy(dst->items[i], item);
    }
}

/* Fills snap from result, read through the handles of t. */
static void snap_take(snapshot_t *snap, const clopts_result_t *result, const fixture_tool_t *t)
{
    const clopts_command_t *command;
    size_t i;

    memset(snap, 0, sizeof(*snap));
    snap->status = clopts_result_status(result);
    snap->usage_error = clopts_result_usage_error(result);
    command = clopts_result_command(result);
    if (command == NULL)
    {
        snap->command = 0;
    }
    else if (command == t->build)
    {
        snap->command = 1;
    }
    else if (command == t->copy)
    {
        snap->command = 2;
    }
    else if (command == t->cat)
    {
        snap->command = 3;
    }
    else
    {
        snap->command = -1;
    }
    snap->verbose = clopts_result_count(result, t->verbose);
    snap->color = clopts_result_flag(result, t->color);
    snap_copy_text(&snap->config, clopts_result_value(result, t->config));
    snap->config_source = clopts_result_source(result, t->config);
    snap_copy_text(&snap->output, clopts_result_value(result, t->output));
    snap->output_source = clopts_result_source(result, t->output);
    snap_copy_list(&snap->keys, result, t->keys);
    snap->jobs = clopts_result_count(result, t->jobs);
    snap_copy_list(&snap->tags, result, t->tags);
    snap->positional_count = clopts_result_positional_count(result);
    NAOS_REQUIRE_UINT_LE(snap->positional_count, SNAP_MAX_ITEMS);
    for (i = 0; i < snap->positional_count; i++)
    {
        const char *arg = clopts_result_positional_at(result, i);

        NAOS_REQUIRE_NOT_NULL(arg);
        NAOS_REQUIRE_UINT_LT(strlen(arg), SNAP_TEXT_SIZE);
        strcpy(snap->positionals[i], arg);
    }
}

static void check_text_eq(const snap_text_t *actual, const snap_text_t *expected)
{
    NAOS_CHECK_INT_EQ(actual->present ? 1 : 0, expected->present ? 1 : 0);
    NAOS_CHECK_STR_EQ(actual->text, expected->text);
}

static void check_list_eq(const snap_list_t *actual, const snap_list_t *expected)
{
    size_t i;

    NAOS_CHECK_INT_EQ(actual->source, expected->source);
    NAOS_REQUIRE_UINT_EQ(actual->count, expected->count);
    for (i = 0; i < expected->count; i++)
    {
        NAOS_CHECK_STR_EQ(actual->items[i], expected->items[i]);
    }
}

/* Checks that actual is the same outcome as expected, field by field. */
static void check_snapshot_eq(const snapshot_t *actual, const snapshot_t *expected)
{
    size_t i;

    NAOS_CHECK_INT_EQ(actual->status, expected->status);
    NAOS_CHECK_INT_EQ(actual->usage_error, expected->usage_error);
    NAOS_CHECK_INT_EQ(actual->command, expected->command);
    NAOS_CHECK_UINT_EQ(actual->verbose, expected->verbose);
    NAOS_CHECK_INT_EQ(actual->color ? 1 : 0, expected->color ? 1 : 0);
    check_text_eq(&actual->config, &expected->config);
    NAOS_CHECK_INT_EQ(actual->config_source, expected->config_source);
    check_text_eq(&actual->output, &expected->output);
    NAOS_CHECK_INT_EQ(actual->output_source, expected->output_source);
    check_list_eq(&actual->keys, &expected->keys);
    NAOS_CHECK_UINT_EQ(actual->jobs, expected->jobs);
    check_list_eq(&actual->tags, &expected->tags);
    NAOS_REQUIRE_UINT_EQ(actual->positional_count, expected->positional_count);
    for (i = 0; i < expected->positional_count; i++)
    {
        NAOS_CHECK_STR_EQ(actual->positionals[i], expected->positionals[i]);
    }
}

/*
 * Parses argv against t->program with a fresh mock environment over vars,
 * takes a snapshot when snap is not NULL, releases the result and returns
 * its status.
 */
static clopts_status_t parse_and_snap(fixture_tool_t *t, int argc, char *const *argv,
                                      const fixture_env_var_t *vars, size_t var_count, snapshot_t *snap)
{
    fixture_env_t env;
    clopts_result_t *result;
    clopts_status_t status;

    fixture_env_init(&env, vars, var_count);
    result = clopts_parse_with_env(t->program, argc, argv, fixture_env_getenv, &env);
    NAOS_REQUIRE_NOT_NULL(result);
    status = clopts_result_status(result);
    if (snap != NULL)
    {
        snap_take(snap, result, t);
    }
    clopts_result_free(result);
    return status;
}

/*
 * Parses argv against a tool built without failures and returns the
 * snapshot. When not NULL, *build_calls gets the number of fault points of
 * the assembly and *parse_calls that of the parse.
 */
static void reference_parse(int argc, char *const *argv, const fixture_env_var_t *vars, size_t var_count,
                            snapshot_t *snap, size_t *build_calls, size_t *parse_calls)
{
    fixture_tool_t t;
    size_t built;

    test_alloc_begin(0);
    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    built = test_alloc.calls;
    (void)parse_and_snap(&t, argc, argv, vars, var_count, snap);
    if (build_calls != NULL)
    {
        *build_calls = built;
    }
    if (parse_calls != NULL)
    {
        *parse_calls = test_alloc.calls - built;
    }
    fixture_tool_free(&t);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* Counts of one sweep, checked by the callers. */
typedef struct sweep_stats
{
    size_t runs;
    size_t oom_runs;
    bool succeeded;     /* the last run passed no failing fault point */
} sweep_stats_t;

/*
 * Sweep over the parse only: tool is built without failures, then the fault
 * points are counted from the start of the parse (result allocation
 * included): offset n makes the n-th allocation of the parse fail. Each run
 * is OUT_OF_MEMORY or equal to expected.
 */
static void sweep_parse(int argc, char *const *argv, const fixture_env_var_t *vars, size_t var_count,
                        const snapshot_t *expected, sweep_stats_t *stats)
{
    size_t offset;

    memset(stats, 0, sizeof(*stats));
    for (offset = 1; (offset <= SWEEP_LIMIT) && (!stats->succeeded); offset++)
    {
        fixture_tool_t t;
        snapshot_t snap;
        clopts_status_t status;
        size_t base;
        size_t parse_calls;

        NAOS_NOTE("parse fault point %u", (unsigned)offset);
        test_alloc_begin(0);
        fixture_tool_init(&t);
        NAOS_REQUIRE_NOT_NULL(t.program);
        NAOS_REQUIRE_TRUE(t.ok);

        base = test_alloc.calls;
        test_alloc_set_fail_at(base + offset);
        status = parse_and_snap(&t, argc, argv, vars, var_count, &snap);
        parse_calls = test_alloc.calls - base;
        test_alloc_set_fail_at(0);
        stats->runs++;
        if (status == CLOPTS_OUT_OF_MEMORY)
        {
            stats->oom_runs++;
            /* Only a run that reached a failing fault point may end so. */
            NAOS_CHECK_UINT_GE(parse_calls, offset);
        }
        else
        {
            check_snapshot_eq(&snap, expected);
        }
        if (parse_calls < offset)
        {
            stats->succeeded = true;
            NAOS_CHECK_INT_EQ(status, expected->status);
        }

        fixture_tool_free(&t);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
}

/*
 * Scenario (a): building tool, then parsing it. fail_at counts from the
 * first allocation of the assembly (the root's clopts_command_new()), and
 * every fault point from fail_at on fails.
 *
 * A failed allocation during the assembly makes that builder call return
 * CLOPTS_DECL_OUT_OF_MEMORY and change nothing; the later builder calls get
 * NULL handles, and clopts_program_new() at the end (which allocates) fails
 * as well, so the fixture releases the root with everything attached to it
 * and the commands, and leaves the whole struct zeroed. So every fault point
 * of the assembly gives no program, no handle and no live block, and a
 * parse of the NULL program is the sentinel without any allocation or
 * environment lookup. A fault point after the assembly gives the complete
 * tool, whose parse is OUT_OF_MEMORY or the outcome of the reference run.
 * Every fault point of the assembly and of the parse is reached exactly once.
 */
NAOS_TEST(sweep_tool_build_then_parse)
{
    static fixture_tool_t zero; /* all zero (static storage); not const: MSVC C4132 */
    char *argv[] = { "tool", "build", "-k", "a,b", "-o", "x", "src", NULL };
    snapshot_t expected;
    size_t build_calls = 0;
    size_t parse_calls = 0;
    size_t fail_at;
    size_t build_oom_runs = 0;
    size_t later_oom_runs = 0;
    bool succeeded = false;

    reference_parse(7, argv, NULL, 0, &expected, &build_calls, &parse_calls);
    NAOS_REQUIRE_INT_EQ(expected.status, CLOPTS_OK);
    NAOS_REQUIRE_UINT_GT(build_calls, 0);
    NAOS_REQUIRE_UINT_GT(parse_calls, 0);

    for (fail_at = 1; (fail_at <= SWEEP_LIMIT) && (!succeeded); fail_at++)
    {
        fixture_tool_t t;
        fixture_env_t env;
        clopts_result_t *parsed;
        snapshot_t snap;

        NAOS_NOTE("build fault point %u", (unsigned)fail_at);
        test_alloc_begin(fail_at);
        fixture_tool_init(&t);
        fixture_env_init(&env, NULL, 0);

        if (t.program == NULL)
        {
            size_t calls_after_build = test_alloc.calls;

            build_oom_runs++;
            NAOS_CHECK_UINT_LE(fail_at, build_calls);
            NAOS_CHECK_UINT_GE(calls_after_build, fail_at);
            NAOS_CHECK_FALSE(t.ok);
            NAOS_CHECK_TRUE(memcmp(&t, &zero, sizeof(t)) == 0);
            NAOS_CHECK_INT_EQ(test_alloc.live, 0);
            test_alloc_set_fail_at(0);
            parsed = clopts_parse_with_env(t.program, 7, argv, fixture_env_getenv, &env);
            NAOS_CHECK_PTR_EQ(parsed, clopts_parser_result_oom());
            NAOS_CHECK_UINT_EQ(test_alloc.calls, calls_after_build);
            NAOS_CHECK_FALSE(env.query_count > 0);
            clopts_result_free(parsed);
        }
        else
        {
            /* The assembly passed no failing fault point. */
            NAOS_CHECK_UINT_GT(fail_at, build_calls);
            NAOS_CHECK_UINT_EQ(test_alloc.calls, build_calls);
            NAOS_CHECK_TRUE(t.ok);
            parsed = clopts_parse_with_env(t.program, 7, argv, fixture_env_getenv, &env);
            NAOS_REQUIRE_NOT_NULL(parsed);
            snap_take(&snap, parsed, &t);
            clopts_result_free(parsed);
            if (snap.status == CLOPTS_OUT_OF_MEMORY)
            {
                later_oom_runs++;
                NAOS_CHECK_UINT_GE(test_alloc.calls, fail_at);
            }
            else
            {
                check_snapshot_eq(&snap, &expected);
            }
            if (test_alloc.calls < fail_at)
            {
                succeeded = true;
                NAOS_CHECK_INT_EQ(snap.status, CLOPTS_OK);
            }
            test_alloc_set_fail_at(0);
        }

        fixture_tool_free(&t);
        NAOS_CHECK_TRUE(test_alloc_end());
    }

    NAOS_CHECK_TRUE(succeeded);
    /* Every fault point of the assembly (clopts_program_new() included) was failed once. */
    NAOS_CHECK_UINT_EQ(build_oom_runs, build_calls);
    /* Every fault point of the parse ended it with OUT_OF_MEMORY. */
    NAOS_CHECK_UINT_EQ(later_oom_runs, parse_calls);
    NAOS_CHECK_UINT_EQ(fail_at - 1, build_calls + parse_calls + 1);
}

/* Scenario (b): the parse of tool build -k a,b -o x src, result allocation included. */
NAOS_TEST(sweep_parse_build_command)
{
    char *argv[] = { "tool", "build", "-k", "a,b", "-o", "x", "src", NULL };
    snapshot_t expected;
    sweep_stats_t stats;

    reference_parse(7, argv, NULL, 0, &expected, NULL, NULL);
    NAOS_REQUIRE_INT_EQ(expected.status, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(expected.command, 1);
    NAOS_CHECK_STR_EQ(expected.output.text, "x");
    NAOS_REQUIRE_UINT_EQ(expected.keys.count, 2);
    NAOS_CHECK_STR_EQ(expected.keys.items[0], "a");
    NAOS_CHECK_STR_EQ(expected.keys.items[1], "b");
    NAOS_REQUIRE_UINT_EQ(expected.positional_count, 1);
    NAOS_CHECK_STR_EQ(expected.positionals[0], "src");

    sweep_parse(7, argv, NULL, 0, &expected, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_GT(stats.oom_runs, 0);
    /* The first run fails the result allocation itself. */
    NAOS_CHECK_UINT_GT(stats.runs, 2);
}

/* Scenario (c): environment variables, a LIST from the environment. */
NAOS_TEST(sweep_resolve_env_list)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "cfg" },
        { "TOOL_TAGS", "x,y,z" }
    };
    char *argv[] = { "tool", "cat", "f", NULL };
    snapshot_t expected;
    sweep_stats_t stats;

    reference_parse(3, argv, vars, ARRAY_LEN(vars), &expected, NULL, NULL);
    NAOS_REQUIRE_INT_EQ(expected.status, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(expected.command, 3);
    NAOS_CHECK_STR_EQ(expected.config.text, "cfg");
    NAOS_CHECK_INT_EQ(expected.config_source, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_UINT_EQ(expected.tags.count, 3);
    NAOS_CHECK_STR_EQ(expected.tags.items[2], "z");
    NAOS_CHECK_INT_EQ(expected.tags.source, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_UINT_EQ(expected.positional_count, 1);

    sweep_parse(3, argv, vars, ARRAY_LEN(vars), &expected, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_GT(stats.oom_runs, 0);
}

/* Scenario (c): the LIST default "a,b" split when TOOL_TAGS is unset. */
NAOS_TEST(sweep_resolve_default_list)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "cfg" }
    };
    char *argv[] = { "tool", "cat", "f", NULL };
    snapshot_t expected;
    sweep_stats_t stats;

    reference_parse(3, argv, vars, ARRAY_LEN(vars), &expected, NULL, NULL);
    NAOS_REQUIRE_INT_EQ(expected.status, CLOPTS_OK);
    NAOS_CHECK_STR_EQ(expected.config.text, "cfg");
    NAOS_REQUIRE_UINT_EQ(expected.tags.count, 2);
    NAOS_CHECK_STR_EQ(expected.tags.items[0], "a");
    NAOS_CHECK_STR_EQ(expected.tags.items[1], "b");
    NAOS_CHECK_INT_EQ(expected.tags.source, CLOPTS_SOURCE_DEFAULT);

    sweep_parse(3, argv, vars, ARRAY_LEN(vars), &expected, &stats);
    NAOS_CHECK_TRUE(stats.succeeded);
    NAOS_CHECK_UINT_GT(stats.oom_runs, 0);
}

NAOS_SUITE(oom,
    NAOS_ADD_TEST(sweep_tool_build_then_parse),
    NAOS_ADD_TEST(sweep_parse_build_command),
    NAOS_ADD_TEST(sweep_resolve_env_list),
    NAOS_ADD_TEST(sweep_resolve_default_list)
)
