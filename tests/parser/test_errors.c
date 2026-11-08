/*
 * Suite errors: fields of the structured error, the first error wins,
 * recovery scan and the priority of help / version.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

#define BUILD_OPT(i) (&fixture_build_options[(i)])
#define TOOL_CMD(i) (&fixture_tool_commands[(i)])

#define COLOR (&fixture_tool_options[FIXTURE_TOOL_OPT_COLOR])
#define OUTPUT BUILD_OPT(FIXTURE_BUILD_OPT_OUTPUT)
#define TOKEN (&fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN])
#define DST (&fixture_copy_positionals[FIXTURE_COPY_POS_DST])
#define BUILD TOOL_CMD(FIXTURE_TOOL_CMD_BUILD)
#define COPY TOOL_CMD(FIXTURE_TOOL_CMD_COPY)
#define CAT TOOL_CMD(FIXTURE_TOOL_CMD_CAT)

/* Maximum number of argv elements parse_argv handles. */
#define MAX_ARGV 12

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

/* Expected content of a usage error. token is argv[index], or NULL for -1. */
typedef struct expected_error
{
    clopts_usage_error_t usage;
    const clopts_command_t *command;
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    int index;
    size_t name_len;
    char short_name;
} expected_error_t;

/* Checks every field of the usage error in result against expected. */
static void check_error(const clopts_result_t *result, char **argv,
                        const expected_error_t *expected)
{
    NAOS_REQUIRE_INT_EQ(result->status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result->error.usage, expected->usage);
    NAOS_CHECK_PTR_EQ(result->error.command, expected->command);
    NAOS_CHECK_PTR_EQ(result->error.option, expected->option);
    NAOS_CHECK_PTR_EQ(result->error.positional, expected->positional);
    if (expected->index < 0)
    {
        NAOS_CHECK_NULL(result->error.token);
    }
    else
    {
        NAOS_CHECK_PTR_EQ(result->error.token, argv[expected->index]);
    }
    NAOS_CHECK_INT_EQ(result->error.argv_index, expected->index);
    NAOS_CHECK_UINT_EQ(result->error.name_len, expected->name_len);
    NAOS_CHECK_INT_EQ(result->error.short_name, expected->short_name);
}

/* Parses argv for program and checks that it is HELP of command. */
static void check_help(const clopts_program_t *program, char **argv,
                       const clopts_command_t *command)
{
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(program, argv, &result), CLOPTS_HELP);
    NAOS_CHECK_PTR_EQ(result.command, command);
    NAOS_CHECK_NULL(result.error.token);
    NAOS_CHECK_INT_EQ(result.error.argv_index, -1);
    clopts_result_free(&result);
}

/* Parses argv for program and checks that it is VERSION. */
static void check_version(const clopts_program_t *program, char **argv)
{
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(program, argv, &result), CLOPTS_VERSION);
    clopts_result_free(&result);
}

/* Parses argv for program and checks the usage error against expected. */
static void check_usage(const clopts_program_t *program, char **argv,
                        const expected_error_t *expected)
{
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_argv(program, argv, &result), CLOPTS_USAGE_ERROR);
    check_error(&result, argv, expected);
    clopts_result_free(&result);
}

NAOS_TEST(fixture_required_options_are_as_designed)
{
    size_t i;

    NAOS_CHECK_TRUE(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].required);
    NAOS_CHECK_FALSE(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].required);
    NAOS_CHECK_FALSE(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].required);
    NAOS_CHECK_TRUE(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].required);
    NAOS_CHECK_INT_EQ(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].kind, CLOPTS_VALUE);

    for (i = 0; i < FIXTURE_TOOL_OPT_COUNT; i++)
    {
        NAOS_CHECK_FALSE(fixture_tool_options[i].required);
    }
    NAOS_CHECK_FALSE(fixture_cat_options[FIXTURE_CAT_OPT_TAGS].required);
    NAOS_CHECK_FALSE(fixture_filt_options[FIXTURE_FILT_OPT_LINES].required);
}

/* ---- the first usage error wins (9.10) ---------------------------------- */

NAOS_TEST(only_the_first_of_two_unknown_options_is_reported)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "--worse", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, NULL, 5, 5,
                                        '\0' };

    check_usage(&fixture_tool, argv, &expected);
}

NAOS_TEST(only_the_first_of_different_errors_is_reported)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", "--bad", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, COPY, NULL, NULL, 4, 0,
                                        '\0' };

    check_usage(&fixture_tool, argv, &expected);
}

/* ---- recovery scan: help / version win over usage errors (9.6, 9.12) ---- */

NAOS_TEST(short_help_after_unknown_option_is_help_of_command)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "-h", NULL };

    check_help(&fixture_tool, argv, BUILD);
}

NAOS_TEST(long_help_after_unknown_option_is_help_of_command)
{
    char *argv[] = { "tool", "build", "--bad", "x", "--help", NULL };

    check_help(&fixture_tool, argv, BUILD);
}

NAOS_TEST(help_after_error_before_selection_is_whole_program_help)
{
    /* The scan stops at --bad, so build is never selected. */
    char *argv[] = { "tool", "--bad", "build", "--help", NULL };

    check_help(&fixture_tool, argv, NULL);
}

NAOS_TEST(help_after_unknown_command_is_whole_program_help)
{
    char *argv[] = { "tool", "bulid", "-h", NULL };

    check_help(&fixture_tool, argv, NULL);
}

NAOS_TEST(help_after_unexpected_argument_is_help_of_command)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", "--help", NULL };

    check_help(&fixture_tool, argv, COPY);
}

NAOS_TEST(help_after_unexpected_value_is_help_of_command)
{
    char *argv[] = { "tool", "build", "x", "--output=y", "--color=1", "-h", NULL };

    check_help(&fixture_tool, argv, BUILD);
}

NAOS_TEST(help_skips_required_check)
{
    char *argv_build[] = { "tool", "build", "--help", NULL };
    char *argv_copy[] = { "tool", "copy", "--bad", "-h", NULL };
    char *argv_strict[] = { "strict", "--help", NULL };

    check_help(&fixture_tool, argv_build, BUILD);
    check_help(&fixture_tool, argv_copy, COPY);
    check_help(&fixture_strict, argv_strict, NULL);
}

NAOS_TEST(version_after_unknown_option_is_version)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "--version", NULL };
    char *argv_unknown[] = { "tool", "nope", "--version", NULL };

    check_version(&fixture_tool, argv);
    check_version(&fixture_tool, argv_unknown);
}

NAOS_TEST(version_without_version_string_does_not_recover)
{
    char *argv[] = { "filt", "--bad", "--version", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, NULL, 1, 5,
                                        '\0' };

    check_usage(&fixture_filt, argv, &expected);
}

NAOS_TEST(help_recovers_without_subcommands)
{
    char *argv[] = { "filt", "--bad", "a", "-h", NULL };

    check_help(&fixture_filt, argv, NULL);
}

/* ---- help and version: the first one wins (9.7) ------------------------- */

NAOS_TEST(first_of_help_and_version_wins_after_an_error)
{
    char *argv_help[] = { "tool", "--bad", "--help", "--version", NULL };
    char *argv_version[] = { "tool", "build", "--bad", "--version", "-h", NULL };

    check_help(&fixture_tool, argv_help, NULL);
    check_version(&fixture_tool, argv_version);
}

NAOS_TEST(first_of_help_and_version_wins_without_an_error)
{
    char *argv_help[] = { "tool", "build", "x", "-h", "--version", NULL };
    char *argv_version[] = { "tool", "build", "x", "--version", "--help", NULL };

    check_help(&fixture_tool, argv_help, BUILD);
    check_version(&fixture_tool, argv_version);
}

/* ---- recovery scan: what does not count (9.1, 9.12) --------------------- */

NAOS_TEST(help_consumed_as_value_is_not_a_request)
{
    char *argv[] = { "tool", "build", "x", "-o", "-h", NULL };
    char *argv_long[] = { "tool", "build", "x", "--output", "--help", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "-h");
    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, argv_long, &result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT), "--help");
    clopts_result_free(&result);
}

NAOS_TEST(help_consumed_as_value_before_an_error_is_not_a_request)
{
    char *argv[] = { "tool", "build", "x", "-o", "-h", "--bad", NULL };
    char *argv_version[] = { "tool", "--config", "--version", "nope", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, NULL, 5, 5,
                                        '\0' };
    const expected_error_t expected_version = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL,
                                                3, 0, '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_version, &expected_version);
}

NAOS_TEST(recovery_stops_at_dashdash)
{
    char *argv[] = { "tool", "cat", "--bad", "--", "--help", NULL };
    char *argv_version[] = { "tool", "build", "--bad", "--", "--version", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, CAT, NULL, NULL, 2, 5,
                                        '\0' };
    const expected_error_t expected_version = { CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, NULL, 2,
                                                5, '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_version, &expected_version);
}

NAOS_TEST(help_after_dashdash_is_not_a_request)
{
    char *argv[] = { "tool", "--", "--help", NULL };
    char *argv_args[] = { "tool", "copy", "a", "--", "b", "c", "-h", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 2, 0,
                                        '\0' };
    const expected_error_t expected_args = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, COPY, NULL, NULL,
                                             5, 0, '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_args, &expected_args);
}

NAOS_TEST(only_exact_tokens_count_in_recovery)
{
    char *argv[] = { "tool", "build", "--bad", "-vh", "--help=1", "--helpx", "-hv", "help",
                     "--version=1", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, NULL, 2, 5,
                                        '\0' };

    check_usage(&fixture_tool, argv, &expected);
}

/* ---- the error token itself is a request (controller decision, 9.1) ----- */

NAOS_TEST(help_command_followed_by_help_option_is_help)
{
    char *argv_long[] = { "tool", "help", "--help", NULL };
    char *argv_short[] = { "tool", "help", "-h", NULL };
    char *argv_copy[] = { "tool", "help", "copy", "--help", NULL };
    char *argv_copy_short[] = { "tool", "help", "copy", "-h", NULL };

    check_help(&fixture_tool, argv_long, NULL);
    check_help(&fixture_tool, argv_short, NULL);
    check_help(&fixture_tool, argv_copy, COPY);
    check_help(&fixture_tool, argv_copy_short, COPY);
}

NAOS_TEST(help_command_followed_by_version_is_version)
{
    char *argv[] = { "tool", "help", "--version", NULL };
    char *argv_copy[] = { "tool", "help", "copy", "--version", NULL };

    check_version(&fixture_tool, argv);
    check_version(&fixture_tool, argv_copy);
}

NAOS_TEST(help_command_errors_recover_on_later_tokens)
{
    char *argv_nope[] = { "tool", "help", "nope", "--help", NULL };
    char *argv_extra[] = { "tool", "help", "copy", "x", "-h", NULL };

    check_help(&fixture_tool, argv_nope, NULL);
    check_help(&fixture_tool, argv_extra, COPY);
}

NAOS_TEST(help_command_followed_by_dashdash_does_not_recover)
{
    char *argv[] = { "tool", "help", "--", "--help", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 2, 0,
                                        '\0' };

    check_usage(&fixture_tool, argv, &expected);
}

/* ---- declaration errors always win (9.9) -------------------------------- */

/* An invalid declaration: its second option has neither a long nor a short name. */
static const clopts_option_t broken_options[] = {
    { .long_name = "fine", .kind = CLOPTS_FLAG },
    { .kind = CLOPTS_FLAG },
};
static const clopts_program_t broken_program = {
    .name = "broken",
    .version = "0.1",
    .options = broken_options,
    .option_count = 2,
};

NAOS_TEST(declaration_error_wins_over_help_and_version)
{
    char *argv_help[] = { "broken", "--help", NULL };
    char *argv_version[] = { "broken", "--version", NULL };
    char *argv_recover[] = { "broken", "--bad", "-h", NULL };
    char **cases[] = { argv_help, argv_version, argv_recover };
    clopts_result_t result = { 0 };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        NAOS_REQUIRE_INT_EQ(parse_argv(&broken_program, cases[i], &result), CLOPTS_DECL_ERROR);
        NAOS_CHECK_INT_EQ(result.error.decl, CLOPTS_DECL_NO_NAME);
        NAOS_CHECK_PTR_EQ(result.error.option, &broken_options[1]);
        NAOS_CHECK_NULL(result.error.token);
        NAOS_CHECK_INT_EQ(result.error.argv_index, -1);
    }
    clopts_result_free(&result);
}

/* ---- fields of each usage error kind (10.3, 10.4, 10.7) ----------------- */

NAOS_TEST(unknown_command_fields)
{
    char *argv[] = { "tool", "bulid", NULL };
    char *argv_help[] = { "tool", "help", "nope", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 1, 0,
                                        '\0' };
    const expected_error_t expected_help = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 2, 0,
                                             '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_help, &expected_help);
}

NAOS_TEST(unknown_option_fields_long_form)
{
    char *argv[] = { "tool", "build", "x", "--nope=1", NULL };
    char *argv_global[] = { "tool", "--output", "y", "build", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, NULL, 3, 6,
                                        '\0' };
    const expected_error_t expected_global = { CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, NULL, 1,
                                               8, '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_global, &expected_global);
}

NAOS_TEST(unknown_option_fields_short_form)
{
    char *argv[] = { "tool", "build", "x", "-vz", NULL };
    char *argv_global[] = { "tool", "-q", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, BUILD, NULL, NULL, 3, 0,
                                        'z' };
    const expected_error_t expected_global = { CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, NULL, 1,
                                               0, 'q' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_global, &expected_global);
}

NAOS_TEST(missing_value_fields)
{
    char *argv_long[] = { "tool", "build", "x", "--output", NULL };
    char *argv_short[] = { "tool", "build", "x", "-vo", NULL };
    char *argv_global[] = { "tool", "--config", NULL };
    const expected_error_t expected_long = { CLOPTS_USAGE_MISSING_VALUE, BUILD, OUTPUT, NULL, 3,
                                             0, '\0' };
    const expected_error_t expected_short = { CLOPTS_USAGE_MISSING_VALUE, BUILD, OUTPUT, NULL, 3,
                                              0, 'o' };
    const expected_error_t expected_global = {
        CLOPTS_USAGE_MISSING_VALUE, NULL, &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG], NULL, 1,
        0, '\0'
    };

    check_usage(&fixture_tool, argv_long, &expected_long);
    check_usage(&fixture_tool, argv_short, &expected_short);
    check_usage(&fixture_tool, argv_global, &expected_global);
}

NAOS_TEST(unexpected_value_fields)
{
    char *argv[] = { "tool", "--color=yes", NULL };
    char *argv_reserved[] = { "tool", "build", "x", "--help=1", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, COLOR, NULL, 1, 0,
                                        '\0' };
    const expected_error_t expected_reserved = { CLOPTS_USAGE_UNEXPECTED_VALUE, BUILD, NULL, NULL,
                                                 3, 0, '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_reserved, &expected_reserved);
}

NAOS_TEST(missing_option_fields)
{
    char *argv[] = { "tool", "build", "x", NULL };
    char *argv_global[] = { "strict", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_MISSING_OPTION, BUILD, OUTPUT, NULL, -1, 0,
                                        '\0' };
    const expected_error_t expected_global = { CLOPTS_USAGE_MISSING_OPTION, NULL, TOKEN, NULL, -1,
                                               0, '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_strict, argv_global, &expected_global);
}

NAOS_TEST(missing_argument_fields)
{
    char *argv[] = { "tool", "copy", "a", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_MISSING_ARGUMENT, COPY, NULL, DST, -1, 0,
                                        '\0' };

    check_usage(&fixture_tool, argv, &expected);
}

NAOS_TEST(unexpected_argument_fields)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", NULL };
    char *argv_help[] = { "tool", "help", "copy", "x", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, COPY, NULL, NULL, 4, 0,
                                        '\0' };
    const expected_error_t expected_help = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, COPY, NULL, NULL,
                                             3, 0, '\0' };

    check_usage(&fixture_tool, argv, &expected);
    check_usage(&fixture_tool, argv_help, &expected_help);
}

NAOS_SUITE(errors,
    NAOS_ADD_TEST(fixture_required_options_are_as_designed),
    NAOS_ADD_TEST(only_the_first_of_two_unknown_options_is_reported),
    NAOS_ADD_TEST(only_the_first_of_different_errors_is_reported),
    NAOS_ADD_TEST(short_help_after_unknown_option_is_help_of_command),
    NAOS_ADD_TEST(long_help_after_unknown_option_is_help_of_command),
    NAOS_ADD_TEST(help_after_error_before_selection_is_whole_program_help),
    NAOS_ADD_TEST(help_after_unknown_command_is_whole_program_help),
    NAOS_ADD_TEST(help_after_unexpected_argument_is_help_of_command),
    NAOS_ADD_TEST(help_after_unexpected_value_is_help_of_command),
    NAOS_ADD_TEST(help_skips_required_check),
    NAOS_ADD_TEST(version_after_unknown_option_is_version),
    NAOS_ADD_TEST(version_without_version_string_does_not_recover),
    NAOS_ADD_TEST(help_recovers_without_subcommands),
    NAOS_ADD_TEST(first_of_help_and_version_wins_after_an_error),
    NAOS_ADD_TEST(first_of_help_and_version_wins_without_an_error),
    NAOS_ADD_TEST(help_consumed_as_value_is_not_a_request),
    NAOS_ADD_TEST(help_consumed_as_value_before_an_error_is_not_a_request),
    NAOS_ADD_TEST(recovery_stops_at_dashdash),
    NAOS_ADD_TEST(help_after_dashdash_is_not_a_request),
    NAOS_ADD_TEST(only_exact_tokens_count_in_recovery),
    NAOS_ADD_TEST(help_command_followed_by_help_option_is_help),
    NAOS_ADD_TEST(help_command_followed_by_version_is_version),
    NAOS_ADD_TEST(help_command_errors_recover_on_later_tokens),
    NAOS_ADD_TEST(help_command_followed_by_dashdash_does_not_recover),
    NAOS_ADD_TEST(declaration_error_wins_over_help_and_version),
    NAOS_ADD_TEST(unknown_command_fields),
    NAOS_ADD_TEST(unknown_option_fields_long_form),
    NAOS_ADD_TEST(unknown_option_fields_short_form),
    NAOS_ADD_TEST(missing_value_fields),
    NAOS_ADD_TEST(unexpected_value_fields),
    NAOS_ADD_TEST(missing_option_fields),
    NAOS_ADD_TEST(missing_argument_fields),
    NAOS_ADD_TEST(unexpected_argument_fields)
)
