/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite long: long options, --name=value, value consumption, unknown long
 * options, --no-<name>, global and command scope, the terminal --help /
 * --version of the fixtures, and on a custom program the lookup order
 * (command exact, command negation, global exact, global negation), the
 * names that are not a negation, and terminal options declared on a command
 * or on the program.
 *
 * Ported from the first-version long suite onto the builder fixtures of
 * test_fixtures.c (handles instead of index constants) and the result handle
 * accessors. The status is always checked with NAOS_REQUIRE_INT_EQ before
 * values are read.
 */

#include "parse/result.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <clopts/parse/parse.h>

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Maximum number of argv elements parse_checked handles. */
#define MAX_ARGV 16

/*
 * Parses the NULL-terminated argv for program with an empty mock environment
 * and checks that the result is a parse result (not the sentinel) and that
 * neither the argv array nor the strings it points to changed.
 */
static clopts_result_t *parse_checked(clopts_program_t *program, char **argv)
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
    fixture_env_init(&env, NULL, 0);
    result = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, &env);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_TRUE(result != clopts_result_sentinel());
    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved[i]);
        NAOS_CHECK_STR_EQ(argv[i], text[i]);
    }
    NAOS_CHECK_NULL(argv[argc]);
    return result;
}

/* Builds the tool fixture into f and parses argv with it. */
static clopts_result_t *parse_tool(fixture_tool_t *f, char **argv)
{
    fixture_tool_init(f);
    NAOS_REQUIRE_NOT_NULL(f->program);
    NAOS_REQUIRE_TRUE(f->ok);
    return parse_checked(f->program, argv);
}

/* Builds the filt fixture into f and parses argv with it. */
static clopts_result_t *parse_filt(fixture_filt_t *f, char **argv)
{
    fixture_filt_init(f);
    NAOS_REQUIRE_NOT_NULL(f->program);
    NAOS_REQUIRE_TRUE(f->ok);
    return parse_checked(f->program, argv);
}

/*
 * A custom program for the lookup order and the terminal options (fictional):
 *
 *   shadow   global: --mode VALUE, --color negatable FLAG, --no-fast FLAG,
 *                    --dry FLAG, -q/--quit terminal FLAG
 *     run:   --mode VALUE, --no-color FLAG, --fast negatable FLAG,
 *            -s/--stop terminal FLAG, -o/--out VALUE required, ARG optional
 *     idle:  -r negatable FLAG without a long name
 *
 * Every global name but --dry and --quit is shadowed in run: --mode by the
 * same name (command exact over global exact), --no-color of the global
 * negation by run's --no-color (command exact over global negation), and the
 * global --no-fast by the negation of run's --fast (command negation over global
 * exact). idle shadows nothing.
 */
typedef struct custom
{
    clopts_program_t *program;
    clopts_command_t *run;
    clopts_command_t *idle;
    clopts_option_t *global_mode;     /* global: --mode VALUE */
    clopts_option_t *global_color;    /* global: --color negatable FLAG */
    clopts_option_t *global_no_fast;  /* global: --no-fast FLAG */
    clopts_option_t *global_dry;      /* global: --dry FLAG */
    clopts_option_t *quit;            /* global: -q/--quit terminal FLAG */
    clopts_option_t *run_mode;        /* run:    --mode VALUE */
    clopts_option_t *run_no_color;    /* run:    --no-color FLAG */
    clopts_option_t *run_fast;        /* run:    --fast negatable FLAG */
    clopts_option_t *stop;            /* run:    -s/--stop terminal FLAG */
    clopts_option_t *out;             /* run:    -o/--out VALUE required */
} custom_t;

/* Makes an unattached FLAG, terminal and / or negatable (NULL on failure). */
static clopts_option_t *custom_flag(const char *long_name, char short_name, bool terminal, bool negatable)
{
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, long_name, short_name);

    test_decl_check(clopts_option_set_terminal(option, terminal));
    test_decl_check(clopts_option_set_negatable(option, negatable));
    return option;
}

/* Builds the custom program into c (every builder call must succeed). */
static void custom_init(custom_t *c)
{
    clopts_program_t *program;

    memset(c, 0, sizeof(*c));
    test_decl_reset();
    program = test_decl_begin("shadow");
    c->global_mode = test_decl_global_add(program, CLOPTS_OPTION_VALUE, "mode", '\0');
    c->global_color = test_decl_global_attach(program, custom_flag("color", '\0', false, true));
    c->global_no_fast = test_decl_global_attach(program, custom_flag("no-fast", '\0', false, false));
    c->global_dry = test_decl_global_attach(program, custom_flag("dry", '\0', false, false));
    c->quit = test_decl_global_attach(program, custom_flag("quit", 'q', true, false));

    c->run = test_decl_command(program, "run");
    c->run_mode = test_decl_add(c->run, CLOPTS_OPTION_VALUE, "mode", '\0');
    c->run_no_color = test_decl_attach(c->run, custom_flag("no-color", '\0', false, false));
    c->run_fast = test_decl_attach(c->run, custom_flag("fast", '\0', false, true));
    c->stop = test_decl_attach(c->run, custom_flag("stop", 's', true, false));
    c->out = test_decl_option(CLOPTS_OPTION_VALUE, "out", 'o');
    test_decl_check(clopts_option_set_required(c->out, true));
    c->out = test_decl_attach(c->run, c->out);
    (void)test_decl_add_positional(c->run, "ARG");

    c->idle = test_decl_command(program, "idle");
    (void)test_decl_attach(c->idle, custom_flag(NULL, 'r', false, true));

    c->program = test_decl_finish(program);
    NAOS_REQUIRE_NOT_NULL(c->program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
}

/* Builds the custom program into c and parses argv with it. */
static clopts_result_t *parse_custom(custom_t *c, char **argv)
{
    custom_init(c);
    return parse_checked(c->program, argv);
}

/*
 * Checks a usage error of kind usage on argv[index] naming option (may be
 * NULL), with command as the error command and name_len as the name length.
 */
static void check_error(const clopts_result_t *result, clopts_usage_error_kind_t usage,
                        const clopts_command_t *command, const clopts_option_t *option,
                        char **argv, int index, size_t name_len)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_kind(clopts_result_get_usage_error(result)), usage);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), command);
    NAOS_CHECK_PTR_EQ(clopts_usage_error_get_option(clopts_result_get_usage_error(result)), option);
    NAOS_CHECK_NULL(clopts_usage_error_get_positional(clopts_result_get_usage_error(result)));
    NAOS_CHECK_PTR_EQ(clopts_usage_error_get_token(clopts_result_get_usage_error(result)), argv[index]);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_argv_index(clopts_result_get_usage_error(result)), index);
    NAOS_CHECK_UINT_EQ(clopts_usage_error_get_name_len(clopts_result_get_usage_error(result)), name_len);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_short_name(clopts_result_get_usage_error(result)), '\0');
}

/* ---- sentinel ----------------------------------------------------------- */

NAOS_TEST(parse_with_env_null_program_returns_sentinel)
{
    clopts_result_t *result;

    result = clopts_parse_with_env(NULL, 0, NULL, NULL, NULL);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_PTR_EQ(result, clopts_parse(NULL, 0, NULL));
    clopts_result_destroy(result);
}

/* ---- fixture ------------------------------------------------------------ */

NAOS_TEST(fixture_long_names_and_kinds_are_as_designed)
{
    fixture_tool_t t;
    fixture_filt_t f;
    fixture_strict_t s;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(t.verbose), "verbose");
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(t.verbose), CLOPTS_OPTION_COUNT);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(t.color), "color");
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(t.color), CLOPTS_OPTION_FLAG);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(t.config), "config");
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(t.config), CLOPTS_OPTION_VALUE);

    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(t.output), "output");
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(t.output), CLOPTS_OPTION_VALUE);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(t.keys), "keys");
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(t.keys), CLOPTS_OPTION_LIST);
    NAOS_CHECK_NULL(clopts_option_get_long_name(t.jobs));

    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(t.tags), "tags");
    fixture_tool_free(&t);

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(f.lines), "lines");
    fixture_filt_free(&f);

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(s.token), "token");
    fixture_strict_free(&s);
}

NAOS_TEST(fixture_only_color_is_negatable)
{
    fixture_tool_t t;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    NAOS_CHECK_TRUE(clopts_option_is_negatable(t.color));
    NAOS_CHECK_FALSE(clopts_option_is_negatable(t.verbose));
    NAOS_CHECK_FALSE(clopts_option_is_negatable(t.config));
    fixture_tool_free(&t);
}

/* ---- VALUE: --name value / --name=value (6.1 to 6.4, 7.8) ------------- */

NAOS_TEST(value_takes_the_next_token)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[4]);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "x");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_takes_a_next_token_starting_with_dash)
{
    char *argv[] = { "tool", "build", "x", "--output", "-y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[4]);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_takes_dashdash_and_help_verbatim)
{
    char *argv[] = { "tool", "build", "--output", "--", "x", "--keys", "--help", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "--");
    NAOS_REQUIRE_TRUE(clopts_result_get_list_count(result, f.keys) == 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 0), "--help");
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "x");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_after_equals_may_be_empty)
{
    char *argv[] = { "tool", "build", "x", "--output=", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_is_everything_after_the_first_equals)
{
    char *argv[] = { "tool", "build", "x", "--output=a=b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "a=b");
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[3] + strlen("--output="));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_with_equals_does_not_consume_the_next_token)
{
    char *argv[] = { "tool", "build", "--output=y", "x", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "x");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_at_the_end_is_missing_value)
{
    char *argv[] = { "tool", "build", "x", "--output", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_MISSING_VALUE, f.build, f.output, argv, 3, 0);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_value_at_the_end_before_a_command_is_missing_value)
{
    char *argv[] = { "tool", "--config", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_MISSING_VALUE, NULL, f.config, argv, 1, 0);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_value_before_a_command_consumes_a_command_name)
{
    char *argv[] = { "tool", "--config", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.config), argv[2]);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.config), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_given_twice_keeps_the_last)
{
    char *argv[] = { "tool", "build", "x", "--output", "a", "--output", "b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[6]);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "b");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(filt_lines_with_equals_and_a_positional)
{
    char *argv[] = { "filt", "--lines=5", "a", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.lines), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "a");
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

NAOS_TEST(filt_lines_with_the_next_token)
{
    char *argv[] = { "filt", "--lines", "5", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.lines), argv[2]);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.lines), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 0);
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

/* ---- LIST (7.9 to 7.11) ----------------------------------------------- */

NAOS_TEST(list_given_twice_concatenates_the_items)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys", "a,b", "--keys", "c", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_TRUE(clopts_result_get_list_count(result, f.keys) == 3);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 2), "c");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.keys), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_keeps_an_empty_item)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys", "a,,b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_TRUE(clopts_result_get_list_count(result, f.keys) == 3);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 1), "");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 2), "b");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_with_equals_splits_the_rest)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys=a,b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_TRUE(clopts_result_get_list_count(result, f.keys) == 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 1), "b");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_at_the_end_is_missing_value)
{
    char *argv[] = { "tool", "build", "x", "--keys", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_MISSING_VALUE, f.build, f.keys, argv, 3, 0);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- FLAG / COUNT (6.5, 7.1, 7.6) ------------------------------------- */

NAOS_TEST(flag_given_twice_is_true)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--color", "--color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_counts_each_long_occurrence)
{
    char *argv[] = { "tool", "--verbose", "build", "--verbose", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 2);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "build", "x", "--verbose=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, f.build, f.verbose, argv, 3, 0);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(flag_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "--color=yes", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, NULL, f.color, argv, 1, 0);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- unknown long options (6.6, 6.7) ---------------------------------- */

NAOS_TEST(prefix_of_a_name_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--out", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.build, NULL, argv, 3, strlen("--out"));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(name_with_other_case_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--Output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.build, NULL, argv, 3, strlen("--Output"));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(unknown_name_len_stops_at_equals)
{
    char *argv[] = { "tool", "build", "x", "--out=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.build, NULL, argv, 3, 5);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- --no-<name> (7.2 to 7.5) ----------------------------------------- */

NAOS_TEST(negation_sets_false_with_explicit_source)
{
    char *argv[] = { "tool", "--no-color", "build", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(flag_then_negation_is_false)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--color", "--no-color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_then_flag_is_true)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--no-color", "--color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_of_a_value_option_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--no-output", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.build, NULL, argv, 3,
                strlen("--no-output"));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_of_a_count_option_is_unknown)
{
    char *argv[] = { "tool", "--no-verbose", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, NULL, NULL, argv, 1,
                strlen("--no-verbose"));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "build", "x", "--no-color=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, f.build, f.color, argv, 3, 0);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- global and command scope (5.3, 5.4) ------------------------------ */

NAOS_TEST(global_count_before_the_command)
{
    char *argv[] = { "tool", "--verbose", "build", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.verbose), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_count_after_the_command)
{
    char *argv[] = { "tool", "build", "--verbose", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "x");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(command_option_before_the_command_is_unknown)
{
    /* 5.3: before the subcommand only global options are matched. */
    char *argv[] = { "tool", "--output", "y", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--output"));
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(other_command_option_is_unknown)
{
    char *argv[] = { "tool", "cat", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.cat, NULL, argv,
                2, strlen("--output"));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- --help / --version: the terminal FLAGs of the fixtures (10.1, 10.5, 10.6) */

NAOS_TEST(help_after_the_command_terminates_in_that_command)
{
    /* The required --output of build is not checked. */
    char *argv[] = { "tool", "build", "x", "--help", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.help);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.help));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.help), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.version));
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_NULL(clopts_result_get_usage_error(result));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(help_in_filt_stops_the_scan)
{
    /* The tokens after --help are not read: b is not a positional. */
    char *argv[] = { "filt", "a", "--help", "b", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.help);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "a");
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

NAOS_TEST(version_after_the_command_terminates_in_that_command)
{
    char *argv[] = { "tool", "build", "--version", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.version);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.version));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.help));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(first_terminal_wins)
{
    /* --version stops the scan, so the later --help is not read. */
    char *argv[] = { "tool", "--version", "--help", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.version);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.help));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(help_with_equals_is_unexpected_value_on_the_help_flag)
{
    char *argv[] = { "tool", "build", "x", "--help=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, f.build, f.help, argv, 3, 0);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(help_with_equals_before_the_command_is_unexpected_value)
{
    char *argv[] = { "tool", "--help=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, NULL, f.help, argv, 1, 0);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(version_with_equals_is_unexpected_value_on_the_version_flag)
{
    char *argv[] = { "tool", "--version=1", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, NULL, f.version, argv, 1, 0);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(version_without_a_version_option_is_unknown)
{
    /* filt declares no --version. */
    char *argv[] = { "filt", "--version", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--version"));
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

NAOS_TEST(version_with_equals_without_a_version_option_is_unknown)
{
    char *argv[] = { "filt", "--version=1", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--version"));
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

NAOS_TEST(help_consumed_as_a_value_is_not_help)
{
    char *argv[] = { "tool", "build", "x", "--output", "--help", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[4]);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.help));
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- custom program: lookup order -------------------------------------- */

NAOS_TEST(lookup_command_exact_shadows_global_exact)
{
    char *argv[] = { "shadow", "run", "--mode", "a", "-o", "b", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, c.run_mode), "a");
    NAOS_CHECK_NULL(clopts_result_get_value(result, c.global_mode));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.global_mode), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_command_exact_shadows_global_negation)
{
    /* --no-color is run's own FLAG, not the negation of the global --color. */
    char *argv[] = { "shadow", "run", "--no-color", "-o", "b", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.run_no_color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.global_color), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_command_negation_shadows_global_exact)
{
    /* --no-fast is the negation of run's --fast, not the global --no-fast. */
    char *argv[] = { "shadow", "run", "--fast", "--no-fast", "-o", "b", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.run_fast));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.run_fast), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.global_no_fast));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.global_no_fast), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_global_exact_and_negation_when_the_command_has_no_such_name)
{
    /* idle shadows nothing: --no-fast is the global FLAG, --no-color the global negation. */
    char *argv[] = { "shadow", "idle", "--mode", "a", "--no-fast", "--color", "--no-color", "--dry", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), c.idle);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, c.global_mode), "a");
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.global_no_fast));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.global_color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.global_color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.global_dry));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_global_names_before_the_command)
{
    /* Before run is selected only the program is searched, so nothing is shadowed yet. */
    char *argv[] = { "shadow", "--mode", "a", "--no-color", "--no-fast", "run", "-o", "b", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), c.run);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, c.global_mode), "a");
    NAOS_CHECK_NULL(clopts_result_get_value(result, c.run_mode));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.global_color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.global_color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.run_no_color));
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.global_no_fast));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.run_fast), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_unshadowed_global_name_from_the_command)
{
    /* --dry is not in run: the global one is found. */
    char *argv[] = { "shadow", "run", "--dry", "-o", "b", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.global_dry));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_negation_of_a_flag_that_is_not_negatable_is_unknown)
{
    char *argv[] = { "shadow", "idle", "--no-dry", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, c.idle, NULL, argv, 2, strlen("--no-dry"));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_negatable_flag_without_a_long_name_has_no_negation)
{
    char *argv[] = { "shadow", "idle", "--no-r", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, c.idle, NULL, argv, 2, strlen("--no-r"));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(lookup_negation_is_exact_and_case_sensitive)
{
    /* --color of the program is negatable; none of these is its negation. */
    static const char *const tokens[] = { "--no-", "--No-color", "--no-Color", "--no-col", "--nocolor" };
    size_t i;

    for (i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
    {
        char token[16];
        char *argv[] = { "shadow", "idle", token, NULL };
        custom_t c;
        clopts_result_t *result;

        strcpy(token, tokens[i]);
        result = parse_custom(&c, argv);
        check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, c.idle, NULL, argv, 2, strlen(tokens[i]));
        NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.global_color), CLOPTS_SOURCE_NONE);
        clopts_result_destroy(result);
        clopts_program_destroy(c.program);
    }
}

/* ---- custom program: terminal options ---------------------------------- */

NAOS_TEST(terminal_of_the_selected_command)
{
    /* --stop: values given before it are kept, the rest is not read, -o is not required. */
    char *argv[] = { "shadow", "run", "--mode", "a", "--stop", "--mode", "b", "--bogus", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), c.stop);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.stop));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, c.run_mode), "a");
    NAOS_CHECK_NULL(clopts_result_get_value(result, c.out));
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_kind(clopts_result_get_usage_error(result)), CLOPTS_USAGE_ERROR_NONE);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(global_terminal_reached_from_a_command)
{
    /* --quit is global; the extra positionals after it are not read. */
    char *argv[] = { "shadow", "run", "--quit", "x", "y", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), c.quit);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.quit));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.stop));
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 0);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(global_terminal_before_the_command_has_no_command)
{
    char *argv[] = { "shadow", "--quit", "run", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), c.quit);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(command_terminal_in_another_command_is_unknown)
{
    /* --stop belongs to run only: in idle it is unknown. */
    char *argv[] = { "shadow", "idle", "--stop", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, c.idle, NULL, argv, 2, strlen("--stop"));
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(terminal_with_equals_is_unexpected_value_on_that_flag)
{
    char *argv[] = { "shadow", "run", "--stop=1", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, c.run, c.stop, argv, 2, 0);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(terminal_after_dashdash_is_a_positional)
{
    char *argv[] = { "shadow", "run", "-o", "b", "--", "--stop", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.stop));
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "--stop");
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_SUITE(long,
    NAOS_ADD_TEST(parse_with_env_null_program_returns_sentinel),
    NAOS_ADD_TEST(fixture_long_names_and_kinds_are_as_designed),
    NAOS_ADD_TEST(fixture_only_color_is_negatable),
    NAOS_ADD_TEST(value_takes_the_next_token),
    NAOS_ADD_TEST(value_takes_a_next_token_starting_with_dash),
    NAOS_ADD_TEST(value_takes_dashdash_and_help_verbatim),
    NAOS_ADD_TEST(value_after_equals_may_be_empty),
    NAOS_ADD_TEST(value_is_everything_after_the_first_equals),
    NAOS_ADD_TEST(value_with_equals_does_not_consume_the_next_token),
    NAOS_ADD_TEST(value_at_the_end_is_missing_value),
    NAOS_ADD_TEST(global_value_at_the_end_before_a_command_is_missing_value),
    NAOS_ADD_TEST(global_value_before_a_command_consumes_a_command_name),
    NAOS_ADD_TEST(value_given_twice_keeps_the_last),
    NAOS_ADD_TEST(filt_lines_with_equals_and_a_positional),
    NAOS_ADD_TEST(filt_lines_with_the_next_token),
    NAOS_ADD_TEST(list_given_twice_concatenates_the_items),
    NAOS_ADD_TEST(list_keeps_an_empty_item),
    NAOS_ADD_TEST(list_with_equals_splits_the_rest),
    NAOS_ADD_TEST(list_at_the_end_is_missing_value),
    NAOS_ADD_TEST(flag_given_twice_is_true),
    NAOS_ADD_TEST(count_counts_each_long_occurrence),
    NAOS_ADD_TEST(count_with_equals_is_unexpected_value),
    NAOS_ADD_TEST(flag_with_equals_is_unexpected_value),
    NAOS_ADD_TEST(prefix_of_a_name_is_unknown),
    NAOS_ADD_TEST(name_with_other_case_is_unknown),
    NAOS_ADD_TEST(unknown_name_len_stops_at_equals),
    NAOS_ADD_TEST(negation_sets_false_with_explicit_source),
    NAOS_ADD_TEST(flag_then_negation_is_false),
    NAOS_ADD_TEST(negation_then_flag_is_true),
    NAOS_ADD_TEST(negation_of_a_value_option_is_unknown),
    NAOS_ADD_TEST(negation_of_a_count_option_is_unknown),
    NAOS_ADD_TEST(negation_with_equals_is_unexpected_value),
    NAOS_ADD_TEST(global_count_before_the_command),
    NAOS_ADD_TEST(global_count_after_the_command),
    NAOS_ADD_TEST(command_option_before_the_command_is_unknown),
    NAOS_ADD_TEST(other_command_option_is_unknown),
    NAOS_ADD_TEST(help_after_the_command_terminates_in_that_command),
    NAOS_ADD_TEST(help_in_filt_stops_the_scan),
    NAOS_ADD_TEST(version_after_the_command_terminates_in_that_command),
    NAOS_ADD_TEST(first_terminal_wins),
    NAOS_ADD_TEST(help_with_equals_is_unexpected_value_on_the_help_flag),
    NAOS_ADD_TEST(help_with_equals_before_the_command_is_unexpected_value),
    NAOS_ADD_TEST(version_with_equals_is_unexpected_value_on_the_version_flag),
    NAOS_ADD_TEST(version_without_a_version_option_is_unknown),
    NAOS_ADD_TEST(version_with_equals_without_a_version_option_is_unknown),
    NAOS_ADD_TEST(help_consumed_as_a_value_is_not_help),
    NAOS_ADD_TEST(lookup_command_exact_shadows_global_exact),
    NAOS_ADD_TEST(lookup_command_exact_shadows_global_negation),
    NAOS_ADD_TEST(lookup_command_negation_shadows_global_exact),
    NAOS_ADD_TEST(lookup_global_exact_and_negation_when_the_command_has_no_such_name),
    NAOS_ADD_TEST(lookup_global_names_before_the_command),
    NAOS_ADD_TEST(lookup_unshadowed_global_name_from_the_command),
    NAOS_ADD_TEST(lookup_negation_of_a_flag_that_is_not_negatable_is_unknown),
    NAOS_ADD_TEST(lookup_negatable_flag_without_a_long_name_has_no_negation),
    NAOS_ADD_TEST(lookup_negation_is_exact_and_case_sensitive),
    NAOS_ADD_TEST(terminal_of_the_selected_command),
    NAOS_ADD_TEST(global_terminal_reached_from_a_command),
    NAOS_ADD_TEST(global_terminal_before_the_command_has_no_command),
    NAOS_ADD_TEST(command_terminal_in_another_command_is_unknown),
    NAOS_ADD_TEST(terminal_with_equals_is_unexpected_value_on_that_flag),
    NAOS_ADD_TEST(terminal_after_dashdash_is_a_positional)
)
