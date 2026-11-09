/*
 * Suite errors: fields of the structured error, the first error wins, the
 * recovery scan and the priority between help, version and errors.
 *
 * Ported from the first-version errors suite onto the builder fixtures of
 * test_fixtures.c (handles instead of index constants) and the result handle
 * accessors. Every case uses the same argv as the first version. The status
 * is always checked with NAOS_REQUIRE_INT_EQ before anything else is read.
 */

#include "parser/parser_internal.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <clopts/parse.h>

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Maximum number of argv elements parse_checked handles. */
#define MAX_ARGV 12

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

/* Expected content of a usage error. The token is argv[index], or NULL for -1. */
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

/* Checks every error accessor of the usage error in result against expected. */
static void check_error(const clopts_result_t *result, char **argv, const expected_error_t *expected)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), expected->usage);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), expected->command);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), expected->option);
    NAOS_CHECK_PTR_EQ(clopts_result_error_positional(result), expected->positional);
    if (expected->index < 0)
    {
        NAOS_CHECK_NULL(clopts_result_error_token(result));
    }
    else
    {
        NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[expected->index]);
    }
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), expected->index);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), expected->name_len);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), expected->short_name);
}

/* Checks that every error accessor reports no error. */
static void check_no_error(const clopts_result_t *result)
{
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_NONE);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_NULL(clopts_result_error_token(result));
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), -1);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');
}

/* Parses argv for program and checks that it is HELP of command (NULL: the whole program). */
static void check_help(clopts_program_t *program, char **argv, const clopts_command_t *command)
{
    clopts_result_t *result;

    result = parse_checked(program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_HELP);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), command);
    check_no_error(result);
    clopts_result_free(result);
}

/* Parses argv for program and checks that it is VERSION. */
static void check_version(clopts_program_t *program, char **argv)
{
    clopts_result_t *result;

    result = parse_checked(program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_VERSION);
    check_no_error(result);
    clopts_result_free(result);
}

/* Parses argv for program and checks the usage error against expected. */
static void check_usage(clopts_program_t *program, char **argv, const expected_error_t *expected)
{
    clopts_result_t *result;

    result = parse_checked(program, argv);
    check_error(result, argv, expected);
    clopts_result_free(result);
}

/* Builds the tool fixture into f. */
#define TOOL_BEGIN(f)                                                               \
    do                                                                              \
    {                                                                               \
        fixture_tool_init(&(f));                                                    \
        NAOS_REQUIRE_NOT_NULL((f).program);                                         \
        NAOS_REQUIRE_TRUE((f).ok);                                                  \
    } while (0)

/* Builds the filt fixture into f. */
#define FILT_BEGIN(f)                                                               \
    do                                                                              \
    {                                                                               \
        fixture_filt_init(&(f));                                                    \
        NAOS_REQUIRE_NOT_NULL((f).program);                                         \
        NAOS_REQUIRE_TRUE((f).ok);                                                  \
    } while (0)

/* Builds the strict fixture into f. */
#define STRICT_BEGIN(f)                                                             \
    do                                                                              \
    {                                                                               \
        fixture_strict_init(&(f));                                                  \
        NAOS_REQUIRE_NOT_NULL((f).program);                                         \
        NAOS_REQUIRE_TRUE((f).ok);                                                  \
    } while (0)

/* ---- sentinel ----------------------------------------------------------- */

NAOS_TEST(sentinel_has_no_usage_error)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    check_no_error(result);
    clopts_result_free(result);
}

/* ---- fixture ------------------------------------------------------------ */

NAOS_TEST(fixture_required_options_are_as_designed)
{
    fixture_tool_t t;
    fixture_filt_t f;
    fixture_strict_t s;

    TOOL_BEGIN(t);
    NAOS_CHECK_TRUE(t.output->required);
    NAOS_CHECK_FALSE(t.keys->required);
    NAOS_CHECK_FALSE(t.jobs->required);
    NAOS_CHECK_FALSE(t.verbose->required);
    NAOS_CHECK_FALSE(t.color->required);
    NAOS_CHECK_FALSE(t.config->required);
    NAOS_CHECK_FALSE(t.tags->required);
    fixture_tool_free(&t);

    FILT_BEGIN(f);
    NAOS_CHECK_FALSE(f.lines->required);
    fixture_filt_free(&f);

    STRICT_BEGIN(s);
    NAOS_CHECK_TRUE(s.token->required);
    NAOS_CHECK_INT_EQ(s.token->kind, CLOPTS_PARSER_VALUE);
    fixture_strict_free(&s);
}

/* ---- the first usage error wins (10.12) --------------------------------- */

NAOS_TEST(only_the_first_of_two_unknown_options_is_reported)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "--worse", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, t.build, NULL, NULL, 5, 5, '\0' };

        check_usage(t.program, argv, &expected);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(only_the_first_of_different_errors_is_reported)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", "--bad", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, t.copy, NULL, NULL, 4, 0, '\0' };

        check_usage(t.program, argv, &expected);
    }
    fixture_tool_free(&t);
}

/* ---- recovery scan: help / version win over usage errors (10.7, 10.8) --- */

NAOS_TEST(short_help_after_unknown_option_is_help_of_command)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv, t.build);
    fixture_tool_free(&t);
}

NAOS_TEST(long_help_after_unknown_option_is_help_of_command)
{
    char *argv[] = { "tool", "build", "--bad", "x", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv, t.build);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_error_before_selection_is_whole_program_help)
{
    /* The scan stops at --bad, so build is never selected. */
    char *argv[] = { "tool", "--bad", "build", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv, NULL);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_unknown_command_is_whole_program_help)
{
    char *argv[] = { "tool", "bulid", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv, NULL);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_unexpected_argument_is_help_of_command)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv, t.copy);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_unexpected_value_is_help_of_command)
{
    char *argv[] = { "tool", "build", "x", "--output=y", "--color=1", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv, t.build);
    fixture_tool_free(&t);
}

NAOS_TEST(help_skips_required_check)
{
    char *argv_build[] = { "tool", "build", "--help", NULL };
    char *argv_copy[] = { "tool", "copy", "--bad", "-h", NULL };
    char *argv_strict[] = { "strict", "--help", NULL };
    fixture_tool_t t;
    fixture_strict_t s;

    TOOL_BEGIN(t);
    check_help(t.program, argv_build, t.build);
    check_help(t.program, argv_copy, t.copy);
    fixture_tool_free(&t);

    STRICT_BEGIN(s);
    check_help(s.program, argv_strict, NULL);
    fixture_strict_free(&s);
}

NAOS_TEST(version_after_unknown_option_is_version)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "--version", NULL };
    char *argv_unknown[] = { "tool", "nope", "--version", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_version(t.program, argv);
    check_version(t.program, argv_unknown);
    fixture_tool_free(&t);
}

NAOS_TEST(version_without_version_string_does_not_recover)
{
    char *argv[] = { "filt", "--bad", "--version", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, NULL, 1, 5, '\0' };
    fixture_filt_t f;

    FILT_BEGIN(f);
    check_usage(f.program, argv, &expected);
    fixture_filt_free(&f);
}

NAOS_TEST(help_recovers_without_subcommands)
{
    char *argv[] = { "filt", "--bad", "a", "-h", NULL };
    fixture_filt_t f;

    FILT_BEGIN(f);
    check_help(f.program, argv, NULL);
    fixture_filt_free(&f);
}

/* ---- help and version: the first one wins (10.9) ------------------------ */

NAOS_TEST(first_of_help_and_version_wins_after_an_error)
{
    char *argv_help[] = { "tool", "--bad", "--help", "--version", NULL };
    char *argv_version[] = { "tool", "build", "--bad", "--version", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv_help, NULL);
    check_version(t.program, argv_version);
    fixture_tool_free(&t);
}

NAOS_TEST(first_of_help_and_version_wins_without_an_error)
{
    char *argv_help[] = { "tool", "build", "x", "-h", "--version", NULL };
    char *argv_version[] = { "tool", "build", "x", "--version", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv_help, t.build);
    check_version(t.program, argv_version);
    fixture_tool_free(&t);
}

/* ---- recovery scan: what does not count (10.1, 10.5, 10.7) -------------- */

NAOS_TEST(help_consumed_as_value_is_not_a_request)
{
    char *argv[] = { "tool", "build", "x", "-o", "-h", NULL };
    char *argv_long[] = { "tool", "build", "x", "--output", "--help", NULL };
    fixture_tool_t t;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    result = parse_checked(t.program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, t.output), "-h");
    clopts_result_free(result);

    result = parse_checked(t.program, argv_long);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, t.output), "--help");
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(help_consumed_as_value_before_an_error_is_not_a_request)
{
    char *argv[] = { "tool", "build", "x", "-o", "-h", "--bad", NULL };
    char *argv_version[] = { "tool", "--config", "--version", "nope", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, t.build, NULL, NULL, 5, 5, '\0' };
        const expected_error_t expected_version = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 3, 0, '\0' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_version, &expected_version);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(recovery_stops_at_dashdash)
{
    char *argv[] = { "tool", "cat", "--bad", "--", "--help", NULL };
    char *argv_version[] = { "tool", "build", "--bad", "--", "--version", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, t.cat, NULL, NULL, 2, 5, '\0' };
        const expected_error_t expected_version = { CLOPTS_USAGE_UNKNOWN_OPTION, t.build, NULL, NULL, 2, 5, '\0' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_version, &expected_version);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_dashdash_is_not_a_request)
{
    char *argv[] = { "tool", "--", "--help", NULL };
    char *argv_args[] = { "tool", "copy", "a", "--", "b", "c", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 2, 0, '\0' };
        const expected_error_t expected_args = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, t.copy, NULL, NULL, 5, 0, '\0' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_args, &expected_args);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(only_exact_tokens_count_in_recovery)
{
    char *argv[] = { "tool", "build", "--bad", "-vh", "--help=1", "--helpx", "-hv", "help", "--version=1", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, t.build, NULL, NULL, 2, 5, '\0' };

        check_usage(t.program, argv, &expected);
    }
    fixture_tool_free(&t);
}

/* ---- the error token itself is a request (3.6 recovery scan, 10.1) ------ */

NAOS_TEST(help_command_followed_by_help_option_is_help)
{
    char *argv_long[] = { "tool", "help", "--help", NULL };
    char *argv_short[] = { "tool", "help", "-h", NULL };
    char *argv_copy[] = { "tool", "help", "copy", "--help", NULL };
    char *argv_copy_short[] = { "tool", "help", "copy", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv_long, NULL);
    check_help(t.program, argv_short, NULL);
    check_help(t.program, argv_copy, t.copy);
    check_help(t.program, argv_copy_short, t.copy);
    fixture_tool_free(&t);
}

NAOS_TEST(help_command_followed_by_version_is_version)
{
    char *argv[] = { "tool", "help", "--version", NULL };
    char *argv_copy[] = { "tool", "help", "copy", "--version", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_version(t.program, argv);
    check_version(t.program, argv_copy);
    fixture_tool_free(&t);
}

NAOS_TEST(help_command_errors_recover_on_later_tokens)
{
    char *argv_nope[] = { "tool", "help", "nope", "--help", NULL };
    char *argv_extra[] = { "tool", "help", "copy", "x", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_help(t.program, argv_nope, NULL);
    check_help(t.program, argv_extra, t.copy);
    fixture_tool_free(&t);
}

NAOS_TEST(help_command_followed_by_dashdash_does_not_recover)
{
    char *argv[] = { "tool", "help", "--", "--help", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 2, 0, '\0' };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_usage(t.program, argv, &expected);
    fixture_tool_free(&t);
}

/* ---- fields of each usage error kind (11.3, 11.4) ----------------------- */

NAOS_TEST(unknown_command_fields)
{
    char *argv[] = { "tool", "bulid", NULL };
    char *argv_help[] = { "tool", "help", "nope", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 1, 0, '\0' };
    const expected_error_t expected_help = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 2, 0, '\0' };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_usage(t.program, argv, &expected);
    check_usage(t.program, argv_help, &expected_help);
    fixture_tool_free(&t);
}

NAOS_TEST(unknown_option_fields_long_form)
{
    char *argv[] = { "tool", "build", "x", "--nope=1", NULL };
    char *argv_global[] = { "tool", "--output", "y", "build", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, t.build, NULL, NULL, 3, 6, '\0' };
        const expected_error_t expected_global = { CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, NULL, 1, 8, '\0' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_global, &expected_global);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(unknown_option_fields_short_form)
{
    char *argv[] = { "tool", "build", "x", "-vz", NULL };
    char *argv_global[] = { "tool", "-q", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, t.build, NULL, NULL, 3, 0, 'z' };
        const expected_error_t expected_global = { CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, NULL, 1, 0, 'q' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_global, &expected_global);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(missing_value_fields)
{
    char *argv_long[] = { "tool", "build", "x", "--output", NULL };
    char *argv_short[] = { "tool", "build", "x", "-vo", NULL };
    char *argv_global[] = { "tool", "--config", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected_long = { CLOPTS_USAGE_MISSING_VALUE, t.build, t.output, NULL, 3, 0, '\0' };
        const expected_error_t expected_short = { CLOPTS_USAGE_MISSING_VALUE, t.build, t.output, NULL, 3, 0, 'o' };
        const expected_error_t expected_global = { CLOPTS_USAGE_MISSING_VALUE, NULL, t.config, NULL, 1, 0, '\0' };

        check_usage(t.program, argv_long, &expected_long);
        check_usage(t.program, argv_short, &expected_short);
        check_usage(t.program, argv_global, &expected_global);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(unexpected_value_fields)
{
    char *argv[] = { "tool", "--color=yes", NULL };
    char *argv_reserved[] = { "tool", "build", "x", "--help=1", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, t.color, NULL, 1, 0, '\0' };
        const expected_error_t expected_reserved = { CLOPTS_USAGE_UNEXPECTED_VALUE, t.build, NULL, NULL, 3, 0, '\0' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_reserved, &expected_reserved);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(missing_option_fields)
{
    char *argv[] = { "tool", "build", "x", NULL };
    char *argv_global[] = { "strict", NULL };
    fixture_tool_t t;
    fixture_strict_t s;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_MISSING_OPTION, t.build, t.output, NULL, -1, 0, '\0' };

        check_usage(t.program, argv, &expected);
    }
    fixture_tool_free(&t);

    STRICT_BEGIN(s);
    {
        const expected_error_t expected_global = { CLOPTS_USAGE_MISSING_OPTION, NULL, s.token, NULL, -1, 0, '\0' };

        check_usage(s.program, argv_global, &expected_global);
    }
    fixture_strict_free(&s);
}

NAOS_TEST(missing_argument_fields)
{
    char *argv[] = { "tool", "copy", "a", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_MISSING_ARGUMENT, t.copy, NULL, t.copy_dst, -1, 0, '\0' };

        check_usage(t.program, argv, &expected);
    }
    fixture_tool_free(&t);
}

NAOS_TEST(unexpected_argument_fields)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", NULL };
    char *argv_help[] = { "tool", "help", "copy", "x", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, t.copy, NULL, NULL, 4, 0, '\0' };
        const expected_error_t expected_help = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, t.copy, NULL, NULL, 3, 0, '\0' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_help, &expected_help);
    }
    fixture_tool_free(&t);
}

NAOS_SUITE(errors,
    NAOS_ADD_TEST(sentinel_has_no_usage_error),
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
    NAOS_ADD_TEST(unknown_command_fields),
    NAOS_ADD_TEST(unknown_option_fields_long_form),
    NAOS_ADD_TEST(unknown_option_fields_short_form),
    NAOS_ADD_TEST(missing_value_fields),
    NAOS_ADD_TEST(unexpected_value_fields),
    NAOS_ADD_TEST(missing_option_fields),
    NAOS_ADD_TEST(missing_argument_fields),
    NAOS_ADD_TEST(unexpected_argument_fields)
)
