/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite values: repetition rules of FLAG / COUNT / VALUE / LIST, list
 * delimiters and the count limit, all through argv and the parse functions.
 *
 * Ported from the first-version values suite onto the builder fixtures of
 * test_fixtures.c (handles instead of index constants) and the result handle
 * accessors. The first-version cases that recorded through the internal
 * result store directly are covered by the result suite; the aspects that
 * suite does not cover (a repeated FLAG, a VALUE kept as is, LIST white space
 * and copies, unset slots, counting with the default limit) are checked here
 * through argv instead. The cases named nosub_* cover a
 * program without subcommands. The status is always checked with
 * NAOS_REQUIRE_INT_EQ before values are read.
 */

#include "parser/parser_internal.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <clopts/parse.h>

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Maximum number of argv elements parse_checked handles. */
#define MAX_ARGV 16

/*
 * Parses the NULL-terminated argv for program with the mock environment vars
 * (var_count entries, may be NULL / 0) and checks that the result is a parse
 * result (not the sentinel) and that neither the argv array nor the strings
 * it points to changed.
 */
static clopts_result_t *parse_checked(clopts_program_t *program, char **argv,
                                      const fixture_env_var_t *vars, size_t var_count)
{
    char *saved[MAX_ARGV];
    char text[MAX_ARGV][32];
    fixture_env_t env;
    clopts_result_t *result;
    int argc = 0;
    int i;

    while (argv[argc] != NULL)
    {
        NAOS_REQUIRE_TRUE(argc + 1 < MAX_ARGV);
        NAOS_REQUIRE_TRUE(strlen(argv[argc]) < sizeof(text[0]));
        saved[argc] = argv[argc];
        strcpy(text[argc], argv[argc]);
        argc++;
    }
    fixture_env_init(&env, vars, var_count);
    result = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, &env);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved[i]);
        NAOS_CHECK_STR_EQ(argv[i], text[i]);
    }
    NAOS_CHECK_NULL(argv[argc]);
    return result;
}

/* Builds the tool fixture into f and parses argv with an empty environment. */
static clopts_result_t *parse_tool(fixture_tool_t *f, char **argv)
{
    fixture_tool_init(f);
    NAOS_REQUIRE_NOT_NULL(f->program);
    NAOS_REQUIRE_TRUE(f->ok);
    return parse_checked(f->program, argv, NULL, 0);
}

/* Builds the tool fixture into f and parses argv with the environment vars. */
static clopts_result_t *parse_tool_env(fixture_tool_t *f, char **argv,
                                       const fixture_env_var_t *vars, size_t var_count)
{
    fixture_tool_init(f);
    NAOS_REQUIRE_NOT_NULL(f->program);
    NAOS_REQUIRE_TRUE(f->ok);
    return parse_checked(f->program, argv, vars, var_count);
}

/* Checks the items of a LIST option against expected[0..count-1]. */
static void check_items(const clopts_result_t *result, const clopts_option_t *option,
                        const char *const *expected, size_t count)
{
    size_t i;

    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, option), count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, option, i), expected[i]);
    }
}

/* Checks the positionals of result against expected[0..count-1] (by pointer). */
static void check_positionals(const clopts_result_t *result, char *const *expected, size_t count)
{
    size_t i;

    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, i), expected[i]);
    }
}

/* ---- sentinel ----------------------------------------------------------- */

NAOS_TEST(sentinel_values_are_unset)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, NULL));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, NULL), 0);
    NAOS_CHECK_NULL(clopts_result_get_value(result, NULL));
    NAOS_CHECK_UINT_EQ(clopts_result_get_list_count(result, NULL), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, NULL), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
}

/* ---- fixtures ---------------------------------------------------------- */

NAOS_TEST(fixture_list_delimiters_are_as_designed)
{
    fixture_tool_t f;

    fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(f.keys), CLOPTS_OPTION_LIST);
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(f.keys), ',');
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(f.tags), CLOPTS_OPTION_LIST);
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(f.tags), ',');
    fixture_tool_free(&f);
}

NAOS_TEST(fixture_count_limits_are_as_designed)
{
    fixture_tool_t f;

    fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(f.jobs), CLOPTS_OPTION_COUNT);
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(f.jobs), 8);
    /* 0 selects the library default (UINT_MAX). */
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(f.verbose), CLOPTS_OPTION_COUNT);
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(f.verbose), 0);
    fixture_tool_free(&f);
}

/* ---- aspects of the first-version store cases, through argv ------------ */

NAOS_TEST(parse_unset_options_read_as_empty)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.verbose), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(clopts_result_get_value(result, f.config));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.config), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(clopts_result_get_list_count(result, f.keys), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.keys), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.jobs), 0);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_flag_repeated_stays_true)
{
    char *argv[] = { "tool", "--color", "cat", "--color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_count_with_default_limit_counts_every_occurrence)
{
    char *argv[] = { "tool", "-vvv", "cat", "-v", "--verbose", "-vv", "--verbose", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    /* 3 + 1 + 1 + 2 + 1 = 8: no clamp below the default limit. */
    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 8);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.verbose), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_value_keeps_the_string_as_is)
{
    char *argv[] = { "tool", "build", "x", "-o", "not-a-number", NULL };
    char *argv_empty[] = { "tool", "build", "x", "--output=", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    /* No conversion or validation: any string, including "", is kept. */
    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[4]);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "not-a-number");
    clopts_result_destroy(result);
    fixture_tool_free(&f);

    result = parse_tool(&f, argv_empty);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv_empty[3] + 9);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_list_keeps_whitespace)
{
    static const char *const expected[] = { " a ", " b" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", " a , b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.keys, expected, ARRAY_LEN(expected));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_list_items_are_copies)
{
    char t0[] = "tool";
    char t1[] = "build";
    char t2[] = "x";
    char t3[] = "-o";
    char t4[] = "y";
    char t5[] = "-k";
    char t6[] = "c";
    char *argv[8];
    fixture_tool_t f;
    clopts_result_t *result;

    argv[0] = t0;
    argv[1] = t1;
    argv[2] = t2;
    argv[3] = t3;
    argv[4] = t4;
    argv[5] = t5;
    argv[6] = t6;
    argv[7] = NULL;
    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, f.keys), 1);
    NAOS_CHECK_TRUE(clopts_result_get_list_at(result, f.keys, 0) != t6);

    /* Changing the argv string after the parse does not change the item. */
    t6[0] = 'z';
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 0), "c");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- through the parse function ---------------------------------------- */

NAOS_TEST(parse_short_list_repeats_are_concatenated)
{
    static const char *const expected[] = { "a", "b", "c" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", "a,b", "-k", "c", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.keys, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.keys), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_long_list_repeats_are_concatenated)
{
    static const char *const expected[] = { "a", "b", "c" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "--keys=a", "--keys", "b,c", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.keys, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.keys), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_list_keeps_empty_items)
{
    static const char *const expected[] = { "a", "", "b" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", "a,,b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.keys, expected, ARRAY_LEN(expected));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_list_lone_delimiter_gives_two_empty_items)
{
    static const char *const expected[] = { "", "" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", ",", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.keys, expected, ARRAY_LEN(expected));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_value_last_occurrence_wins)
{
    char *argv[] = { "tool", "build", "x", "--output", "a", "--output", "b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[6]);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "b");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_count_nine_times_is_clamped_to_eight)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-j", "-j", "-j", "-j",
                     "-j", "-j", "-j", "-j", "-j", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.jobs), 8);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.jobs), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_flag_then_negation_is_false)
{
    char *argv[] = { "tool", "--color", "--no-color", "cat", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_negation_then_flag_is_true)
{
    char *argv[] = { "tool", "cat", "--no-color", "--color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_cat_tags_default_is_split)
{
    static const char *const expected[] = { "a", "b" };
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.tags, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.tags), CLOPTS_SOURCE_DEFAULT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_cat_tags_env_is_split)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x,,y" } };
    static const char *const expected[] = { "x", "", "y" };
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool_env(&f, argv, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.tags, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.tags), CLOPTS_SOURCE_ENV);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(parse_cat_tags_explicit_is_split_and_wins)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x,y" } };
    static const char *const expected[] = { "p", "q", "r" };
    char *argv[] = { "tool", "cat", "--tags", "p,q", "--tags=r", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool_env(&f, argv, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, f.tags, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.tags), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- サブコマンドなし構成 ---------------------------------------- */

/*
 * A program without subcommands. fixture_filt covers
 * long / short / "=" / positional mix / default / env / -h/--help; the
 * fictional program nosub below covers the rest: "--", "-" alone, --no-x,
 * the count limit, list concatenation and splitting, required checks, the
 * terminal --version and the order of the terminal options. It is assembled
 * with the builder for each test and released at the end of the test.
 *
 *   nosub        without subcommands
 *     --color negatable FLAG, -v/--verbose COUNT max 3, -k/--keys LIST ',',
 *     -o/--output VALUE required, -m/--mode VALUE (default "fast",
 *     env NOSUB_MODE), -h/--help and --version terminal FLAGs
 *     (test_decl_help_option() / test_decl_version_option()), SRC required,
 *     REST variadic
 */

typedef struct nosub
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of nosub_init() succeeded */
    clopts_option_t *color;
    clopts_option_t *verbose;
    clopts_option_t *keys;
    clopts_option_t *output;
    clopts_option_t *mode;
    const clopts_option_t *help;     /* -h/--help terminal FLAG */
    const clopts_option_t *version;  /* --version terminal FLAG */
    clopts_positional_t *src;
    clopts_positional_t *rest;
} nosub_t;

/* Assembles nosub into n (handles are NULL after a failed allocation). */
static void nosub_init(nosub_t *n)
{
    clopts_program_t *program;

    memset(n, 0, sizeof(*n));
    test_decl_reset();
    program = test_decl_begin("nosub");

    n->color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    test_decl_check(clopts_option_set_negatable(n->color, true));
    n->color = test_decl_global_attach(program, n->color);

    n->verbose = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    test_decl_check(clopts_option_set_count_max(n->verbose, 3));
    n->verbose = test_decl_global_attach(program, n->verbose);

    n->keys = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');
    test_decl_check(clopts_option_set_delimiter(n->keys, ','));
    n->keys = test_decl_global_attach(program, n->keys);

    n->output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    test_decl_check(clopts_option_set_required(n->output, true));
    n->output = test_decl_global_attach(program, n->output);

    n->mode = test_decl_option(CLOPTS_OPTION_VALUE, "mode", 'm');
    test_decl_check(clopts_option_set_default(n->mode, "fast"));
    test_decl_check(clopts_option_set_env(n->mode, "NOSUB_MODE"));
    n->mode = test_decl_global_attach(program, n->mode);

    n->src = test_decl_positional("SRC");
    clopts_positional_set_required(n->src, true);
    n->src = test_decl_global_attach_positional(program, n->src);

    n->rest = test_decl_positional("REST");
    clopts_positional_set_variadic(n->rest, true);
    n->rest = test_decl_global_attach_positional(program, n->rest);

    n->help = test_decl_global_attach(program, test_decl_help_option());
    n->version = test_decl_global_attach(program, test_decl_version_option());
    n->program = test_decl_finish(program);
    if (n->program == NULL)
    {
        /* Everything was released with the program. */
        memset(n, 0, sizeof(*n));
    }
    n->ok = (test_decl_failures == 0);
}

/* Releases the program of n and clears it. */
static void nosub_free(nosub_t *n)
{
    clopts_program_destroy(n->program);
    memset(n, 0, sizeof(*n));
}

/* Parses argv for n->program (already assembled) with the mock environment vars. */
static clopts_result_t *parse_nosub(nosub_t *n, char **argv, const fixture_env_var_t *vars,
                                    size_t var_count)
{
    NAOS_REQUIRE_NOT_NULL(n->program);
    NAOS_REQUIRE_TRUE(n->ok);
    return parse_checked(n->program, argv, vars, var_count);
}

/* Checks a usage error of nosub: kind, no command, option / positional, index. */
static void check_nosub_usage(const clopts_result_t *result, char **argv,
                              clopts_usage_error_t usage, const clopts_option_t *option,
                              const clopts_positional_t *positional, int index)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), usage);
    NAOS_CHECK_NULL(clopts_result_get_error_command(result));
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), option);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_positional(result), positional);
    NAOS_CHECK_INT_EQ(clopts_result_get_error_argv_index(result), index);
    if (index < 0)
    {
        NAOS_CHECK_NULL(clopts_result_get_error_token(result));
    }
    else
    {
        NAOS_CHECK_PTR_EQ(clopts_result_get_error_token(result), argv[index]);
    }
}

NAOS_TEST(nosub_declaration_is_valid)
{
    nosub_t n;

    nosub_init(&n);
    NAOS_REQUIRE_NOT_NULL(n.program);
    NAOS_REQUIRE_TRUE(n.ok);
    NAOS_CHECK_STR_EQ(clopts_program_get_name(n.program), "nosub");
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(n.program)), 7);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(n.program), 5), n.help);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(n.program), 6), n.version);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(n.program)), 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(n.program), 0), n.src);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(n.program), 1), n.rest);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(n.program), 0);
    NAOS_CHECK_TRUE(clopts_option_is_terminal(n.help));
    NAOS_CHECK_TRUE(clopts_option_is_terminal(n.version));
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(n.verbose), 3);
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(n.keys), ',');
    NAOS_CHECK_TRUE(clopts_option_is_required(n.output));
    NAOS_CHECK_TRUE(clopts_option_is_negatable(n.color));
    NAOS_CHECK_TRUE(clopts_positional_is_required(n.src));
    NAOS_CHECK_TRUE(clopts_positional_is_variadic(n.rest));
    nosub_free(&n);
}

NAOS_TEST(nosub_dashdash_makes_the_rest_positional)
{
    char *argv[] = { "nosub", "-o", "out", "--", "-v", "--color", "-", "--version", "-h", NULL };
    char *expected[] = { argv[4], argv[5], argv[6], argv[7], argv[8] };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    check_positionals(result, expected, ARRAY_LEN(expected));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, n.verbose), 0);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, n.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.color), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_lone_dash_is_positional)
{
    char *argv[] = { "nosub", "-", "-o", "out", "-", NULL };
    char *expected[] = { argv[1], argv[4] };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_positionals(result, expected, ARRAY_LEN(expected));
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, n.output), argv[3]);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_negation_last_occurrence_wins)
{
    char *argv_off[] = { "nosub", "--color", "-o", "out", "--no-color", "src", NULL };
    char *argv_on[] = { "nosub", "--no-color", "src", "--color", "-o", "out", NULL };
    char *argv_only_off[] = { "nosub", "--no-color", "-o", "out", "src", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv_off, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, n.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_on, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, n.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_only_off, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, n.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_negation_errors)
{
    char *argv_value[] = { "nosub", "-o", "out", "src", "--no-color=x", NULL };
    char *argv_not_negatable[] = { "nosub", "-o", "out", "--no-verbose", "src", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv_value, NULL, 0);
    check_nosub_usage(result, argv_value, CLOPTS_USAGE_UNEXPECTED_VALUE, n.color, NULL, 4);
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_not_negatable, NULL, 0);
    check_nosub_usage(result, argv_not_negatable, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, 3);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_count_is_clamped_to_count_max)
{
    char *argv[] = { "nosub", "-vv", "src", "-v", "--verbose", "-vvo", "out", "--verbose", NULL };
    char *argv_two[] = { "nosub", "-v", "-o", "out", "src", "--verbose", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    /* 2 + 1 + 1 + 2 + 1 = 7 occurrences, clamped to 3. */
    result = parse_nosub(&n, argv, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, n.verbose), 3);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.verbose), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, n.output), argv[6]);
    clopts_result_destroy(result);

    /* Below the limit the occurrences are counted as they are. */
    result = parse_nosub(&n, argv_two, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, n.verbose), 2);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_list_is_concatenated_and_split)
{
    static const char *const expected[] = { "a", "", "b", "", "", "c", "d", "" };
    char *argv[] = { "nosub", "-k", "a,,b", "--keys=,", "src", "-kc", "--keys", "d", "-o", "out",
                     "--keys=", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_items(result, n.keys, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.keys), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_unset_options_are_empty_and_default_applies)
{
    char *argv[] = { "nosub", "-o", "out", "src", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_list_count(result, n.keys), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.keys), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, n.verbose), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.verbose), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, n.mode), clopts_option_get_default(n.mode));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, n.mode), "fast");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.mode), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_env_fallback_and_source)
{
    static const fixture_env_var_t vars[] = { { "NOSUB_MODE", "slow" } };
    static const fixture_env_var_t empty[] = { { "NOSUB_MODE", "" } };
    char *argv[] = { "nosub", "-o", "out", "src", NULL };
    char *argv_explicit[] = { "nosub", "-o", "out", "src", "--mode=eager", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, n.mode), "slow");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.mode), CLOPTS_SOURCE_ENV);
    clopts_result_destroy(result);

    /* An empty variable is still set and wins over the default. */
    result = parse_nosub(&n, argv, empty, ARRAY_LEN(empty));
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, n.mode), "");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.mode), CLOPTS_SOURCE_ENV);
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_explicit, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, n.mode), argv_explicit[4] + 7);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, n.mode), "eager");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.mode), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_missing_required_option)
{
    char *argv[] = { "nosub", "src", "-v", NULL };
    char *argv_none[] = { "nosub", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv, NULL, 0);
    check_nosub_usage(result, argv, CLOPTS_USAGE_MISSING_OPTION, n.output, NULL, -1);
    clopts_result_destroy(result);

    /* Options are checked before positionals: the option is reported first. */
    result = parse_nosub(&n, argv_none, NULL, 0);
    check_nosub_usage(result, argv_none, CLOPTS_USAGE_MISSING_OPTION, n.output, NULL, -1);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_missing_required_positional)
{
    char *argv[] = { "nosub", "-o", "out", "-k", "a", NULL };
    char *argv_src_only[] = { "nosub", "-o", "out", "src", NULL };
    char *expected[] = { argv_src_only[3] };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv, NULL, 0);
    check_nosub_usage(result, argv, CLOPTS_USAGE_MISSING_ARGUMENT, NULL, n.src, -1);
    clopts_result_destroy(result);

    /* The optional variadic REST accepts zero tokens. */
    result = parse_nosub(&n, argv_src_only, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_positionals(result, expected, ARRAY_LEN(expected));
    clopts_result_destroy(result);
    nosub_free(&n);
}

/* Checks that result is TERMINATED by terminal (recorded as given), without a command. */
static void check_nosub_terminated(const clopts_result_t *result, const clopts_option_t *terminal)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), terminal);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, terminal));
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_NONE);
}

NAOS_TEST(nosub_version_terminates)
{
    char *argv[] = { "nosub", "--version", NULL };
    char *argv_late[] = { "nosub", "-o", "out", "src", "--version", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    /* Required option and positional are missing, but a terminal option skips the check. */
    result = parse_nosub(&n, argv, NULL, 0);
    check_nosub_terminated(result, n.version);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, n.help));
    clopts_result_destroy(result);

    /* The values before it are kept, and the defaults are resolved. */
    result = parse_nosub(&n, argv_late, NULL, 0);
    check_nosub_terminated(result, n.version);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, n.output), "out");
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, n.mode), "fast");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, n.mode), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_version_wins_over_an_earlier_usage_error)
{
    char *argv_unknown[] = { "nosub", "--bogus", "-o", "out", "src", "--version", NULL };
    char *argv_value[] = { "nosub", "--color=yes", "--version", NULL };
    char *argv_group[] = { "nosub", "-vx", "--version", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv_unknown, NULL, 0);
    check_nosub_terminated(result, n.version);
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_value, NULL, 0);
    check_nosub_terminated(result, n.version);
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_group, NULL, 0);
    check_nosub_terminated(result, n.version);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_version_consumed_as_value_or_after_dashdash_is_not_a_request)
{
    char *argv_value[] = { "nosub", "-o", "--version", "src", NULL };
    char *argv_dashdash[] = { "nosub", "-o", "out", "--", "--version", NULL };
    char *argv_error[] = { "nosub", "--bogus", "--", "--version", NULL };
    char *expected[] = { argv_dashdash[4] };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv_value, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, n.output), argv_value[2]);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, n.version));
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_dashdash, NULL, 0);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    check_positionals(result, expected, ARRAY_LEN(expected));
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_error, NULL, 0);
    check_nosub_usage(result, argv_error, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, 1);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_TEST(nosub_first_of_help_and_version_wins)
{
    char *argv_help_first[] = { "nosub", "--help", "--version", NULL };
    char *argv_version_first[] = { "nosub", "--version", "-h", NULL };
    char *argv_group_help[] = { "nosub", "-vh", "--version", NULL };
    char *argv_error_version[] = { "nosub", "--bogus", "--version", "--help", NULL };
    char *argv_error_help[] = { "nosub", "--bogus", "-h", "--version", NULL };
    nosub_t n;
    clopts_result_t *result;

    nosub_init(&n);
    result = parse_nosub(&n, argv_help_first, NULL, 0);
    check_nosub_terminated(result, n.help);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, n.version));
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_version_first, NULL, 0);
    check_nosub_terminated(result, n.version);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, n.help));
    clopts_result_destroy(result);

    /* -vh: v is counted, then h ends the scan. */
    result = parse_nosub(&n, argv_group_help, NULL, 0);
    check_nosub_terminated(result, n.help);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, n.verbose), 1);
    clopts_result_destroy(result);

    /* After a usage error the recovery scan also keeps the argv order. */
    result = parse_nosub(&n, argv_error_version, NULL, 0);
    check_nosub_terminated(result, n.version);
    clopts_result_destroy(result);

    result = parse_nosub(&n, argv_error_help, NULL, 0);
    check_nosub_terminated(result, n.help);
    clopts_result_destroy(result);
    nosub_free(&n);
}

NAOS_SUITE(values,
    NAOS_ADD_TEST(sentinel_values_are_unset),
    NAOS_ADD_TEST(fixture_list_delimiters_are_as_designed),
    NAOS_ADD_TEST(fixture_count_limits_are_as_designed),
    NAOS_ADD_TEST(parse_unset_options_read_as_empty),
    NAOS_ADD_TEST(parse_flag_repeated_stays_true),
    NAOS_ADD_TEST(parse_count_with_default_limit_counts_every_occurrence),
    NAOS_ADD_TEST(parse_value_keeps_the_string_as_is),
    NAOS_ADD_TEST(parse_list_keeps_whitespace),
    NAOS_ADD_TEST(parse_list_items_are_copies),
    NAOS_ADD_TEST(parse_short_list_repeats_are_concatenated),
    NAOS_ADD_TEST(parse_long_list_repeats_are_concatenated),
    NAOS_ADD_TEST(parse_list_keeps_empty_items),
    NAOS_ADD_TEST(parse_list_lone_delimiter_gives_two_empty_items),
    NAOS_ADD_TEST(parse_value_last_occurrence_wins),
    NAOS_ADD_TEST(parse_count_nine_times_is_clamped_to_eight),
    NAOS_ADD_TEST(parse_flag_then_negation_is_false),
    NAOS_ADD_TEST(parse_negation_then_flag_is_true),
    NAOS_ADD_TEST(parse_cat_tags_default_is_split),
    NAOS_ADD_TEST(parse_cat_tags_env_is_split),
    NAOS_ADD_TEST(parse_cat_tags_explicit_is_split_and_wins),
    NAOS_ADD_TEST(nosub_declaration_is_valid),
    NAOS_ADD_TEST(nosub_dashdash_makes_the_rest_positional),
    NAOS_ADD_TEST(nosub_lone_dash_is_positional),
    NAOS_ADD_TEST(nosub_negation_last_occurrence_wins),
    NAOS_ADD_TEST(nosub_negation_errors),
    NAOS_ADD_TEST(nosub_count_is_clamped_to_count_max),
    NAOS_ADD_TEST(nosub_list_is_concatenated_and_split),
    NAOS_ADD_TEST(nosub_unset_options_are_empty_and_default_applies),
    NAOS_ADD_TEST(nosub_env_fallback_and_source),
    NAOS_ADD_TEST(nosub_missing_required_option),
    NAOS_ADD_TEST(nosub_missing_required_positional),
    NAOS_ADD_TEST(nosub_version_terminates),
    NAOS_ADD_TEST(nosub_version_wins_over_an_earlier_usage_error),
    NAOS_ADD_TEST(nosub_version_consumed_as_value_or_after_dashdash_is_not_a_request),
    NAOS_ADD_TEST(nosub_first_of_help_and_version_wins)
)
