/*
 * Suite long: long options, --name=value, value consumption, unknown long
 * options, --no-<name>, global and command scope and --help / --version
 * after a subcommand.
 *
 * Ported from the first-version long suite onto the builder fixtures of
 * test_fixtures.c (handles instead of index constants) and the result handle
 * accessors. The status is always checked with NAOS_REQUIRE_INT_EQ before
 * values are read.
 */

#include "parser/parser_internal.h"
#include "test_fixtures.h"

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
    fixture_tool_init(f, NULL);
    NAOS_REQUIRE_NOT_NULL(f->program);
    return parse_checked(f->program, argv);
}

/* Builds the filt fixture into f and parses argv with it. */
static clopts_result_t *parse_filt(fixture_filt_t *f, char **argv)
{
    fixture_filt_init(f, NULL);
    NAOS_REQUIRE_NOT_NULL(f->program);
    return parse_checked(f->program, argv);
}

/*
 * Checks a usage error of kind usage on argv[index] naming option (may be
 * NULL), with command as the error command and name_len as the name length.
 */
static void check_error(const clopts_result_t *result, clopts_usage_error_t usage,
                        const clopts_command_t *command, const clopts_option_t *option,
                        char **argv, int index, size_t name_len)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), usage);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), command);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), option);
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[index]);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), index);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), name_len);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');
}

/* ---- sentinel ----------------------------------------------------------- */

NAOS_TEST(parse_with_env_null_program_returns_sentinel)
{
    clopts_result_t *result;

    result = clopts_parse_with_env(NULL, 0, NULL, NULL, NULL);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_PTR_EQ(result, clopts_parse(NULL, 0, NULL));
    clopts_result_free(result);
}

/* ---- fixture ------------------------------------------------------------ */

NAOS_TEST(fixture_long_names_and_kinds_are_as_designed)
{
    fixture_tool_t t;
    fixture_filt_t f;
    fixture_strict_t s;

    fixture_tool_init(&t, NULL);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_CHECK_STR_EQ(t.verbose->long_name, "verbose");
    NAOS_CHECK_INT_EQ(t.verbose->kind, CLOPTS_PARSER_COUNT);
    NAOS_CHECK_STR_EQ(t.color->long_name, "color");
    NAOS_CHECK_INT_EQ(t.color->kind, CLOPTS_PARSER_FLAG);
    NAOS_CHECK_STR_EQ(t.config->long_name, "config");
    NAOS_CHECK_INT_EQ(t.config->kind, CLOPTS_PARSER_VALUE);

    NAOS_CHECK_STR_EQ(t.output->long_name, "output");
    NAOS_CHECK_INT_EQ(t.output->kind, CLOPTS_PARSER_VALUE);
    NAOS_CHECK_STR_EQ(t.keys->long_name, "keys");
    NAOS_CHECK_INT_EQ(t.keys->kind, CLOPTS_PARSER_LIST);
    NAOS_CHECK_NULL(t.jobs->long_name);

    NAOS_CHECK_STR_EQ(t.tags->long_name, "tags");
    fixture_tool_free(&t);

    fixture_filt_init(&f, NULL);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_STR_EQ(f.lines->long_name, "lines");
    fixture_filt_free(&f);

    fixture_strict_init(&s, NULL);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_CHECK_STR_EQ(s.token->long_name, "token");
    fixture_strict_free(&s);
}

NAOS_TEST(fixture_only_color_is_negatable)
{
    fixture_tool_t t;

    fixture_tool_init(&t, NULL);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_CHECK_TRUE(t.color->negatable);
    NAOS_CHECK_FALSE(t.verbose->negatable);
    NAOS_CHECK_FALSE(t.config->negatable);
    fixture_tool_free(&t);
}

/* ---- VALUE: --name value / --name=value (6.1 to 6.4, 7.8) ------------- */

NAOS_TEST(value_takes_the_next_token)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[4]);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "x");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_takes_a_next_token_starting_with_dash)
{
    char *argv[] = { "tool", "build", "x", "--output", "-y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[4]);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_takes_dashdash_and_help_verbatim)
{
    char *argv[] = { "tool", "build", "--output", "--", "x", "--keys", "--help", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "--");
    NAOS_REQUIRE_TRUE(clopts_result_list_count(result, f.keys) == 1);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 0), "--help");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "x");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_after_equals_may_be_empty)
{
    char *argv[] = { "tool", "build", "x", "--output=", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_is_everything_after_the_first_equals)
{
    char *argv[] = { "tool", "build", "x", "--output=a=b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "a=b");
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[3] + strlen("--output="));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_with_equals_does_not_consume_the_next_token)
{
    char *argv[] = { "tool", "build", "--output=y", "x", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "x");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_at_the_end_is_missing_value)
{
    char *argv[] = { "tool", "build", "x", "--output", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_MISSING_VALUE, f.build, f.output, argv, 3, 0);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_value_at_the_end_before_a_command_is_missing_value)
{
    char *argv[] = { "tool", "--config", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_MISSING_VALUE, NULL, f.config, argv, 1, 0);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_value_before_a_command_consumes_a_command_name)
{
    char *argv[] = { "tool", "--config", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.config), argv[2]);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.config), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(value_given_twice_keeps_the_last)
{
    char *argv[] = { "tool", "build", "x", "--output", "a", "--output", "b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[6]);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "b");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(filt_lines_with_equals_and_a_positional)
{
    char *argv[] = { "filt", "--lines=5", "a", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.lines), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "a");
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(filt_lines_with_the_next_token)
{
    char *argv[] = { "filt", "--lines", "5", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.lines), argv[2]);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.lines), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

/* ---- LIST (7.9 to 7.11) ----------------------------------------------- */

NAOS_TEST(list_given_twice_concatenates_the_items)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys", "a,b", "--keys", "c", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_REQUIRE_TRUE(clopts_result_list_count(result, f.keys) == 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 2), "c");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.keys), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_keeps_an_empty_item)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys", "a,,b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_REQUIRE_TRUE(clopts_result_list_count(result, f.keys) == 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 1), "");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 2), "b");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_with_equals_splits_the_rest)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys=a,b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_REQUIRE_TRUE(clopts_result_list_count(result, f.keys) == 2);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, f.keys, 1), "b");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(list_at_the_end_is_missing_value)
{
    char *argv[] = { "tool", "build", "x", "--keys", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_MISSING_VALUE, f.build, f.keys, argv, 3, 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

/* ---- FLAG / COUNT (6.5, 7.1, 7.6) ------------------------------------- */

NAOS_TEST(flag_given_twice_is_true)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--color", "--color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_counts_each_long_occurrence)
{
    char *argv[] = { "tool", "--verbose", "build", "--verbose", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 2);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(count_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "build", "x", "--verbose=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNEXPECTED_VALUE, f.build, f.verbose, argv, 3, 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(flag_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "--color=yes", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, f.color, argv, 1, 0);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

/* ---- unknown long options (6.6, 6.7) ---------------------------------- */

NAOS_TEST(prefix_of_a_name_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--out", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, f.build, NULL, argv, 3, strlen("--out"));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(name_with_other_case_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--Output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, f.build, NULL, argv, 3, strlen("--Output"));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(unknown_name_len_stops_at_equals)
{
    char *argv[] = { "tool", "build", "x", "--out=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, f.build, NULL, argv, 3, 5);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

/* ---- --no-<name> (7.2 to 7.5) ----------------------------------------- */

NAOS_TEST(negation_sets_false_with_explicit_source)
{
    char *argv[] = { "tool", "--no-color", "build", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(flag_then_negation_is_false)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--color", "--no-color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_then_flag_is_true)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--no-color", "--color", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_of_a_value_option_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--no-output", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, f.build, NULL, argv, 3,
                strlen("--no-output"));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_of_a_count_option_is_unknown)
{
    char *argv[] = { "tool", "--no-verbose", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1,
                strlen("--no-verbose"));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(negation_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "build", "x", "--no-color=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNEXPECTED_VALUE, f.build, f.color, argv, 3, 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

/* ---- global and command scope (5.3, 5.4) ------------------------------ */

NAOS_TEST(global_count_before_the_command)
{
    char *argv[] = { "tool", "--verbose", "build", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.verbose), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(global_count_after_the_command)
{
    char *argv[] = { "tool", "build", "--verbose", "x", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, f.verbose), 1);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "x");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(command_option_before_the_command_is_unknown)
{
    /* 5.3: before the subcommand only global options are matched. */
    char *argv[] = { "tool", "--output", "y", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--output"));
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(other_command_option_is_unknown)
{
    char *argv[] = { "tool", "cat", "--output", "y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, f.cat, NULL, argv,
                2, strlen("--output"));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

/* ---- --help / --version (10.1, 10.5, 10.6) ---------------------------- */

NAOS_TEST(help_after_the_command_is_help_of_that_command)
{
    char *argv[] = { "tool", "build", "x", "--help", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_HELP);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_NULL(result->error.token);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(help_in_filt_is_help)
{
    char *argv[] = { "filt", "a", "--help", "b", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_HELP);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(version_after_the_command_is_version)
{
    char *argv[] = { "tool", "build", "--version", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_VERSION);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(help_with_equals_is_unexpected_value_without_option)
{
    char *argv[] = { "tool", "build", "x", "--help=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNEXPECTED_VALUE, f.build, NULL, argv, 3, 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(help_with_equals_before_the_command_is_unexpected_value)
{
    char *argv[] = { "tool", "--help=1", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, NULL, argv, 1, 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(version_with_equals_is_unexpected_value_without_option)
{
    char *argv[] = { "tool", "--version=1", "build", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, NULL, argv, 1, 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(version_without_a_version_string_is_unknown)
{
    char *argv[] = { "filt", "--version", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--version"));
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(version_with_equals_without_a_version_string_is_unknown)
{
    char *argv[] = { "filt", "--version=1", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    result = parse_filt(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    check_error(result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--version"));
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(help_consumed_as_a_value_is_not_help)
{
    char *argv[] = { "tool", "build", "x", "--output", "--help", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, f.output), argv[4]);
    clopts_result_free(result);
    fixture_tool_free(&f);
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
    NAOS_ADD_TEST(help_after_the_command_is_help_of_that_command),
    NAOS_ADD_TEST(help_in_filt_is_help),
    NAOS_ADD_TEST(version_after_the_command_is_version),
    NAOS_ADD_TEST(help_with_equals_is_unexpected_value_without_option),
    NAOS_ADD_TEST(help_with_equals_before_the_command_is_unexpected_value),
    NAOS_ADD_TEST(version_with_equals_is_unexpected_value_without_option),
    NAOS_ADD_TEST(version_without_a_version_string_is_unknown),
    NAOS_ADD_TEST(version_with_equals_without_a_version_string_is_unknown),
    NAOS_ADD_TEST(help_consumed_as_a_value_is_not_help)
)
