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

#include "parser/parser_internal.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <clopts/parse.h>

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
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
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
 *   shadow   root: -x/--root-x FLAG, -v/--verbose COUNT, -q/--quit terminal FLAG
 *     run:   -x/--run-x FLAG, -s/--stop terminal FLAG, -o/--out VALUE required
 */
typedef struct custom
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *run;
    clopts_option_t *root_x;   /* root: -x/--root-x FLAG */
    clopts_option_t *verbose;  /* root: -v/--verbose COUNT */
    clopts_option_t *quit;     /* root: -q/--quit terminal FLAG */
    clopts_option_t *run_x;    /* run:  -x/--run-x FLAG */
    clopts_option_t *stop;     /* run:  -s/--stop terminal FLAG */
    clopts_option_t *out;      /* run:  -o/--out VALUE required */
} custom_t;

/* Attaches a terminal FLAG to command (NULL on failure). */
static clopts_option_t *custom_terminal(clopts_command_t *command, const char *long_name, char short_name)
{
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, long_name, short_name);

    test_decl_check(clopts_option_set_terminal(option, true));
    return test_decl_attach(command, option);
}

/* Builds the custom program into c (every builder call must succeed). */
static void custom_init(custom_t *c)
{
    memset(c, 0, sizeof(*c));
    test_decl_reset();
    c->root = test_decl_root("shadow");
    c->root_x = test_decl_add(c->root, CLOPTS_OPTION_FLAG, "root-x", 'x');
    c->verbose = test_decl_add(c->root, CLOPTS_OPTION_COUNT, "verbose", 'v');
    c->quit = custom_terminal(c->root, "quit", 'q');

    c->run = test_decl_command(c->root, "run");
    c->run_x = test_decl_add(c->run, CLOPTS_OPTION_FLAG, "run-x", 'x');
    c->stop = custom_terminal(c->run, "stop", 's');
    c->out = test_decl_option(CLOPTS_OPTION_VALUE, "out", 'o');
    test_decl_check(clopts_option_set_required(c->out, true));
    c->out = test_decl_attach(c->run, c->out);

    c->program = test_decl_finish(c->root);
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
static void check_short_error(const clopts_result_t *result, clopts_usage_error_t usage,
                              const clopts_command_t *command, const clopts_option_t *option,
                              char **argv, int index, char short_name)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), usage);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), command);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), option);
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[index]);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), index);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), short_name);
}

/* ---- sentinel ----------------------------------------------------------- */

NAOS_TEST(sentinel_has_no_short_name)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);
    clopts_result_free(result);
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
    NAOS_CHECK_INT_EQ(t.verbose->short_name, 'v');
    NAOS_CHECK_INT_EQ(t.color->short_name, '\0');
    NAOS_CHECK_INT_EQ(t.config->short_name, '\0');

    NAOS_CHECK_INT_EQ(t.output->short_name, 'o');
    NAOS_CHECK_INT_EQ(t.keys->short_name, 'k');
    NAOS_CHECK_INT_EQ(t.jobs->short_name, 'j');
    NAOS_CHECK_INT_EQ(t.jobs->kind, CLOPTS_OPTION_COUNT);
    NAOS_CHECK_UINT_EQ(t.jobs->count_max, 8);

    NAOS_CHECK_INT_EQ(t.tags->short_name, '\0');
    NAOS_CHECK_INT_EQ(t.help->short_name, 'h');
    NAOS_CHECK_TRUE(t.help->terminal);
    NAOS_CHECK_INT_EQ(t.version->short_name, '\0');
    NAOS_CHECK_TRUE(t.version->terminal);
    fixture_tool_free(&t);

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_INT_EQ(f.lines->short_name, 'n');
    fixture_filt_free(&f);

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    NAOS_CHECK_INT_EQ(s.token->short_name, '\0');
    fixture_strict_free(&s);
}

/* ---- groups of FLAG / COUNT (6.8, 7.6, 7.7) --------------------------- */

NAOS_TEST(group_counts_each_character)
{
    char *argv[] = { "tool", "build", "x", "-vvv", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 3);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.verbose), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[5]);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_is_clamped_to_its_limit)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-j", "-j", "-j",
                     "-j", "-j", "-j", "-j", "-j", "-j", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.jobs), 8);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_in_a_group_is_clamped_to_its_limit)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-jjjjjjjjj", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.jobs), 8);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(group_mixes_global_and_command_options)
{
    char *argv[] = { "tool", "build", "x", "-vjv", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 2);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.jobs), 1);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

/* ---- global options before and after the command (5.3, 5.4) ----------- */

NAOS_TEST(global_short_before_the_command)
{
    char *argv[] = { "tool", "-v", "build", "x", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[5]);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_short_after_the_command)
{
    char *argv[] = { "tool", "build", "-v", "x", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[5]);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "x");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_group_without_a_command_is_no_command)
{
    char *argv[] = { "tool", "-vv", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 2);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(command_short_before_the_command_is_unknown)
{
    char *argv[] = { "tool", "-o", "y", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, 'o');
    clopts_result_free(result);
    fixture_tool_free(&f);
}

/* ---- VALUE / LIST: attached value or next token (6.9, 6.10, 7.9) ------ */

NAOS_TEST(value_takes_the_next_token)
{
    char *argv[] = { "tool", "build", "x", "-vo", "file", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[4]);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_takes_the_rest_of_the_group)
{
    char *argv[] = { "tool", "build", "x", "-vofile", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "file");
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[3] + 3);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_keeps_the_equals_sign)
{
    char *argv[] = { "tool", "build", "x", "-o=file", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "=file");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(rest_of_the_group_after_a_value_is_not_options)
{
    char *argv[] = { "tool", "build", "x", "-ovh", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "vh");
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_consumes_dash_h_verbatim)
{
    char *argv[] = { "tool", "build", "x", "-o", "-h", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[4]);
    clopts_result_free(result);
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
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.jobs), 1);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, f.keys), 1);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 0), "4");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_followed_by_a_digit_is_an_unknown_option)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-vj4", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, f.build, NULL, argv, 5, '4');
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_concatenates_repeated_shorts)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", "a,b", "-k", "c", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, f.keys), 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 2), "c");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.keys), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(program_without_subcommands_attached_value)
{
    char *argv[] = { "filt", "-n5", "a", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.lines), CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "a");
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(program_without_subcommands_next_token_value)
{
    char *argv[] = { "filt", "-n", "5", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.lines), argv[2]);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "5");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

/* ---- errors: unknown character and missing value (6.4, 6.11) ---------- */

NAOS_TEST(unknown_character_in_a_group)
{
    char *argv[] = { "tool", "build", "x", "-vx", "-o", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, f.build, NULL, argv, 3, 'x');
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(trailing_value_option_is_a_missing_value)
{
    char *argv[] = { "tool", "build", "x", "-o", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_MISSING_VALUE, f.build, f.output, argv, 3, 'o');
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(trailing_value_option_in_a_group_is_a_missing_value)
{
    char *argv[] = { "tool", "build", "x", "-vo", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_MISSING_VALUE, f.build, f.output, argv, 3, 'o');
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(trailing_value_option_without_subcommands_is_a_missing_value)
{
    char *argv[] = { "filt", "a", "-n", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_short_error(result, CLOPTS_USAGE_MISSING_VALUE, NULL, f.lines, argv, 2, 'n');
    clopts_result_free(result);
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
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), f.help);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.help));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_NONE);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(h_in_a_group_before_the_command_terminates_without_a_command)
{
    char *argv[] = { "tool", "-vh", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), f.help);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(h_after_the_command_terminates_in_the_command)
{
    char *argv[] = { "tool", "build", "-h", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), f.help);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(h_stops_the_group_without_subcommands)
{
    /* -hn: the scan stops at h, so n is not read (no missing value). */
    char *argv[] = { "filt", "-hn", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), f.help);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "10");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.lines), CLOPTS_SOURCE_DEFAULT);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

/* ---- custom program: lookup order and terminal options in a group ----- */

NAOS_TEST(custom_command_short_shadows_the_root_short)
{
    /* -x before the command is the root's; after it, the command's -x shadows the root's. */
    char *argv[] = { "shadow", "-x", "run", "-xv", "-o", "a", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_flag(result, c.root_x));
    NAOS_CHECK_TRUE(clopts_result_flag(result, c.run_x));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, c.run_x), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, c.verbose), 1);
    clopts_result_free(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_command_short_shadow_leaves_the_root_unset)
{
    /* -x after the command is only the command's. */
    char *argv[] = { "shadow", "run", "-x", "-o", "a", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, c.run_x));
    NAOS_CHECK_FALSE(clopts_result_flag(result, c.root_x));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, c.root_x), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_root_short_is_reached_from_the_command)
{
    /* The command has no -v of its own: -v is looked up in the root. */
    char *argv[] = { "shadow", "run", "-o", "a", "-vv", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, c.verbose), 2);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, c.out), "a");
    clopts_result_free(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_command_terminal_stops_the_group)
{
    /* -vsx: v counted, s terminal; x is not read and the required -o is not checked. */
    char *argv[] = { "shadow", "run", "-vsx", "-o", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), c.stop);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_flag(result, c.stop));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, c.verbose), 1);
    NAOS_CHECK_FALSE(clopts_result_flag(result, c.run_x));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, c.run_x), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(clopts_result_value(result, c.out));
    clopts_result_free(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_root_terminal_in_a_group_after_the_command)
{
    /* -xqv after the command: x is the command's, q the root's terminal, v is not read. */
    char *argv[] = { "shadow", "run", "-xqv", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), c.quit);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), c.run);
    NAOS_CHECK_TRUE(clopts_result_flag(result, c.quit));
    NAOS_CHECK_TRUE(clopts_result_flag(result, c.run_x));
    NAOS_CHECK_FALSE(clopts_result_flag(result, c.root_x));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, c.verbose), 0);
    clopts_result_free(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_command_terminal_before_the_command_is_unknown)
{
    /* -s belongs to run only: before the command it is unknown (no recovery: -q is not given). */
    char *argv[] = { "shadow", "-s", "run", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    check_short_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, 's');
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    clopts_result_free(result);
    clopts_program_destroy(c.program);
}

NAOS_TEST(custom_terminal_after_a_value_in_a_group_is_the_value)
{
    /* -os: the rest of the group is the value of -o, so s is not a terminal option here. */
    char *argv[] = { "shadow", "run", "-os", NULL };
    custom_t c;
    clopts_result_t *result;

    result = parse_custom(&c, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, c.out), "s");
    NAOS_CHECK_FALSE(clopts_result_flag(result, c.stop));
    clopts_result_free(result);
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
    NAOS_ADD_TEST(trailing_value_option_is_a_missing_value),
    NAOS_ADD_TEST(trailing_value_option_in_a_group_is_a_missing_value),
    NAOS_ADD_TEST(trailing_value_option_without_subcommands_is_a_missing_value),
    NAOS_ADD_TEST(h_in_a_group_after_the_command_counts_v_then_terminates),
    NAOS_ADD_TEST(h_in_a_group_before_the_command_terminates_without_a_command),
    NAOS_ADD_TEST(h_after_the_command_terminates_in_the_command),
    NAOS_ADD_TEST(h_stops_the_group_without_subcommands),
    NAOS_ADD_TEST(custom_command_short_shadows_the_root_short),
    NAOS_ADD_TEST(custom_command_short_shadow_leaves_the_root_unset),
    NAOS_ADD_TEST(custom_root_short_is_reached_from_the_command),
    NAOS_ADD_TEST(custom_command_terminal_stops_the_group),
    NAOS_ADD_TEST(custom_root_terminal_in_a_group_after_the_command),
    NAOS_ADD_TEST(custom_command_terminal_before_the_command_is_unknown),
    NAOS_ADD_TEST(custom_terminal_after_a_value_in_a_group_is_the_value)
)
