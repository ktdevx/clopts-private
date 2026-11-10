/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite short: short option groups, attached values, terminal options in a
 * group (-h of the fixtures and those of a custom program), the lookup order
 * of short names and unknown short options.
 *
 * Ported from the first-version short suite onto the builder fixtures of
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
 * A custom program for the terminal options and the lookup order of short
 * names (fictional):
 *
 *   shadow   global: -x/--global-x FLAG, -v/--verbose COUNT, -q/--quit terminal FLAG
 *     run:     -x/--run-x FLAG, -s/--stop terminal FLAG, -o/--out VALUE required
 */
typedef struct custom
{
    clopts_program_t *program;
    clopts_command_t *run;
    clopts_option_t *global_x;  /* global: -x/--global-x FLAG */
    clopts_option_t *verbose;   /* global: -v/--verbose COUNT */
    clopts_option_t *quit;      /* global: -q/--quit terminal FLAG */
    clopts_option_t *run_x;     /* run:    -x/--run-x FLAG */
    clopts_option_t *stop;      /* run:    -s/--stop terminal FLAG */
    clopts_option_t *out;       /* run:    -o/--out VALUE required */
} custom_t;

/* Makes an unattached terminal FLAG (NULL on failure). */
static clopts_option_t *custom_terminal(const char *long_name, char short_name)
{
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, long_name, short_name);

    test_decl_check(clopts_option_set_terminal(option, true));
    return option;
}

/* Builds the custom program into c (every builder call must succeed). */
static void custom_init(custom_t *c)
{
    clopts_program_t *program;

    memset(c, 0, sizeof(*c));
    test_decl_reset();
    program = test_decl_begin("shadow");
    c->global_x = test_decl_global_add(program, CLOPTS_OPTION_FLAG, "global-x", 'x');
    c->verbose = test_decl_global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    c->quit = test_decl_global_attach(program, custom_terminal("quit", 'q'));

    c->run = test_decl_command(program, "run");
    c->run_x = test_decl_add(c->run, CLOPTS_OPTION_FLAG, "run-x", 'x');
    c->stop = test_decl_attach(c->run, custom_terminal("stop", 's'));
    c->out = test_decl_option(CLOPTS_OPTION_VALUE, "out", 'o');
    test_decl_check(clopts_option_set_required(c->out, true));
    c->out = test_decl_attach(c->run, c->out);

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
 * Checks a usage error of kind usage on argv[index] for the short option
 * character short_name, naming option (may be NULL), with command as the
 * error command.
 */
static void check_short_error(const clopts_result_t *result, clopts_usage_error_kind_t usage,
                              const clopts_command_t *command, const clopts_option_t *option,
                              char **argv, int index, char short_name)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_kind(clopts_result_get_usage_error(result)), usage);
    NAOS_CHECK_PTR_EQ(clopts_usage_error_get_command(clopts_result_get_usage_error(result)), command);
    NAOS_CHECK_PTR_EQ(clopts_usage_error_get_option(clopts_result_get_usage_error(result)), option);
    NAOS_CHECK_NULL(clopts_usage_error_get_positional(clopts_result_get_usage_error(result)));
    NAOS_CHECK_PTR_EQ(clopts_usage_error_get_token(clopts_result_get_usage_error(result)), argv[index]);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_argv_index(clopts_result_get_usage_error(result)), index);
    NAOS_CHECK_UINT_EQ(clopts_usage_error_get_name_len(clopts_result_get_usage_error(result)), 0);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_short_name(clopts_result_get_usage_error(result)), short_name);
}

/* ---- sentinel ----------------------------------------------------------- */

NAOS_TEST(sentinel_has_no_short_name)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_short_name(clopts_result_get_usage_error(result)), '\0');
    NAOS_CHECK_UINT_EQ(clopts_usage_error_get_name_len(clopts_result_get_usage_error(result)), 0);
    clopts_result_destroy(result);
}

/* ---- fixture ------------------------------------------------------------ */

NAOS_TEST(fixture_short_names_are_as_designed)
{
    fixture_tool_t t;
    fixture_filt_t f;
    fixture_strict_t s;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.verbose), 'v');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.color), '\0');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.config), '\0');

    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.output), 'o');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.keys), 'k');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.jobs), 'j');
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(t.jobs), CLOPTS_OPTION_COUNT);
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(t.jobs), 8);

    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.tags), '\0');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.help), 'h');
    NAOS_CHECK_TRUE(clopts_option_is_terminal(t.help));
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(t.version), '\0');
    NAOS_CHECK_TRUE(clopts_option_is_terminal(t.version));
    fixture_tool_free(&t);

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(f.lines), 'n');
    fixture_filt_free(&f);

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(s.token), '\0');
    fixture_strict_free(&s);
}

/* ---- groups of FLAG / COUNT (6.8, 7.6, 7.7) --------------------------- */

NAOS_TEST(group_counts_each_character)
{
    char *argv[] = { "tool", "build", "x", "-vvv", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 3);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.verbose), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[5]);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_is_clamped_to_its_limit)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-j", "-j", "-j",
                     "-j", "-j", "-j", "-j", "-j", "-j", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.jobs), 8);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_in_a_group_is_clamped_to_its_limit)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-jjjjjjjjj", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.jobs), 8);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(group_mixes_global_and_command_options)
{
    char *argv[] = { "tool", "build", "x", "-vjv", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 2);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.jobs), 1);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- global options before and after the command (5.3, 5.4) ----------- */

NAOS_TEST(global_short_before_the_command)
{
    char *argv[] = { "tool", "-v", "build", "x", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[5]);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_short_after_the_command)
{
    char *argv[] = { "tool", "build", "-v", "x", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[5]);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "x");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_group_without_a_command_is_no_command)
{
    char *argv[] = { "tool", "-vv", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 2);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(command_short_before_the_command_is_unknown)
{
    char *argv[] = { "tool", "-o", "y", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, NULL, NULL, argv, 1, 'o');
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

/* ---- VALUE / LIST: attached value or next token (6.9, 6.10, 7.9) ------ */

NAOS_TEST(value_takes_the_next_token)
{
    char *argv[] = { "tool", "build", "x", "-vo", "file", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[4]);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_takes_the_rest_of_the_group)
{
    char *argv[] = { "tool", "build", "x", "-vofile", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "file");
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[3] + 3);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_keeps_the_equals_sign)
{
    char *argv[] = { "tool", "build", "x", "-o=file", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "=file");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(rest_of_the_group_after_a_value_is_not_options)
{
    char *argv[] = { "tool", "build", "x", "-ovh", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "vh");
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 0);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_consumes_dash_h_verbatim)
{
    char *argv[] = { "tool", "build", "x", "-o", "-h", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.output), argv[4]);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_attached_after_a_count_in_a_group)
{
    /*
     * The design's "-vj4" item assumes a value option j; in the fixture j is
     * a COUNT, so the attached value case uses the LIST k and "-vj4" is an
     * unknown option on '4' (see the test below).
     */
    char *argv[] = { "tool", "build", "x", "-o", "y", "-vjk4", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.jobs), 1);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, f.keys), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 0), "4");
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_followed_by_a_digit_is_an_unknown_option)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-vj4", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.build, NULL, argv, 5, '4');
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_concatenates_repeated_shorts)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", "a,b", "-k", "c", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, f.keys), 3);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, f.keys, 2), "c");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.keys), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(program_without_subcommands_attached_value)
{
    char *argv[] = { "filt", "-n5", "a", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.lines), CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_positional_at(result, 0), "a");
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

NAOS_TEST(program_without_subcommands_next_token_value)
{
    char *argv[] = { "filt", "-n", "5", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.lines), argv[2]);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.lines), "5");
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 0);
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

/* ---- errors: unknown character and missing value (6.4, 6.11) ---------- */

NAOS_TEST(unknown_character_in_a_group)
{
    char *argv[] = { "tool", "build", "x", "-vx", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.build, NULL, argv, 3, 'x');
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(short_name_with_other_case_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "-V", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, f.build, NULL, argv, 3, 'V');
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(trailing_value_option_is_a_missing_value)
{
    char *argv[] = { "tool", "build", "x", "-o", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_ERROR_MISSING_VALUE, f.build, f.output, argv, 3, 'o');
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(trailing_value_option_in_a_group_is_a_missing_value)
{
    char *argv[] = { "tool", "build", "x", "-vo", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_ERROR_MISSING_VALUE, f.build, f.output, argv, 3, 'o');
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(trailing_value_option_without_subcommands_is_a_missing_value)
{
    char *argv[] = { "filt", "a", "-n", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_ERROR_MISSING_VALUE, NULL, f.lines, argv, 2, 'n');
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

/* ---- -h: the terminal FLAG of the fixtures (10.1) --------------------- */

NAOS_TEST(h_in_a_group_after_the_command_counts_v_then_terminates)
{
    /* -vh: v is counted, then h is terminal; the required --output is not checked. */
    char *argv[] = { "tool", "build", "x", "-vh", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.help);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.help));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_kind(clopts_result_get_usage_error(result)), CLOPTS_USAGE_ERROR_NONE);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(h_in_a_group_before_the_command_terminates_without_a_command)
{
    char *argv[] = { "tool", "-vh", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.help);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, f.verbose), 1);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(h_after_the_command_terminates_in_the_command)
{
    char *argv[] = { "tool", "build", "-h", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.help);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    clopts_result_destroy(result);
    fixture_tool_free(&f);
}

NAOS_TEST(h_stops_the_group_without_subcommands)
{
    /* -hn: the scan stops at h, so n is not read (no missing value). */
    char *argv[] = { "filt", "-hn", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), f.help);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.lines), "10");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.lines), CLOPTS_SOURCE_DEFAULT);
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

/* ---- custom program: lookup order and terminal options in a group ----- */

NAOS_TEST(custom_command_short_shadows_the_global_short)
{
    /* -x before the command is the global one; after it, the command's -x shadows it. */
    char *argv[] = { "shadow", "-x", "run", "-xv", "-o", "a", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.global_x));
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.run_x));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.run_x), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, c.verbose), 1);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_command_short_shadow_leaves_the_global_unset)
{
    /* -x after the command is only the command's. */
    char *argv[] = { "shadow", "run", "-x", "-o", "a", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.run_x));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.global_x));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.global_x), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_global_short_is_reached_from_the_command)
{
    /* The command has no -v of its own: -v is looked up in the program. */
    char *argv[] = { "shadow", "run", "-o", "a", "-vv", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, c.verbose), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, c.out), "a");
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_command_terminal_stops_the_group)
{
    /* -vsx: v counted, s terminal; x is not read and the required -o is not checked. */
    char *argv[] = { "shadow", "run", "-vsx", "-o", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), c.stop);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.stop));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, c.verbose), 1);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.run_x));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, c.run_x), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(clopts_result_get_value(result, c.out));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_global_terminal_in_a_group_after_the_command)
{
    /* -xqv after the command: x is the command's, q the global terminal, v is not read. */
    char *argv[] = { "shadow", "run", "-xqv", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), c.quit);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.quit));
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, c.run_x));
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.global_x));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, c.verbose), 0);
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_command_terminal_before_the_command_is_unknown)
{
    /* -s belongs to run only: before the command it is unknown (no recovery: -q is not given). */
    char *argv[] = { "shadow", "-s", "run", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    check_short_error(result, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, NULL, NULL, argv, 1, 's');
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_terminal_after_a_value_in_a_group_is_the_value)
{
    /* -os: the rest of the group is the value of -o, so s is not a terminal option here. */
    char *argv[] = { "shadow", "run", "-os", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, c.out), "s");
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, c.stop));
    clopts_result_destroy(result);
    clopts_program_destroy(c.program);
}

NAOS_SUITE(short,
    NAOS_ADD_TEST(sentinel_has_no_short_name),
    NAOS_ADD_TEST(fixture_short_names_are_as_designed),
    NAOS_ADD_TEST(group_counts_each_character),
    NAOS_ADD_TEST(count_is_clamped_to_its_limit),
    NAOS_ADD_TEST(count_in_a_group_is_clamped_to_its_limit),
    NAOS_ADD_TEST(group_mixes_global_and_command_options),
    NAOS_ADD_TEST(global_short_before_the_command),
    NAOS_ADD_TEST(global_short_after_the_command),
    NAOS_ADD_TEST(global_group_without_a_command_is_no_command),
    NAOS_ADD_TEST(command_short_before_the_command_is_unknown),
    NAOS_ADD_TEST(value_takes_the_next_token),
    NAOS_ADD_TEST(value_takes_the_rest_of_the_group),
    NAOS_ADD_TEST(value_keeps_the_equals_sign),
    NAOS_ADD_TEST(rest_of_the_group_after_a_value_is_not_options),
    NAOS_ADD_TEST(value_consumes_dash_h_verbatim),
    NAOS_ADD_TEST(value_attached_after_a_count_in_a_group),
    NAOS_ADD_TEST(count_followed_by_a_digit_is_an_unknown_option),
    NAOS_ADD_TEST(list_concatenates_repeated_shorts),
    NAOS_ADD_TEST(program_without_subcommands_attached_value),
    NAOS_ADD_TEST(program_without_subcommands_next_token_value),
    NAOS_ADD_TEST(unknown_character_in_a_group),
    NAOS_ADD_TEST(short_name_with_other_case_is_unknown),
    NAOS_ADD_TEST(trailing_value_option_is_a_missing_value),
    NAOS_ADD_TEST(trailing_value_option_in_a_group_is_a_missing_value),
    NAOS_ADD_TEST(trailing_value_option_without_subcommands_is_a_missing_value),
    NAOS_ADD_TEST(h_in_a_group_after_the_command_counts_v_then_terminates),
    NAOS_ADD_TEST(h_in_a_group_before_the_command_terminates_without_a_command),
    NAOS_ADD_TEST(h_after_the_command_terminates_in_the_command),
    NAOS_ADD_TEST(h_stops_the_group_without_subcommands),
    NAOS_ADD_TEST(custom_command_short_shadows_the_global_short),
    NAOS_ADD_TEST(custom_command_short_shadow_leaves_the_global_unset),
    NAOS_ADD_TEST(custom_global_short_is_reached_from_the_command),
    NAOS_ADD_TEST(custom_command_terminal_stops_the_group),
    NAOS_ADD_TEST(custom_global_terminal_in_a_group_after_the_command),
    NAOS_ADD_TEST(custom_command_terminal_before_the_command_is_unknown),
    NAOS_ADD_TEST(custom_terminal_after_a_value_in_a_group_is_the_value)
)
