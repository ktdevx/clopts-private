/*
 * Suite errors: fields of the structured error, the first error wins, the
 * recovery scan and the priority between terminal options (the -h/--help
 * and --version of the fixtures, and a terminal option of a command) and
 * usage errors.
 *
 * Ported from the first-version errors suite onto the builder fixtures of
 * test_fixtures.c (handles instead of index constants) and the result handle
 * accessors. Every case uses the same argv as the first version, except one
 * case of unknown_command_fields: "tool help nope" now selects the help
 * command, an ordinary command. The status is always checked with
 * NAOS_REQUIRE_INT_EQ before anything else is read.
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

/*
 * Parses argv for program and checks that it is TERMINATED by the terminal
 * option terminal (recorded as given), with command selected (NULL: none)
 * and no error.
 */
static void check_terminated(clopts_program_t *program, char **argv, const clopts_option_t *terminal,
                             const clopts_command_t *command)
{
    clopts_result_t *result;

    result = parse_checked(program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), terminal);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), command);
    NAOS_CHECK_TRUE(clopts_result_flag(result, terminal));
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
    NAOS_CHECK_INT_EQ(s.token->kind, CLOPTS_OPTION_VALUE);
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

/* ---- recovery scan: a terminal option wins over usage errors (10.7, 10.8) */

NAOS_TEST(short_help_after_unknown_option_terminates_in_the_command)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.help, t.build);
    fixture_tool_free(&t);
}

NAOS_TEST(long_help_after_unknown_option_terminates_in_the_command)
{
    char *argv[] = { "tool", "build", "--bad", "x", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.help, t.build);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_error_before_selection_terminates_without_a_command)
{
    /* The scan stops at --bad, so build is never selected. */
    char *argv[] = { "tool", "--bad", "build", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.help, NULL);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_unknown_command_terminates_without_a_command)
{
    char *argv[] = { "tool", "bulid", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.help, NULL);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_unexpected_argument_terminates_in_the_command)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.help, t.copy);
    fixture_tool_free(&t);
}

NAOS_TEST(help_after_unexpected_value_terminates_in_the_command)
{
    char *argv[] = { "tool", "build", "x", "--output=y", "--color=1", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.help, t.build);
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
    check_terminated(t.program, argv_build, t.help, t.build);
    check_terminated(t.program, argv_copy, t.help, t.copy);
    fixture_tool_free(&t);

    STRICT_BEGIN(s);
    check_terminated(s.program, argv_strict, s.help, NULL);
    fixture_strict_free(&s);
}

NAOS_TEST(version_after_unknown_option_terminates)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "--bad", "--version", NULL };
    char *argv_unknown[] = { "tool", "nope", "--version", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.version, t.build);
    check_terminated(t.program, argv_unknown, t.version, NULL);
    fixture_tool_free(&t);
}

NAOS_TEST(version_without_a_version_option_does_not_recover)
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
    check_terminated(f.program, argv, f.help, NULL);
    fixture_filt_free(&f);
}

/* ---- terminal options: the first one wins (10.9) ------------------------ */

NAOS_TEST(first_of_help_and_version_wins_after_an_error)
{
    char *argv_help[] = { "tool", "--bad", "--help", "--version", NULL };
    char *argv_version[] = { "tool", "build", "--bad", "--version", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv_help, t.help, NULL);
    check_terminated(t.program, argv_version, t.version, t.build);
    fixture_tool_free(&t);
}

NAOS_TEST(first_of_help_and_version_wins_without_an_error)
{
    char *argv_help[] = { "tool", "build", "x", "-h", "--version", NULL };
    char *argv_version[] = { "tool", "build", "x", "--version", "--help", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv_help, t.help, t.build);
    check_terminated(t.program, argv_version, t.version, t.build);
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

/* ---- the help command of the fixture: an ordinary command (10.1) -------- */

NAOS_TEST(help_command_followed_by_help_option_terminates)
{
    /* help is a declared command: --help / -h after it terminate in it, whatever its COMMAND. */
    char *argv_long[] = { "tool", "help", "--help", NULL };
    char *argv_short[] = { "tool", "help", "-h", NULL };
    char *argv_copy[] = { "tool", "help", "copy", "--help", NULL };
    char *argv_copy_short[] = { "tool", "help", "copy", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv_long, t.help, t.help_command);
    check_terminated(t.program, argv_short, t.help, t.help_command);
    check_terminated(t.program, argv_copy, t.help, t.help_command);
    check_terminated(t.program, argv_copy_short, t.help, t.help_command);
    fixture_tool_free(&t);
}

NAOS_TEST(help_command_followed_by_version_terminates)
{
    char *argv[] = { "tool", "help", "--version", NULL };
    char *argv_copy[] = { "tool", "help", "copy", "--version", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv, t.version, t.help_command);
    check_terminated(t.program, argv_copy, t.version, t.help_command);
    fixture_tool_free(&t);
}

NAOS_TEST(help_command_errors_recover_on_later_tokens)
{
    /* nope is the COMMAND positional (the parser does not check it); x is an unexpected argument. */
    char *argv_nope[] = { "tool", "help", "nope", "--help", NULL };
    char *argv_extra[] = { "tool", "help", "copy", "x", "-h", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_terminated(t.program, argv_nope, t.help, t.help_command);
    check_terminated(t.program, argv_extra, t.help, t.help_command);
    fixture_tool_free(&t);
}

NAOS_TEST(help_command_takes_any_name_as_its_positional)
{
    char *argv[] = { "tool", "help", "nope", NULL };
    fixture_tool_t t;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    result = parse_checked(t.program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.help_command);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "nope");
    check_no_error(result);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(help_command_followed_by_dashdash_takes_a_positional)
{
    /* After "--", --help is the COMMAND positional of help, not a terminal option. */
    char *argv[] = { "tool", "help", "--", "--help", NULL };
    fixture_tool_t t;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    result = parse_checked(t.program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.help_command);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "--help");
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* ---- recovery scan: a terminal option of a command ----------------------- */

/*
 * A custom program (fictional): root "rec" with -q/--quit terminal FLAG and
 * a negatable terminal FLAG --info; command "run" with -s/--stop terminal FLAG
 * and -o/--out VALUE required; command "idle" with nothing.
 */
typedef struct rec
{
    clopts_program_t *program;
    clopts_command_t *run;
    clopts_command_t *idle;
    clopts_option_t *quit;
    clopts_option_t *info;
    clopts_option_t *stop;
} rec_t;

static clopts_option_t *rec_terminal(clopts_command_t *command, const char *long_name, char short_name,
                                     bool negatable)
{
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, long_name, short_name);

    test_decl_check(clopts_option_set_terminal(option, true));
    test_decl_check(clopts_option_set_negatable(option, negatable));
    return test_decl_attach(command, option);
}

static void rec_init(rec_t *r)
{
    clopts_command_t *root;
    clopts_option_t *out;

    memset(r, 0, sizeof(*r));
    test_decl_reset();
    root = test_decl_root("rec");
    r->quit = rec_terminal(root, "quit", 'q', false);
    r->info = rec_terminal(root, "info", '\0', true);
    r->run = test_decl_command(root, "run");
    r->stop = rec_terminal(r->run, "stop", 's', false);
    out = test_decl_option(CLOPTS_OPTION_VALUE, "out", 'o');
    test_decl_check(clopts_option_set_required(out, true));
    (void)test_decl_attach(r->run, out);
    r->idle = test_decl_command(root, "idle");
    r->program = test_decl_finish(root);
    NAOS_REQUIRE_NOT_NULL(r->program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
}

NAOS_TEST(recovery_finds_a_terminal_of_the_selected_command)
{
    char *argv_long[] = { "rec", "run", "--bad", "x", "--stop", NULL };
    char *argv_short[] = { "rec", "run", "-z", "-s", "-q", NULL };
    char *argv_root[] = { "rec", "run", "--bad", "--quit", "--stop", NULL };
    rec_t r;

    rec_init(&r);
    check_terminated(r.program, argv_long, r.stop, r.run);
    check_terminated(r.program, argv_short, r.stop, r.run);
    check_terminated(r.program, argv_root, r.quit, r.run);
    clopts_program_free(r.program);
}

NAOS_TEST(recovery_ignores_a_terminal_out_of_scope)
{
    /* Before the selection only the root is in scope; in idle, --stop of run is not. */
    char *argv_before[] = { "rec", "--bad", "run", "--stop", NULL };
    char *argv_idle[] = { "rec", "idle", "--bad", "-s", NULL };
    rec_t r;

    rec_init(&r);
    {
        const expected_error_t expected_before = { CLOPTS_USAGE_UNKNOWN_OPTION, NULL, NULL, NULL, 1, 5, '\0' };
        const expected_error_t expected_idle = { CLOPTS_USAGE_UNKNOWN_OPTION, r.idle, NULL, NULL, 2, 5, '\0' };

        check_usage(r.program, argv_before, &expected_before);
        check_usage(r.program, argv_idle, &expected_idle);
    }
    clopts_program_free(r.program);
}

NAOS_TEST(recovery_ignores_the_negation_of_a_terminal)
{
    /* --no-info negates the terminal --info: it is not an exact name, so it does not recover. */
    char *argv[] = { "rec", "idle", "--bad", "--no-info", NULL };
    char *argv_exact[] = { "rec", "idle", "--bad", "--info", NULL };
    rec_t r;

    rec_init(&r);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_OPTION, r.idle, NULL, NULL, 2, 5, '\0' };

        check_usage(r.program, argv, &expected);
    }
    check_terminated(r.program, argv_exact, r.info, r.idle);
    clopts_program_free(r.program);
}

NAOS_TEST(negation_of_a_terminal_does_not_terminate)
{
    /*
     * In the main scan too, --no-info only records the negatable terminal
     * --info as false: the parse goes on and ends as usual.
     */
    char *argv[] = { "rec", "idle", "--no-info", NULL };
    clopts_result_t *result;
    rec_t r;

    rec_init(&r);
    result = clopts_parse(r.program, 3, argv);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    NAOS_CHECK_FALSE(clopts_result_flag(result, r.info));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, r.info), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);
    clopts_program_free(r.program);
}

/* ---- fields of each usage error kind (11.3, 11.4) ----------------------- */

NAOS_TEST(unknown_command_fields)
{
    char *argv[] = { "tool", "bulid", NULL };
    char *argv_dashdash[] = { "tool", "--", "build", NULL };
    const expected_error_t expected = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 1, 0, '\0' };
    const expected_error_t expected_dashdash = { CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, NULL, NULL, 2, 0, '\0' };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    check_usage(t.program, argv, &expected);
    check_usage(t.program, argv_dashdash, &expected_dashdash);
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
    char *argv_terminal[] = { "tool", "build", "x", "--help=1", NULL };
    fixture_tool_t t;

    TOOL_BEGIN(t);
    {
        const expected_error_t expected = { CLOPTS_USAGE_UNEXPECTED_VALUE, NULL, t.color, NULL, 1, 0, '\0' };
        const expected_error_t expected_terminal = { CLOPTS_USAGE_UNEXPECTED_VALUE, t.build, t.help, NULL, 3, 0, '\0' };

        check_usage(t.program, argv, &expected);
        check_usage(t.program, argv_terminal, &expected_terminal);
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
        const expected_error_t expected_help = { CLOPTS_USAGE_UNEXPECTED_ARGUMENT, t.help_command, NULL, NULL, 3, 0,
                                                 '\0' };

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
    NAOS_ADD_TEST(short_help_after_unknown_option_terminates_in_the_command),
    NAOS_ADD_TEST(long_help_after_unknown_option_terminates_in_the_command),
    NAOS_ADD_TEST(help_after_error_before_selection_terminates_without_a_command),
    NAOS_ADD_TEST(help_after_unknown_command_terminates_without_a_command),
    NAOS_ADD_TEST(help_after_unexpected_argument_terminates_in_the_command),
    NAOS_ADD_TEST(help_after_unexpected_value_terminates_in_the_command),
    NAOS_ADD_TEST(help_skips_required_check),
    NAOS_ADD_TEST(version_after_unknown_option_terminates),
    NAOS_ADD_TEST(version_without_a_version_option_does_not_recover),
    NAOS_ADD_TEST(help_recovers_without_subcommands),
    NAOS_ADD_TEST(first_of_help_and_version_wins_after_an_error),
    NAOS_ADD_TEST(first_of_help_and_version_wins_without_an_error),
    NAOS_ADD_TEST(help_consumed_as_value_is_not_a_request),
    NAOS_ADD_TEST(help_consumed_as_value_before_an_error_is_not_a_request),
    NAOS_ADD_TEST(recovery_stops_at_dashdash),
    NAOS_ADD_TEST(help_after_dashdash_is_not_a_request),
    NAOS_ADD_TEST(only_exact_tokens_count_in_recovery),
    NAOS_ADD_TEST(help_command_followed_by_help_option_terminates),
    NAOS_ADD_TEST(help_command_followed_by_version_terminates),
    NAOS_ADD_TEST(help_command_errors_recover_on_later_tokens),
    NAOS_ADD_TEST(help_command_takes_any_name_as_its_positional),
    NAOS_ADD_TEST(help_command_followed_by_dashdash_takes_a_positional),
    NAOS_ADD_TEST(recovery_finds_a_terminal_of_the_selected_command),
    NAOS_ADD_TEST(recovery_ignores_a_terminal_out_of_scope),
    NAOS_ADD_TEST(recovery_ignores_the_negation_of_a_terminal),
    NAOS_ADD_TEST(negation_of_a_terminal_does_not_terminate),
    NAOS_ADD_TEST(unknown_command_fields),
    NAOS_ADD_TEST(unknown_option_fields_long_form),
    NAOS_ADD_TEST(unknown_option_fields_short_form),
    NAOS_ADD_TEST(missing_value_fields),
    NAOS_ADD_TEST(unexpected_value_fields),
    NAOS_ADD_TEST(missing_option_fields),
    NAOS_ADD_TEST(missing_argument_fields),
    NAOS_ADD_TEST(unexpected_argument_fields)
)
