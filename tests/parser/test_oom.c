/*
 * Suite oom: the allocation gateway (clopts_parser_malloc / calloc / strdup /
 * strndup / vec_push / vec_free / free) and the allocation probe.
 *
 * Checked here: the gateway without a probe, fault points and the n-th and
 * later failures, the live block count returning to 0, overflow checks of
 * count * size and len + 1, and sweeps that move the failing fault point
 * over the assembly of the fixture tool, the parse of
 * "tool build -k a,b -o x src" and the resolution of environment variables
 * and LIST defaults (15.7).
 */

#include "parser/parser_internal.h"
#include "test_fixtures.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Returns a probe with the given fail_at and zero counters. */
static clopts_parser_probe_t make_probe(size_t fail_at)
{
    clopts_parser_probe_t probe;

    probe.fail_at = fail_at;
    probe.calls = 0;
    probe.live = 0;
    return probe;
}

/* ---- without a probe --------------------------------------------------- */

NAOS_TEST(null_probe_allocates_and_frees)
{
    unsigned char *block;
    unsigned char *zeroed;
    char *copy;
    char *part;
    size_t i;

    block = (unsigned char *)clopts_parser_malloc(NULL, 16);
    NAOS_REQUIRE_NOT_NULL(block);
    memset(block, 0xab, 16);

    zeroed = (unsigned char *)clopts_parser_calloc(NULL, 4, 8);
    NAOS_REQUIRE_NOT_NULL(zeroed);
    for (i = 0; i < 32; i++)
    {
        NAOS_CHECK_UINT_EQ(zeroed[i], 0);
    }

    copy = clopts_parser_strdup(NULL, "build");
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_CHECK_STR_EQ(copy, "build");

    part = clopts_parser_strndup(NULL, "output=file", 6);
    NAOS_REQUIRE_NOT_NULL(part);
    NAOS_CHECK_STR_EQ(part, "output");

    NAOS_CHECK_NULL(clopts_parser_strdup(NULL, NULL));

    clopts_parser_free(NULL, block);
    clopts_parser_free(NULL, zeroed);
    clopts_parser_free(NULL, copy);
    clopts_parser_free(NULL, part);
    clopts_parser_free(NULL, NULL);
}

NAOS_TEST(null_probe_vec_push_and_free)
{
    clopts_core_vec_t vec;
    int i;

    clopts_core_vec_init(&vec, sizeof(int));
    for (i = 0; i < 20; i++)
    {
        NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(NULL, &vec, &i), 0);
    }
    NAOS_REQUIRE_UINT_EQ(vec.len, 20);
    NAOS_CHECK_INT_EQ(*(const int *)clopts_core_vec_at(&vec, 19), 19);

    clopts_parser_vec_free(NULL, &vec);
    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(vec.elem_size, sizeof(int));
}

NAOS_TEST(null_probe_overflow_fails)
{
    NAOS_CHECK_NULL(clopts_parser_calloc(NULL, SIZE_MAX, 2));
    NAOS_CHECK_NULL(clopts_parser_calloc(NULL, 2, SIZE_MAX));
    NAOS_CHECK_NULL(clopts_parser_strndup(NULL, "x", SIZE_MAX));
}

/* ---- fault points ------------------------------------------------------ */

NAOS_TEST(each_call_is_one_fault_point)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec;
    void *block;
    void *zeroed;
    char *copy;
    char *part;
    int value = 7;

    clopts_core_vec_init(&vec, sizeof(int));

    block = clopts_parser_malloc(&probe, 8);
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    zeroed = clopts_parser_calloc(&probe, 2, 4);
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    copy = clopts_parser_strdup(&probe, "tool");
    NAOS_CHECK_UINT_EQ(probe.calls, 3);
    part = clopts_parser_strndup(&probe, "tool", 2);
    NAOS_CHECK_UINT_EQ(probe.calls, 4);
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 5);
    /* The second push needs no reallocation but is still a fault point. */
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 6);

    NAOS_REQUIRE_NOT_NULL(block);
    NAOS_REQUIRE_NOT_NULL(zeroed);
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_REQUIRE_NOT_NULL(part);
    NAOS_CHECK_STR_EQ(part, "to");

    /* Releases are not fault points. */
    clopts_parser_free(&probe, block);
    clopts_parser_free(&probe, zeroed);
    clopts_parser_free(&probe, copy);
    clopts_parser_free(&probe, part);
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_UINT_EQ(probe.calls, 6);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(strdup_null_is_not_a_fault_point)
{
    clopts_parser_probe_t probe = make_probe(1);

    NAOS_CHECK_NULL(clopts_parser_strdup(&probe, NULL));
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(fail_at_zero_never_fails)
{
    clopts_parser_probe_t probe = make_probe(0);
    void *blocks[50];
    size_t i;

    for (i = 0; i < 50; i++)
    {
        blocks[i] = clopts_parser_malloc(&probe, 4);
        NAOS_REQUIRE_NOT_NULL(blocks[i]);
    }
    NAOS_CHECK_UINT_EQ(probe.calls, 50);
    NAOS_CHECK_INT_EQ((int)probe.live, 50);

    for (i = 0; i < 50; i++)
    {
        clopts_parser_free(&probe, blocks[i]);
    }
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(nth_and_later_fault_points_fail)
{
    clopts_parser_probe_t probe = make_probe(3);
    clopts_core_vec_t vec;
    void *first;
    char *second;
    int value = 1;

    clopts_core_vec_init(&vec, sizeof(int));

    first = clopts_parser_malloc(&probe, 8);
    second = clopts_parser_strdup(&probe, "build");
    NAOS_REQUIRE_NOT_NULL(first);
    NAOS_REQUIRE_NOT_NULL(second);
    NAOS_CHECK_INT_EQ((int)probe.live, 2);

    /* The 3rd fault point and every later one fail, whatever the function. */
    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, 1, 8));
    NAOS_CHECK_NULL(clopts_parser_malloc(&probe, 8));
    NAOS_CHECK_NULL(clopts_parser_strdup(&probe, "x"));
    NAOS_CHECK_NULL(clopts_parser_strndup(&probe, "xy", 1));
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), -1);
    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 7);

    /* Failures allocate nothing. */
    NAOS_CHECK_INT_EQ((int)probe.live, 2);

    clopts_parser_free(&probe, first);
    clopts_parser_free(&probe, second);
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(fail_at_one_fails_everything)
{
    clopts_parser_probe_t probe = make_probe(1);
    clopts_core_vec_t vec;
    int value = 1;

    clopts_core_vec_init(&vec, sizeof(int));

    NAOS_CHECK_NULL(clopts_parser_malloc(&probe, 1));
    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, 1, 1));
    NAOS_CHECK_NULL(clopts_parser_strdup(&probe, "a"));
    NAOS_CHECK_NULL(clopts_parser_strndup(&probe, "a", 1));
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), -1);
    NAOS_CHECK_UINT_EQ(probe.calls, 5);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(failing_push_keeps_existing_elements)
{
    clopts_parser_probe_t probe = make_probe(2);
    clopts_core_vec_t vec;
    int value = 5;

    clopts_core_vec_init(&vec, sizeof(int));

    NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);

    value = 6;
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), -1);
    NAOS_REQUIRE_UINT_EQ(vec.len, 1);
    NAOS_CHECK_INT_EQ(*(const int *)clopts_core_vec_at(&vec, 0), 5);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

/* ---- live block count -------------------------------------------------- */

NAOS_TEST(live_counts_objects_and_strings)
{
    clopts_parser_probe_t probe = make_probe(0);
    void *block;
    void *zeroed;
    char *copy;
    char *part;

    block = clopts_parser_malloc(&probe, 8);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);
    zeroed = clopts_parser_calloc(&probe, 3, 3);
    NAOS_CHECK_INT_EQ((int)probe.live, 2);
    copy = clopts_parser_strdup(&probe, "");
    NAOS_CHECK_INT_EQ((int)probe.live, 3);
    part = clopts_parser_strndup(&probe, "abc", 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 4);

    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_REQUIRE_NOT_NULL(part);
    NAOS_CHECK_STR_EQ(copy, "");
    NAOS_CHECK_STR_EQ(part, "");

    clopts_parser_free(&probe, part);
    NAOS_CHECK_INT_EQ((int)probe.live, 3);
    clopts_parser_free(&probe, copy);
    clopts_parser_free(&probe, zeroed);
    clopts_parser_free(&probe, block);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(free_null_does_not_change_live)
{
    clopts_parser_probe_t probe = make_probe(0);

    clopts_parser_free(&probe, NULL);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
}

NAOS_TEST(vec_counts_once_per_buffer)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec;
    int i;

    clopts_core_vec_init(&vec, sizeof(int));

    /* Grows from 8 to 16 to 32; still one block. */
    for (i = 0; i < 20; i++)
    {
        NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(&probe, &vec, &i), 0);
        NAOS_CHECK_INT_EQ((int)probe.live, 1);
    }
    NAOS_CHECK_UINT_EQ(probe.calls, 20);

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);

    /* Freeing an empty vec, again or zero-initialized, does not count. */
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);

    /* After vec_free the vec can be reused and is counted again. */
    NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(&probe, &vec, &i), 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(free_zero_initialized_vec_does_not_change_live)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec = {0};

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
}

NAOS_TEST(live_returns_to_zero_at_every_fail_at)
{
    size_t fail_at;

    /* 6 fault points in total; fail_at 7 lets everything succeed. */
    for (fail_at = 1; fail_at <= 7; fail_at++)
    {
        clopts_parser_probe_t probe = make_probe(fail_at);
        clopts_core_vec_t vec;
        void *block;
        void *zeroed;
        char *copy;
        char *part;
        int value = 3;
        int rc1;
        int rc2;

        clopts_core_vec_init(&vec, sizeof(int));

        block = clopts_parser_malloc(&probe, 8);
        zeroed = clopts_parser_calloc(&probe, 2, 8);
        copy = clopts_parser_strdup(&probe, "positional");
        rc1 = clopts_parser_vec_push(&probe, &vec, &value);
        part = clopts_parser_strndup(&probe, "positional", 3);
        rc2 = clopts_parser_vec_push(&probe, &vec, &value);

        NAOS_CHECK_UINT_EQ(probe.calls, 6);
        NAOS_CHECK_INT_EQ((block == NULL) ? 1 : 0, (fail_at <= 1) ? 1 : 0);
        NAOS_CHECK_INT_EQ((zeroed == NULL) ? 1 : 0, (fail_at <= 2) ? 1 : 0);
        NAOS_CHECK_INT_EQ((copy == NULL) ? 1 : 0, (fail_at <= 3) ? 1 : 0);
        NAOS_CHECK_INT_EQ(rc1, (fail_at <= 4) ? -1 : 0);
        NAOS_CHECK_INT_EQ((part == NULL) ? 1 : 0, (fail_at <= 5) ? 1 : 0);
        NAOS_CHECK_INT_EQ(rc2, (fail_at <= 6) ? -1 : 0);

        clopts_parser_free(&probe, block);
        clopts_parser_free(&probe, zeroed);
        clopts_parser_free(&probe, copy);
        clopts_parser_free(&probe, part);
        clopts_parser_vec_free(&probe, &vec);
        NAOS_CHECK_INT_EQ((int)probe.live, 0);
    }
}

/* ---- overflow ---------------------------------------------------------- */

NAOS_TEST(calloc_overflow_fails_without_allocating)
{
    clopts_parser_probe_t probe = make_probe(0);

    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, SIZE_MAX, 2));
    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, (SIZE_MAX / 2) + 1, 2));
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(strndup_overflow_fails_without_allocating)
{
    clopts_parser_probe_t probe = make_probe(0);

    NAOS_CHECK_NULL(clopts_parser_strndup(&probe, "x", SIZE_MAX));
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(vec_push_overflow_fails_without_counting)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec;
    unsigned char elem[16] = {0};

    /* 8 * (SIZE_MAX / 4) overflows, so the first growth is refused. */
    clopts_core_vec_init(&vec, SIZE_MAX / 4);

    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, elem), -1);
    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

/* ---- sweeps over the builder, the parse and the resolution -------------- */

/*
 * Each sweep moves the failing fault point fail_at = 1, 2, ... until a run
 * passes no failing fault point (probe.calls < fail_at after the whole
 * scenario). Every run must end with OUT_OF_MEMORY or with the same outcome
 * as a run without failures, and probe.live must be 0 once the result and
 * the program are released. The sweeps stop at SWEEP_LIMIT runs at most.
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

/* Parses argv against a tool built without a probe and returns the snapshot. */
static void reference_parse(int argc, char *const *argv, const fixture_env_var_t *vars, size_t var_count,
                            snapshot_t *snap)
{
    fixture_tool_t t;

    fixture_tool_init(&t, NULL);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_INT_EQ(t.program->error.status, CLOPTS_OK);
    (void)parse_and_snap(&t, argc, argv, vars, var_count, snap);
    fixture_tool_free(&t);
}

/* Counts of one sweep, checked by the callers. */
typedef struct sweep_stats
{
    size_t runs;
    size_t oom_runs;
    bool succeeded;     /* the last run passed no failing fault point */
} sweep_stats_t;

/*
 * Sweep over the parse only: tool is built with fail_at disabled, then the
 * fault points are counted from the start of the parse (result allocation
 * included). Each run is OUT_OF_MEMORY or equal to expected.
 */
static void sweep_parse(int argc, char *const *argv, const fixture_env_var_t *vars, size_t var_count,
                        const snapshot_t *expected, sweep_stats_t *stats)
{
    size_t offset;

    memset(stats, 0, sizeof(*stats));
    for (offset = 1; (offset <= SWEEP_LIMIT) && (!stats->succeeded); offset++)
    {
        clopts_parser_probe_t probe = make_probe(0);
        fixture_tool_t t;
        snapshot_t snap;
        clopts_status_t status;

        NAOS_NOTE("parse fault point %u", (unsigned)offset);
        fixture_tool_init(&t, &probe);
        NAOS_REQUIRE_NOT_NULL(t.program);
        NAOS_REQUIRE_INT_EQ(t.program->error.status, CLOPTS_OK);

        probe.calls = 0;
        probe.fail_at = offset;
        status = parse_and_snap(&t, argc, argv, vars, var_count, &snap);
        stats->runs++;
        if (status == CLOPTS_OUT_OF_MEMORY)
        {
            stats->oom_runs++;
            /* Only a run that reached a failing fault point may end so. */
            NAOS_CHECK_UINT_GE(probe.calls, offset);
        }
        else
        {
            check_snapshot_eq(&snap, expected);
        }
        if (probe.calls < offset)
        {
            stats->succeeded = true;
            NAOS_CHECK_INT_EQ(status, expected->status);
        }

        probe.fail_at = 0;
        fixture_tool_free(&t);
        NAOS_CHECK_INT_EQ(probe.live, 0);
    }
}

/* Scenario (a): building tool, then validating and parsing it. */
NAOS_TEST(sweep_tool_build_then_validate_and_parse)
{
    char *argv[] = { "tool", "build", "-k", "a,b", "-o", "x", "src", NULL };
    snapshot_t expected;
    size_t fail_at;
    size_t build_oom_runs = 0;
    size_t later_oom_runs = 0;
    bool succeeded = false;

    reference_parse(7, argv, NULL, 0, &expected);
    NAOS_REQUIRE_INT_EQ(expected.status, CLOPTS_OK);

    for (fail_at = 1; (fail_at <= SWEEP_LIMIT) && (!succeeded); fail_at++)
    {
        clopts_parser_probe_t probe = make_probe(fail_at);
        fixture_tool_t t;
        fixture_env_t env;
        clopts_result_t *validated;
        clopts_result_t *parsed;

        NAOS_NOTE("build fault point %u", (unsigned)fail_at);
        fixture_tool_init(&t, &probe);
        /* clopts_program_new() runs before the probe is attached. */
        NAOS_REQUIRE_NOT_NULL(t.program);
        fixture_env_init(&env, NULL, 0);

        if (probe.calls >= fail_at)
        {
            size_t calls_after_build = probe.calls;

            build_oom_runs++;
            NAOS_CHECK_INT_EQ(t.program->error.status, CLOPTS_OUT_OF_MEMORY);
            /*
             * Let every later allocation succeed: the sentinel then comes
             * from the recorded OUT_OF_MEMORY, not from a failing result
             * allocation.
             */
            probe.fail_at = 0;
            validated = clopts_program_validate(t.program);
            parsed = clopts_parse_with_env(t.program, 7, argv, fixture_env_getenv, &env);
            NAOS_CHECK_PTR_EQ(validated, clopts_parser_result_oom());
            NAOS_CHECK_PTR_EQ(parsed, clopts_parser_result_oom());
            NAOS_CHECK_UINT_EQ(probe.calls, calls_after_build);
            NAOS_CHECK_FALSE(env.query_count > 0);
            clopts_result_free(validated);
            clopts_result_free(parsed);
        }
        else
        {
            clopts_status_t validate_status;
            snapshot_t snap;

            NAOS_CHECK_INT_EQ(t.program->error.status, CLOPTS_OK);
            validated = clopts_program_validate(t.program);
            NAOS_REQUIRE_NOT_NULL(validated);
            validate_status = clopts_result_status(validated);
            NAOS_CHECK_TRUE((validate_status == CLOPTS_OK) || (validate_status == CLOPTS_OUT_OF_MEMORY));
            clopts_result_free(validated);

            parsed = clopts_parse_with_env(t.program, 7, argv, fixture_env_getenv, &env);
            NAOS_REQUIRE_NOT_NULL(parsed);
            snap_take(&snap, parsed, &t);
            clopts_result_free(parsed);
            if (snap.status == CLOPTS_OUT_OF_MEMORY)
            {
                later_oom_runs++;
            }
            else
            {
                check_snapshot_eq(&snap, &expected);
            }
            if (probe.calls < fail_at)
            {
                succeeded = true;
                NAOS_CHECK_INT_EQ(validate_status, CLOPTS_OK);
                NAOS_CHECK_INT_EQ(snap.status, CLOPTS_OK);
            }
            probe.fail_at = 0;
        }

        fixture_tool_free(&t);
        NAOS_CHECK_INT_EQ(probe.live, 0);
    }

    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(build_oom_runs, 0);
    NAOS_CHECK_UINT_GT(later_oom_runs, 0);
}

/* Scenario (b): the parse of tool build -k a,b -o x src, result allocation included. */
NAOS_TEST(sweep_parse_build_command)
{
    char *argv[] = { "tool", "build", "-k", "a,b", "-o", "x", "src", NULL };
    snapshot_t expected;
    sweep_stats_t stats;

    reference_parse(7, argv, NULL, 0, &expected);
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

    reference_parse(3, argv, vars, ARRAY_LEN(vars), &expected);
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

    reference_parse(3, argv, vars, ARRAY_LEN(vars), &expected);
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
    NAOS_ADD_TEST(null_probe_allocates_and_frees),
    NAOS_ADD_TEST(null_probe_vec_push_and_free),
    NAOS_ADD_TEST(null_probe_overflow_fails),
    NAOS_ADD_TEST(each_call_is_one_fault_point),
    NAOS_ADD_TEST(strdup_null_is_not_a_fault_point),
    NAOS_ADD_TEST(fail_at_zero_never_fails),
    NAOS_ADD_TEST(nth_and_later_fault_points_fail),
    NAOS_ADD_TEST(fail_at_one_fails_everything),
    NAOS_ADD_TEST(failing_push_keeps_existing_elements),
    NAOS_ADD_TEST(live_counts_objects_and_strings),
    NAOS_ADD_TEST(free_null_does_not_change_live),
    NAOS_ADD_TEST(vec_counts_once_per_buffer),
    NAOS_ADD_TEST(free_zero_initialized_vec_does_not_change_live),
    NAOS_ADD_TEST(live_returns_to_zero_at_every_fail_at),
    NAOS_ADD_TEST(calloc_overflow_fails_without_allocating),
    NAOS_ADD_TEST(strndup_overflow_fails_without_allocating),
    NAOS_ADD_TEST(vec_push_overflow_fails_without_counting),
    NAOS_ADD_TEST(sweep_tool_build_then_validate_and_parse),
    NAOS_ADD_TEST(sweep_parse_build_command),
    NAOS_ADD_TEST(sweep_resolve_env_list),
    NAOS_ADD_TEST(sweep_resolve_default_list)
)
