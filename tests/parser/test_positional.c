/*
 * Suite positional: positionals mixed with options, --, a lone -, required /
 * optional / variadic positionals and unexpected arguments.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

#define COPY_POS(i) (&fixture_copy_positionals[(i)])
#define TOOL_CMD(i) (&fixture_tool_commands[(i)])

/* Maximum number of argv elements parse_checked handles. */
#define MAX_ARGV 8

/*
 * Parses argv[0..argc-1] (NULL-terminated) for program with an empty mock
 * environment and checks that argv is unchanged afterwards.
 */
static clopts_status_t parse_checked(const clopts_program_t *program, int argc, char **argv,
                                     clopts_result_t *result)
{
    char *saved[MAX_ARGV];
    char saved_text[MAX_ARGV][16];
    fixture_env_t env;
    clopts_status_t status;
    int i;

    NAOS_REQUIRE_TRUE(argc < MAX_ARGV);
    for (i = 0; i < argc; i++)
    {
        size_t len = strlen(argv[i]);

        NAOS_REQUIRE_TRUE(len < sizeof(saved_text[i]));
        saved[i] = argv[i];
        memcpy(saved_text[i], argv[i], len + 1);
    }
    fixture_env_init(&env, NULL, 0);
    status = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, &env, result);
    NAOS_CHECK_INT_EQ(result->status, status);
    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved[i]);
        NAOS_CHECK_STR_EQ(argv[i], saved_text[i]);
    }
    NAOS_CHECK_NULL(argv[argc]);
    return status;
}

/*
 * Checks that result is OK with the positionals expected[0..count-1], each
 * being the very argv element (not a copy).
 */
static void check_positionals(const clopts_result_t *result, char *const *expected, size_t count)
{
    size_t i;

    NAOS_REQUIRE_INT_EQ(result->status, CLOPTS_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, i), expected[i]);
    }
}

/* Checks name, required and variadic of one positional. */
static void check_positional(const clopts_positional_t *positional, const char *name,
                             bool required, bool variadic)
{
    NAOS_CHECK_STR_EQ(positional->name, name);
    NAOS_CHECK_INT_EQ(positional->required ? 1 : 0, required ? 1 : 0);
    NAOS_CHECK_INT_EQ(positional->variadic ? 1 : 0, variadic ? 1 : 0);
}

NAOS_TEST(fixture_positionals_are_as_designed)
{
    check_positional(&fixture_build_positionals[FIXTURE_BUILD_POS_SRC], "SRC", true, false);
    check_positional(&fixture_build_positionals[FIXTURE_BUILD_POS_EXTRA], "EXTRA", false, true);
    check_positional(&fixture_copy_positionals[FIXTURE_COPY_POS_SRC], "SRC", true, false);
    check_positional(&fixture_copy_positionals[FIXTURE_COPY_POS_DST], "DST", true, false);
    check_positional(&fixture_cat_positionals[FIXTURE_CAT_POS_FILE], "FILE", false, true);
    check_positional(&fixture_filt_positionals[FIXTURE_FILT_POS_FILE], "FILE", false, true);
}

NAOS_TEST(fixture_programs_with_commands_have_no_program_positionals)
{
    NAOS_CHECK_UINT_EQ(fixture_tool.positional_count, 0);
    NAOS_CHECK_NULL(fixture_tool.positionals);
    NAOS_CHECK_UINT_EQ(fixture_strict.positional_count, 0);
    NAOS_CHECK_NULL(fixture_strict.positionals);
}

NAOS_TEST(copy_takes_two_positionals_in_order)
{
    char *argv[] = { "tool", "copy", "a", "b", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_tool, 4, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, TOOL_CMD(FIXTURE_TOOL_CMD_COPY));
    check_positionals(&result, &argv[2], 2);
    clopts_result_free(&result);
}

NAOS_TEST(copy_without_dst_is_missing_argument)
{
    char *argv[] = { "tool", "copy", "a", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_tool, 3, argv, &result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result.error.command, TOOL_CMD(FIXTURE_TOOL_CMD_COPY));
    NAOS_CHECK_PTR_EQ(result.error.positional, COPY_POS(FIXTURE_COPY_POS_DST));
    NAOS_CHECK_STR_EQ(result.error.positional->name, "DST");
    NAOS_CHECK_NULL(result.error.option);
    clopts_result_free(&result);
}

NAOS_TEST(copy_with_a_third_token_is_unexpected_argument)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_tool, 5, argv, &result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_UNEXPECTED_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result.error.command, TOOL_CMD(FIXTURE_TOOL_CMD_COPY));
    NAOS_CHECK_PTR_EQ(result.error.token, argv[4]);
    NAOS_CHECK_STR_EQ(result.error.token, "c");
    NAOS_CHECK_INT_EQ(result.error.argv_index, 4);
    NAOS_CHECK_NULL(result.error.option);
    NAOS_CHECK_NULL(result.error.positional);
    NAOS_CHECK_UINT_EQ(result.error.name_len, 0);
    NAOS_CHECK_INT_EQ(result.error.short_name, '\0');
    clopts_result_free(&result);
}

NAOS_TEST(cat_without_tokens_has_no_positionals)
{
    char *argv[] = { "tool", "cat", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_tool, 2, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, TOOL_CMD(FIXTURE_TOOL_CMD_CAT));
    check_positionals(&result, NULL, 0);
    clopts_result_free(&result);
}

NAOS_TEST(cat_variadic_takes_every_remaining_token)
{
    char *argv[] = { "tool", "cat", "x", "y", "z", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_tool, 5, argv, &result), CLOPTS_OK);
    check_positionals(&result, &argv[2], 3);
    clopts_result_free(&result);
}

NAOS_TEST(cat_after_dashdash_dash_tokens_are_positionals)
{
    char *argv[] = { "tool", "cat", "--", "-x", "--y", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_tool, 5, argv, &result), CLOPTS_OK);
    check_positionals(&result, &argv[3], 2);
    clopts_result_free(&result);
}

NAOS_TEST(cat_lone_dash_is_a_positional)
{
    char *argv[] = { "tool", "cat", "-", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_tool, 3, argv, &result), CLOPTS_OK);
    check_positionals(&result, &argv[2], 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(&result, 0), "-");
    clopts_result_free(&result);
}

NAOS_TEST(filt_without_subcommands_takes_positionals)
{
    char *argv[] = { "filt", "a", "b", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_filt, 3, argv, &result), CLOPTS_OK);
    NAOS_CHECK_NULL(result.command);
    check_positionals(&result, &argv[1], 2);
    clopts_result_free(&result);
}

NAOS_TEST(strict_without_tokens_checks_the_global_required_option)
{
    char *argv[] = { "strict", NULL };
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(parse_checked(&fixture_strict, 1, argv, &result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN]);
    NAOS_CHECK_NULL(result.error.command);
    NAOS_CHECK_NULL(result.command);
    clopts_result_free(&result);
}

NAOS_SUITE(positional,
    NAOS_ADD_TEST(fixture_positionals_are_as_designed),
    NAOS_ADD_TEST(fixture_programs_with_commands_have_no_program_positionals),
    NAOS_ADD_TEST(copy_takes_two_positionals_in_order),
    NAOS_ADD_TEST(copy_without_dst_is_missing_argument),
    NAOS_ADD_TEST(copy_with_a_third_token_is_unexpected_argument),
    NAOS_ADD_TEST(cat_without_tokens_has_no_positionals),
    NAOS_ADD_TEST(cat_variadic_takes_every_remaining_token),
    NAOS_ADD_TEST(cat_after_dashdash_dash_tokens_are_positionals),
    NAOS_ADD_TEST(cat_lone_dash_is_a_positional),
    NAOS_ADD_TEST(filt_without_subcommands_takes_positionals),
    NAOS_ADD_TEST(strict_without_tokens_checks_the_global_required_option)
)
