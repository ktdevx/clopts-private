/*
 * Suite long: long options, --name=value, value consumption, unknown long
 * options and --no-<name>.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

#define TOOL_OPT(i) (&fixture_tool_options[(i)])
#define BUILD_OPT(i) (&fixture_build_options[(i)])
#define FILT_OPT(i) (&fixture_filt_options[(i)])
#define TOOL_CMD(i) (&fixture_tool_commands[(i)])

#define VERBOSE TOOL_OPT(FIXTURE_TOOL_OPT_VERBOSE)
#define COLOR TOOL_OPT(FIXTURE_TOOL_OPT_COLOR)
#define CONFIG TOOL_OPT(FIXTURE_TOOL_OPT_CONFIG)
#define OUTPUT BUILD_OPT(FIXTURE_BUILD_OPT_OUTPUT)
#define KEYS BUILD_OPT(FIXTURE_BUILD_OPT_KEYS)
#define LINES FILT_OPT(FIXTURE_FILT_OPT_LINES)
#define BUILD TOOL_CMD(FIXTURE_TOOL_CMD_BUILD)

/* Maximum number of argv elements parse_argv handles. */
#define MAX_ARGV 10

/*
 * Parses the NULL-terminated argv for program with an empty mock environment
 * and checks that the status is stored in result and that neither the argv
 * array nor the strings it points to changed.
 */
static clopts_status_t parse_argv(const clopts_program_t *program, char **argv,
                                  clopts_result_t *result)
{
    char *saved[MAX_ARGV];
    char text[MAX_ARGV][32];
    fixture_env_t env;
    clopts_status_t status;
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
    status = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, &env, result);
    NAOS_CHECK_INT_EQ(result->status, status);
    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved[i]);
        NAOS_CHECK_STR_EQ(argv[i], text[i]);
    }
    NAOS_CHECK_NULL(argv[argc]);
    return status;
}

/*
 * Checks a usage error of kind usage on argv[index] naming option (may be
 * NULL), with command as error.command and name_len as error.name_len.
 */
static void check_error(const clopts_result_t *result, clopts_usage_error_t usage,
                        const clopts_command_t *command, const clopts_option_t *option,
                        char **argv, int index, size_t name_len)
{
    NAOS_CHECK_INT_EQ(result->status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result->error.usage, usage);
    NAOS_CHECK_PTR_EQ(result->error.command, command);
    NAOS_CHECK_PTR_EQ(result->error.option, option);
    NAOS_CHECK_NULL(result->error.positional);
    NAOS_CHECK_PTR_EQ(result->error.token, argv[index]);
    NAOS_CHECK_INT_EQ(result->error.argv_index, index);
    NAOS_CHECK_UINT_EQ(result->error.name_len, name_len);
    NAOS_CHECK_INT_EQ(result->error.short_name, '\0');
}

/* ---- fixture ------------------------------------------------------------ */

NAOS_TEST(fixture_long_names_and_kinds_are_as_designed)
{
    NAOS_CHECK_STR_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].long_name, "verbose");
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].kind, CLOPTS_COUNT);
    NAOS_CHECK_STR_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].long_name, "color");
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].kind, CLOPTS_FLAG);
    NAOS_CHECK_STR_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].long_name, "config");
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].kind, CLOPTS_VALUE);

    NAOS_CHECK_STR_EQ(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].long_name, "output");
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].kind, CLOPTS_VALUE);
    NAOS_CHECK_STR_EQ(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].long_name, "keys");
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].kind, CLOPTS_LIST);
    NAOS_CHECK_NULL(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].long_name);

    NAOS_CHECK_STR_EQ(fixture_cat_options[FIXTURE_CAT_OPT_TAGS].long_name, "tags");
    NAOS_CHECK_STR_EQ(fixture_filt_options[FIXTURE_FILT_OPT_LINES].long_name, "lines");
    NAOS_CHECK_STR_EQ(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].long_name, "token");
}

NAOS_TEST(fixture_only_color_is_negatable)
{
    NAOS_CHECK_TRUE(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].negatable);
    NAOS_CHECK_FALSE(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].negatable);
    NAOS_CHECK_FALSE(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].negatable);
}

/* ---- VALUE: --name value / --name=value (5.1 to 5.4, 6.8) --------------- */

NAOS_TEST(value_takes_the_next_token)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[4]);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, OUTPUT), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "x");
    clopts_result_free(&result);
}

NAOS_TEST(value_takes_a_next_token_starting_with_dash)
{
    char *argv[] = { "tool", "build", "x", "--output", "-y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[4]);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    clopts_result_free(&result);
}

NAOS_TEST(value_takes_dashdash_and_help_verbatim)
{
    char *argv[] = { "tool", "build", "--output", "--", "x", "--keys", "--help", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "--");
    NAOS_REQUIRE_TRUE(clopts_result_list_count(&result, KEYS) == 1);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 0), "--help");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "x");
    clopts_result_free(&result);
}

NAOS_TEST(value_after_equals_may_be_empty)
{
    char *argv[] = { "tool", "build", "x", "--output=", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, OUTPUT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(value_is_everything_after_the_first_equals)
{
    char *argv[] = { "tool", "build", "x", "--output=a=b", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "a=b");
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[3] + strlen("--output="));
    clopts_result_free(&result);
}

NAOS_TEST(value_with_equals_does_not_consume_the_next_token)
{
    char *argv[] = { "tool", "build", "--output=y", "x", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "y");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "x");
    clopts_result_free(&result);
}

NAOS_TEST(value_at_the_end_is_missing_value)
{
    char *argv[] = { "tool", "build", "x", "--output", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_MISSING_VALUE, BUILD, OUTPUT, argv, 3, 0);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    clopts_result_free(&result);
}

NAOS_TEST(global_value_at_the_end_before_a_command_is_missing_value)
{
    char *argv[] = { "tool", "--config", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_MISSING_VALUE, NULL, CONFIG, argv, 1, 0);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_TEST(global_value_before_a_command_consumes_a_command_name)
{
    char *argv[] = { "tool", "--config", "build", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, CONFIG), argv[2]);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, CONFIG), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(value_given_twice_keeps_the_last)
{
    char *argv[] = { "tool", "build", "x", "--output", "a", "--output", "b", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[6]);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "b");
    clopts_result_free(&result);
}

NAOS_TEST(filt_lines_with_equals_and_a_positional)
{
    char *argv[] = { "filt", "--lines=5", "a", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_OK);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, LINES), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, LINES), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "a");
    clopts_result_free(&result);
}

NAOS_TEST(filt_lines_with_the_next_token)
{
    char *argv[] = { "filt", "--lines", "5", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, LINES), argv[2]);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, LINES), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, LINES), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 0);
    clopts_result_free(&result);
}

/* ---- LIST (6.9 to 6.11) ------------------------------------------------- */

NAOS_TEST(list_given_twice_concatenates_the_items)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys", "a,b", "--keys", "c", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_REQUIRE_TRUE(clopts_result_list_count(&result, KEYS) == 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 2), "c");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, KEYS), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(list_keeps_an_empty_item)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys", "a,,b", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_REQUIRE_TRUE(clopts_result_list_count(&result, KEYS) == 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 1), "");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 2), "b");
    clopts_result_free(&result);
}

NAOS_TEST(list_with_equals_splits_the_rest)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--keys=a,b", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_REQUIRE_TRUE(clopts_result_list_count(&result, KEYS) == 2);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 1), "b");
    clopts_result_free(&result);
}

NAOS_TEST(list_at_the_end_is_missing_value)
{
    char *argv[] = { "tool", "build", "x", "--keys", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_MISSING_VALUE, BUILD, KEYS, argv, 3, 0);
    clopts_result_free(&result);
}

/* ---- FLAG / COUNT (5.5, 6.1, 6.6) ---------------------------------------- */

NAOS_TEST(flag_given_twice_is_true)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--color", "--color", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(&result, COLOR));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, COLOR), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(count_counts_each_long_occurrence)
{
    char *argv[] = { "tool", "--verbose", "build", "--verbose", "x", "--output", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 2);
    clopts_result_free(&result);
}

NAOS_TEST(count_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "build", "x", "--verbose=1", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNEXPECTED_VALUE, BUILD, VERBOSE, argv, 3, 0);
    clopts_result_free(&result);
}

NAOS_TEST(flag_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "--color=yes", "build", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, COLOR, argv, 1, 0);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

/* ---- unknown long options (5.6, 5.7) ------------------------------------ */

NAOS_TEST(prefix_of_a_name_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--out", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, argv, 3, strlen("--out"));
    clopts_result_free(&result);
}

NAOS_TEST(name_with_other_case_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--Output", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, argv, 3, strlen("--Output"));
    clopts_result_free(&result);
}

NAOS_TEST(unknown_name_len_stops_at_equals)
{
    char *argv[] = { "tool", "build", "x", "--out=1", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, argv, 3, 5);
    clopts_result_free(&result);
}

/* ---- --no-<name> (6.2 to 6.5) ------------------------------------------- */

NAOS_TEST(negation_sets_false_with_explicit_source)
{
    char *argv[] = { "tool", "--no-color", "build", "x", "--output", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(&result, COLOR));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, COLOR), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(flag_then_negation_is_false)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--color", "--no-color", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(&result, COLOR));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, COLOR), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(negation_then_flag_is_true)
{
    char *argv[] = { "tool", "build", "x", "--output", "y", "--no-color", "--color", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(&result, COLOR));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, COLOR), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(negation_of_a_value_option_is_unknown)
{
    char *argv[] = { "tool", "build", "x", "--no-output", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, argv, 3,
                strlen("--no-output"));
    clopts_result_free(&result);
}

NAOS_TEST(negation_of_a_count_option_is_unknown)
{
    char *argv[] = { "tool", "--no-verbose", "build", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1,
                strlen("--no-verbose"));
    clopts_result_free(&result);
}

NAOS_TEST(negation_with_equals_is_unexpected_value)
{
    char *argv[] = { "tool", "build", "x", "--no-color=1", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNEXPECTED_VALUE, BUILD, COLOR, argv, 3, 0);
    clopts_result_free(&result);
}

/* ---- global and command scope (4.3, 4.4) --------------------------------- */

NAOS_TEST(global_count_before_the_command)
{
    char *argv[] = { "tool", "--verbose", "build", "x", "--output", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 1);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, VERBOSE), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "y");
    clopts_result_free(&result);
}

NAOS_TEST(global_count_after_the_command)
{
    char *argv[] = { "tool", "build", "--verbose", "x", "--output", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 1);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "y");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "x");
    clopts_result_free(&result);
}

NAOS_TEST(command_option_before_the_command_is_unknown)
{
    char *argv[] = { "tool", "--output", "y", "build", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--output"));
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_TEST(other_command_option_is_unknown)
{
    char *argv[] = { "tool", "cat", "--output", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, TOOL_CMD(FIXTURE_TOOL_CMD_CAT), NULL, argv,
                2, strlen("--output"));
    clopts_result_free(&result);
}

/* ---- --help / --version (9.1, 9.4, 9.5) --------------------------------- */

NAOS_TEST(help_after_the_command_is_help_of_that_command)
{
    char *argv[] = { "tool", "build", "x", "--help", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_HELP);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    NAOS_CHECK_NULL(result.error.token);
    NAOS_CHECK_INT_EQ(result.error.argv_index, -1);
    clopts_result_free(&result);
}

NAOS_TEST(help_in_filt_is_help)
{
    char *argv[] = { "filt", "a", "--help", "b", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_HELP);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_TEST(version_after_the_command_is_version)
{
    char *argv[] = { "tool", "build", "--version", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_VERSION);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    clopts_result_free(&result);
}

NAOS_TEST(help_with_equals_is_unexpected_value_without_option)
{
    char *argv[] = { "tool", "build", "x", "--help=1", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNEXPECTED_VALUE, BUILD, NULL, argv, 3, 0);
    clopts_result_free(&result);
}

NAOS_TEST(help_with_equals_before_the_command_is_unexpected_value)
{
    char *argv[] = { "tool", "--help=1", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, NULL, argv, 1, 0);
    clopts_result_free(&result);
}

NAOS_TEST(version_with_equals_is_unexpected_value_without_option)
{
    char *argv[] = { "tool", "--version=1", "build", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, NULL, argv, 1, 0);
    clopts_result_free(&result);
}

NAOS_TEST(version_without_a_version_string_is_unknown)
{
    char *argv[] = { "filt", "--version", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--version"));
    clopts_result_free(&result);
}

NAOS_TEST(version_with_equals_without_a_version_string_is_unknown)
{
    char *argv[] = { "filt", "--version=1", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, strlen("--version"));
    clopts_result_free(&result);
}

NAOS_TEST(help_consumed_as_a_value_is_not_help)
{
    char *argv[] = { "tool", "build", "x", "--output", "--help", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[4]);
    clopts_result_free(&result);
}

NAOS_SUITE(long,
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
