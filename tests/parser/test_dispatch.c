/*
 * Suite dispatch: subcommand selection, global options before and after the
 * subcommand, NO_COMMAND and the help / --help / --version forms handled
 * before a subcommand.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Number of elements of a static array. */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define TOOL_OPT(i) (&fixture_tool_options[(i)])
#define FILT_OPT(i) (&fixture_filt_options[(i)])

/*
 * An invalid declaration: its second option has neither a long nor a short
 * name (CLOPTS_DECL_NO_NAME). Everything else is valid.
 */
static const clopts_option_t broken_options[] = {
    { .long_name = "fine", .kind = CLOPTS_FLAG },
    { .kind = CLOPTS_FLAG },
};
static const clopts_program_t broken_program = {
    .name = "broken",
    .options = broken_options,
    .option_count = 2,
};

NAOS_TEST(fixture_commands_are_shaped_as_designed)
{
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    const clopts_command_t *copy = &fixture_tool_commands[FIXTURE_TOOL_CMD_COPY];
    const clopts_command_t *cat = &fixture_tool_commands[FIXTURE_TOOL_CMD_CAT];

    NAOS_CHECK_STR_EQ(build->name, "build");
    NAOS_CHECK_PTR_EQ(build->options, fixture_build_options);
    NAOS_CHECK_UINT_EQ(build->option_count, 3);
    NAOS_CHECK_PTR_EQ(build->positionals, fixture_build_positionals);
    NAOS_CHECK_UINT_EQ(build->positional_count, 2);
    NAOS_CHECK_TRUE(build->handler == fixture_handler);
    NAOS_CHECK_PTR_EQ(build->userdata, &fixture_build_handler_record);

    NAOS_CHECK_STR_EQ(copy->name, "copy");
    NAOS_CHECK_UINT_EQ(copy->option_count, 0);
    NAOS_CHECK_UINT_EQ(copy->positional_count, 2);

    NAOS_CHECK_STR_EQ(cat->name, "cat");
    NAOS_CHECK_PTR_EQ(cat->options, fixture_cat_options);
    NAOS_CHECK_UINT_EQ(cat->option_count, 1);
    NAOS_CHECK_UINT_EQ(cat->positional_count, 1);
}

NAOS_TEST(fixture_filt_has_no_commands_and_a_handler)
{
    NAOS_CHECK_UINT_EQ(fixture_filt.command_count, 0);
    NAOS_CHECK_PTR_EQ(fixture_filt.positionals, fixture_filt_positionals);
    NAOS_CHECK_TRUE(fixture_filt.handler == fixture_handler);
    NAOS_CHECK_PTR_EQ(fixture_filt.userdata, &fixture_filt_handler_record);
}

/* ---- parse entry: no tokens ------------------------------------------- */

NAOS_TEST(tool_without_arguments_is_no_command)
{
    char *argv[] = { "tool", NULL };
    clopts_result_t result = { 0 };
    clopts_status_t status;

    status = clopts_parse(&fixture_tool, 1, argv, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, TOOL_OPT(FIXTURE_TOOL_OPT_VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TOOL_OPT(FIXTURE_TOOL_OPT_VERBOSE)),
                      CLOPTS_SOURCE_NONE);
    NAOS_CHECK_FALSE(clopts_result_flag(&result, TOOL_OPT(FIXTURE_TOOL_OPT_COLOR)));
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 0);
    NAOS_CHECK_STR_EQ(argv[0], "tool");
    clopts_result_free(&result);
}

NAOS_TEST(tool_with_argc_zero_is_no_command)
{
    clopts_result_t result = { 0 };
    clopts_status_t status;

    status = clopts_parse(&fixture_tool, 0, NULL, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_TEST(no_command_resolves_global_scope_only)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x" },
    };
    char *argv[] = { "tool", NULL };
    fixture_env_t env;
    clopts_result_t result = { 0 };
    clopts_status_t status;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    status = clopts_parse_with_env(&fixture_tool, 1, argv, fixture_env_getenv, &env, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, TOOL_OPT(FIXTURE_TOOL_OPT_CONFIG)), "/etc/t");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TOOL_OPT(FIXTURE_TOOL_OPT_CONFIG)),
                      CLOPTS_SOURCE_ENV);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    clopts_result_free(&result);
}

NAOS_TEST(no_command_checks_required_global_options)
{
    char *argv[] = { "strict", NULL };
    fixture_env_t env;
    clopts_result_t result = { 0 };
    clopts_status_t status;

    fixture_env_init(&env, NULL, 0);
    status = clopts_parse_with_env(&fixture_strict, 1, argv, fixture_env_getenv, &env, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN]);
    NAOS_CHECK_NULL(result.error.command);
    NAOS_CHECK_NULL(result.error.positional);
    NAOS_CHECK_NULL(result.error.token);
    NAOS_CHECK_INT_EQ(result.error.argv_index, -1);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_TEST(filt_without_arguments_is_ok_with_default)
{
    char *argv[] = { "filt", NULL };
    fixture_env_t env;
    clopts_result_t result = { 0 };
    clopts_status_t status;

    fixture_env_init(&env, NULL, 0);
    status = clopts_parse_with_env(&fixture_filt, 1, argv, fixture_env_getenv, &env, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, FILT_OPT(FIXTURE_FILT_OPT_LINES)), "10");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, FILT_OPT(FIXTURE_FILT_OPT_LINES)),
                      CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 0);
    clopts_result_free(&result);
}

NAOS_TEST(filt_with_argc_zero_is_ok_with_default)
{
    fixture_env_t env;
    clopts_result_t result = { 0 };
    clopts_status_t status;

    fixture_env_init(&env, NULL, 0);
    status = clopts_parse_with_env(&fixture_filt, 0, NULL, fixture_env_getenv, &env, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, FILT_OPT(FIXTURE_FILT_OPT_LINES)), "10");
    clopts_result_free(&result);
}

NAOS_TEST(filt_takes_value_from_env)
{
    static const fixture_env_var_t vars[] = {
        { "FILT_LINES", "5" },
    };
    char *argv[] = { "filt", NULL };
    fixture_env_t env;
    clopts_result_t result = { 0 };
    clopts_status_t status;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    status = clopts_parse_with_env(&fixture_filt, 1, argv, fixture_env_getenv, &env, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, FILT_OPT(FIXTURE_FILT_OPT_LINES)), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, FILT_OPT(FIXTURE_FILT_OPT_LINES)),
                      CLOPTS_SOURCE_ENV);
    clopts_result_free(&result);
}

NAOS_TEST(env_failure_is_out_of_memory_and_result_is_freeable)
{
    char *argv[] = { "filt", NULL };
    fixture_env_t env;
    clopts_result_t result = { 0 };
    clopts_status_t status;

    fixture_env_init(&env, NULL, 0);
    env.fail_next = true;
    status = clopts_parse_with_env(&fixture_filt, 1, argv, fixture_env_getenv, &env, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(result.status, status);
    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

NAOS_TEST(previous_result_is_reused)
{
    static const fixture_env_var_t vars[] = {
        { "FILT_LINES", "7" },
    };
    char *argv[] = { "filt", NULL };
    fixture_env_t env;
    clopts_result_t result = { 0 };

    /* The env copy of the first parse is owned by the result and released by the next. */
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_CHECK_INT_EQ(
        clopts_parse_with_env(&fixture_filt, 1, argv, fixture_env_getenv, &env, &result),
        CLOPTS_OK);
    NAOS_CHECK_INT_EQ(
        clopts_parse_with_env(&fixture_tool, 1, argv, fixture_env_getenv, &env, &result),
        CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(result.status, CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(result.command);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_CHECK_INT_EQ(
        clopts_parse_with_env(&fixture_filt, 0, NULL, fixture_env_getenv, &env, &result),
        CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, FILT_OPT(FIXTURE_FILT_OPT_LINES)), "7");
    clopts_result_free(&result);
}

/* ---- parse entry: declaration errors ---------------------------------- */

NAOS_TEST(invalid_declaration_is_decl_error_like_validate)
{
    /* Tokens that would select help / an unknown command if argv were read. */
    char *argv[] = { "broken", "--help", "nope", NULL };
    char *const saved_argv[] = { argv[0], argv[1], argv[2] };
    clopts_error_t validated;
    fixture_env_t env;
    clopts_result_t result = { 0 };
    clopts_status_t status;
    size_t i;

    memset(&validated, 0, sizeof(validated));
    NAOS_REQUIRE_INT_EQ(clopts_decl_validate(&broken_program, &validated), CLOPTS_DECL_ERROR);

    fixture_env_init(&env, NULL, 0);
    status = clopts_parse_with_env(&broken_program, 3, argv, fixture_env_getenv, &env, &result);
    NAOS_CHECK_INT_EQ(status, CLOPTS_DECL_ERROR);
    NAOS_CHECK_INT_EQ(result.status, status);
    NAOS_CHECK_INT_EQ(result.error.decl, CLOPTS_DECL_NO_NAME);
    NAOS_CHECK_INT_EQ(result.error.decl, validated.decl);
    NAOS_CHECK_PTR_EQ(result.error.command, validated.command);
    NAOS_CHECK_PTR_EQ(result.error.option, &broken_options[1]);
    NAOS_CHECK_PTR_EQ(result.error.option, validated.option);
    NAOS_CHECK_PTR_EQ(result.error.positional, validated.positional);
    NAOS_CHECK_PTR_EQ(result.error.token, validated.token);
    NAOS_CHECK_INT_EQ(result.error.argv_index, validated.argv_index);
    NAOS_CHECK_UINT_EQ(result.error.name_len, validated.name_len);
    NAOS_CHECK_INT_EQ(result.error.short_name, validated.short_name);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_NULL(result.impl);
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    for (i = 0; i < ARRAY_LEN(saved_argv); i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved_argv[i]);
    }
    NAOS_CHECK_STR_EQ(argv[1], "--help");
    NAOS_CHECK_STR_EQ(argv[2], "nope");
    clopts_result_free(&result);
}

NAOS_TEST(invalid_declaration_releases_previous_result)
{
    static const fixture_env_var_t vars[] = {
        { "FILT_LINES", "3" },
    };
    char *argv[] = { "filt", NULL };
    fixture_env_t env;
    clopts_result_t result = { 0 };

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(
        clopts_parse_with_env(&fixture_filt, 1, argv, fixture_env_getenv, &env, &result),
        CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_parse(&broken_program, 1, argv, &result), CLOPTS_DECL_ERROR);
    NAOS_CHECK_INT_EQ(result.status, CLOPTS_DECL_ERROR);
    NAOS_CHECK_NULL(result.impl);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_SUITE(dispatch,
    NAOS_ADD_TEST(fixture_commands_are_shaped_as_designed),
    NAOS_ADD_TEST(fixture_filt_has_no_commands_and_a_handler),
    NAOS_ADD_TEST(tool_without_arguments_is_no_command),
    NAOS_ADD_TEST(tool_with_argc_zero_is_no_command),
    NAOS_ADD_TEST(no_command_resolves_global_scope_only),
    NAOS_ADD_TEST(no_command_checks_required_global_options),
    NAOS_ADD_TEST(filt_without_arguments_is_ok_with_default),
    NAOS_ADD_TEST(filt_with_argc_zero_is_ok_with_default),
    NAOS_ADD_TEST(filt_takes_value_from_env),
    NAOS_ADD_TEST(env_failure_is_out_of_memory_and_result_is_freeable),
    NAOS_ADD_TEST(previous_result_is_reused),
    NAOS_ADD_TEST(invalid_declaration_is_decl_error_like_validate),
    NAOS_ADD_TEST(invalid_declaration_releases_previous_result)
)
