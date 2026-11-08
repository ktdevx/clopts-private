/*
 * Suite short: short option groups, attached values, -h and unknown short
 * options.
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
#define OUTPUT BUILD_OPT(FIXTURE_BUILD_OPT_OUTPUT)
#define KEYS BUILD_OPT(FIXTURE_BUILD_OPT_KEYS)
#define JOBS BUILD_OPT(FIXTURE_BUILD_OPT_JOBS)
#define LINES FILT_OPT(FIXTURE_FILT_OPT_LINES)
#define BUILD TOOL_CMD(FIXTURE_TOOL_CMD_BUILD)

/* Maximum number of argv elements parse_argv handles. */
#define MAX_ARGV 16

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
 * Checks a usage error of kind usage on argv[index] for the short option
 * character short_name, naming option (may be NULL), with command as
 * error.command.
 */
static void check_short_error(const clopts_result_t *result, clopts_usage_error_t usage,
                              const clopts_command_t *command, const clopts_option_t *option,
                              char **argv, int index, char short_name)
{
    NAOS_CHECK_INT_EQ(result->status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result->error.usage, usage);
    NAOS_CHECK_PTR_EQ(result->error.command, command);
    NAOS_CHECK_PTR_EQ(result->error.option, option);
    NAOS_CHECK_NULL(result->error.positional);
    NAOS_CHECK_PTR_EQ(result->error.token, argv[index]);
    NAOS_CHECK_INT_EQ(result->error.argv_index, index);
    NAOS_CHECK_UINT_EQ(result->error.name_len, 0);
    NAOS_CHECK_INT_EQ(result->error.short_name, short_name);
}

/* ---- fixture ------------------------------------------------------------ */

NAOS_TEST(fixture_short_names_are_as_designed)
{
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].short_name, 'v');
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].short_name, '\0');
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].short_name, '\0');

    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].short_name, 'o');
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].short_name, 'k');
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].short_name, 'j');
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].kind, CLOPTS_COUNT);

    NAOS_CHECK_INT_EQ(fixture_cat_options[FIXTURE_CAT_OPT_TAGS].short_name, '\0');
    NAOS_CHECK_INT_EQ(fixture_filt_options[FIXTURE_FILT_OPT_LINES].short_name, 'n');
    NAOS_CHECK_INT_EQ(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].short_name, '\0');
}

/* ---- groups of FLAG / COUNT (5.8, 6.6, 6.7) ----------------------------- */

NAOS_TEST(group_counts_each_character)
{
    char *argv[] = { "tool", "build", "x", "-vvv", "-o", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 3);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, VERBOSE), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[5]);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    clopts_result_free(&result);
}

NAOS_TEST(count_is_clamped_to_its_limit)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-j", "-j", "-j",
                     "-j", "-j", "-j", "-j", "-j", "-j", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, JOBS), 8);
    clopts_result_free(&result);
}

NAOS_TEST(count_in_a_group_is_clamped_to_its_limit)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-jjjjjjjjj", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, JOBS), 8);
    clopts_result_free(&result);
}

NAOS_TEST(group_mixes_global_and_command_options)
{
    char *argv[] = { "tool", "build", "x", "-vjv", "-o", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 2);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, JOBS), 1);
    clopts_result_free(&result);
}

/* ---- global options before and after the command (4.3, 4.4) ------------- */

NAOS_TEST(global_short_before_the_command)
{
    char *argv[] = { "tool", "-v", "build", "x", "-o", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[5]);
    clopts_result_free(&result);
}

NAOS_TEST(global_short_after_the_command)
{
    char *argv[] = { "tool", "build", "-v", "x", "-o", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[5]);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "x");
    clopts_result_free(&result);
}

NAOS_TEST(global_group_without_a_command_is_no_command)
{
    char *argv[] = { "tool", "-vv", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 2);
    clopts_result_free(&result);
}

NAOS_TEST(command_short_before_the_command_is_unknown)
{
    char *argv[] = { "tool", "-o", "y", "build", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_short_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, argv, 1, 'o');
    clopts_result_free(&result);
}

/* ---- VALUE / LIST: attached value or next token (5.9, 5.10, 6.9) -------- */

NAOS_TEST(value_takes_the_next_token)
{
    char *argv[] = { "tool", "build", "x", "-vo", "file", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[4]);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, OUTPUT), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    clopts_result_free(&result);
}

NAOS_TEST(value_takes_the_rest_of_the_group)
{
    char *argv[] = { "tool", "build", "x", "-vofile", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 1);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "file");
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[3] + 3);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    clopts_result_free(&result);
}

NAOS_TEST(value_keeps_the_equals_sign)
{
    char *argv[] = { "tool", "build", "x", "-o=file", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "=file");
    clopts_result_free(&result);
}

NAOS_TEST(rest_of_the_group_after_a_value_is_not_options)
{
    char *argv[] = { "tool", "build", "x", "-ovh", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "vh");
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 0);
    clopts_result_free(&result);
}

NAOS_TEST(value_consumes_dash_h_verbatim)
{
    char *argv[] = { "tool", "build", "x", "-o", "-h", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT), argv[4]);
    clopts_result_free(&result);
}

NAOS_TEST(value_attached_after_a_count_in_a_group)
{
    /*
     * The design's "-vj4" item assumes a value option j; in the fixture j is
     * a COUNT, so the attached value case uses the LIST k and "-vj4" is an
     * unknown option on '4' (see the test below).
     */
    char *argv[] = { "tool", "build", "x", "-o", "y", "-vjk4", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, VERBOSE), 1);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, JOBS), 1);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(&result, KEYS), 1);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 0), "4");
    clopts_result_free(&result);
}

NAOS_TEST(count_followed_by_a_digit_is_an_unknown_option)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-vj4", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_short_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, argv, 5, '4');
    clopts_result_free(&result);
}

NAOS_TEST(list_concatenates_repeated_shorts)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", "a,b", "-k", "c", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(&result, KEYS), 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, KEYS, 2), "c");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, KEYS), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(program_without_subcommands_attached_value)
{
    char *argv[] = { "filt", "-n5", "a", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_OK);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, LINES), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, LINES), CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(&result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "a");
    clopts_result_free(&result);
}

NAOS_TEST(program_without_subcommands_next_token_value)
{
    char *argv[] = { "filt", "-n", "5", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, LINES), argv[2]);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, LINES), "5");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 0);
    clopts_result_free(&result);
}

/* ---- errors: unknown character and missing value (5.9, 5.11) ------------ */

NAOS_TEST(unknown_character_in_a_group)
{
    char *argv[] = { "tool", "build", "x", "-vx", "-o", "y", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_short_error(&result, CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, argv, 3, 'x');
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    clopts_result_free(&result);
}

NAOS_TEST(trailing_value_option_is_a_missing_value)
{
    char *argv[] = { "tool", "build", "x", "-o", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_short_error(&result, CLOPTS_USAGE_MISSING_VALUE, BUILD, OUTPUT, argv, 3, 'o');
    clopts_result_free(&result);
}

NAOS_TEST(trailing_value_option_in_a_group_is_a_missing_value)
{
    char *argv[] = { "tool", "build", "x", "-vo", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_USAGE_ERROR);
    check_short_error(&result, CLOPTS_USAGE_MISSING_VALUE, BUILD, OUTPUT, argv, 3, 'o');
    clopts_result_free(&result);
}

NAOS_TEST(trailing_value_option_without_subcommands_is_a_missing_value)
{
    char *argv[] = { "filt", "a", "-n", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_USAGE_ERROR);
    check_short_error(&result, CLOPTS_USAGE_MISSING_VALUE, NULL, LINES, argv, 2, 'n');
    clopts_result_free(&result);
}

/* ---- h in a group (9.1) ------------------------------------------------- */

NAOS_TEST(h_in_a_group_after_the_command_is_help_of_the_command)
{
    char *argv[] = { "tool", "build", "x", "-vh", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_HELP);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    clopts_result_free(&result);
}

NAOS_TEST(h_in_a_group_before_the_command_is_help_of_the_program)
{
    char *argv[] = { "tool", "-vh", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_HELP);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_TEST(h_after_the_command_is_help_of_the_command)
{
    char *argv[] = { "tool", "build", "-h", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_HELP);
    NAOS_CHECK_PTR_EQ(result.command, BUILD);
    clopts_result_free(&result);
}

NAOS_TEST(h_in_a_group_without_subcommands_is_help_of_the_program)
{
    char *argv[] = { "filt", "-hn", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(&fixture_filt, argv, &result), CLOPTS_HELP);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_SUITE(short,
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
    NAOS_ADD_TEST(h_in_a_group_after_the_command_is_help_of_the_command),
    NAOS_ADD_TEST(h_in_a_group_before_the_command_is_help_of_the_program),
    NAOS_ADD_TEST(h_after_the_command_is_help_of_the_command),
    NAOS_ADD_TEST(h_in_a_group_without_subcommands_is_help_of_the_program)
)
