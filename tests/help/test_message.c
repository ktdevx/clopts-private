/*
 * Suite message: the texts of every usage and declaration error kind, the
 * prefixes and the coverage of the text tables.
 *
 * The usage errors are caused by parsing real argv against the fictional
 * declarations (the result handles are never forged). The expected lines are
 * derived by hand from the table of the design (HelpMessage).
 *
 * It also checks the failing callback of the shared fixtures, which the
 * write failure tests rely on.
 */

#include "test_fixtures.h"

#include "help/help_internal.h"

#include <naos/naos.h>

#include <string.h>

/* Number of elements of argv before its terminating NULL. */
#define MESSAGE_ARGC(argv) ((int)(sizeof(argv) / sizeof((argv)[0])) - 1)

/* Both defined prefix flags. */
#define MESSAGE_PREFIX_BOTH (CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR)

/* Environment lookup that finds no variable, so the parses do not depend on the environment. */
static clopts_env_status_t message_env_unset(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_UNSET;
}

/* What message_parse_and_write observed. */
typedef struct message_run
{
    clopts_status_t status;       /* status of the result */
    clopts_usage_error_t usage;   /* usage error kind of the result */
    clopts_decl_error_t decl;     /* declaration error kind of the result */
    clopts_write_status_t write;  /* return value of clopts_write_error */
    help_recorder_t rec;          /* the bytes written */
} message_run_t;

/*
 * Parses argv against program (no environment variable is set), writes the
 * error line of the result with flags into run->rec and releases the result.
 */
static void message_parse_and_write(clopts_program_t *program, int argc, char *const *argv, unsigned flags,
                                    message_run_t *run)
{
    clopts_result_t *result = clopts_parse_with_env(program, argc, argv, message_env_unset, NULL);

    run->status = clopts_result_status(result);
    run->usage = clopts_result_usage_error(result);
    run->decl = clopts_result_decl_error(result);
    help_recorder_init(&run->rec);
    run->write = clopts_write_error(program, result, flags, help_recorder_write, &run->rec);
    clopts_result_free(result);
}

/* ---- usage errors (7.1 to 7.9) ----------------------------------------- */

NAOS_TEST(unknown_command_names_the_token)
{
    char *argv[] = { "tool", "bulid", NULL };
    const char *expected = "unknown command 'bulid'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNKNOWN_COMMAND);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unknown_long_option_names_the_part_before_the_equals_sign)
{
    char *argv[] = { "tool", "build", "--bad=1", NULL };
    const char *expected = "unknown option '--bad'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unknown_long_option_without_value_names_the_whole_name)
{
    char *argv[] = { "tool", "--bad", NULL };
    const char *expected = "unknown option '--bad'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unknown_short_option_in_a_group_names_the_character)
{
    char *argv[] = { "tool", "build", "-vx", NULL };
    const char *expected = "unknown option '-x'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unknown_non_ascii_byte_in_a_group_names_the_whole_token)
{
    /* -v then U+00E9 (C3 A9): the offending byte is 0xC3, which is not ASCII. */
    char *argv[] = { "tool", "build", "-v\xC3\xA9", NULL };
    const char *expected = "unknown option '-v\xC3\xA9'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unknown_non_ascii_byte_first_in_a_group_names_the_whole_token)
{
    char *argv[] = { "tool", "-\xE6\x97\xA5", NULL };
    const char *expected = "unknown option '-\xE6\x97\xA5'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(missing_value_names_the_short_form_as_written)
{
    char *argv[] = { "tool", "build", "-o", NULL };
    const char *expected = "option '-o' requires a value\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_MISSING_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(missing_value_in_a_group_names_the_short_form_as_written)
{
    char *argv[] = { "tool", "build", "-vo", NULL };
    const char *expected = "option '-o' requires a value\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_MISSING_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(missing_value_names_the_long_form_as_written)
{
    char *argv[] = { "tool", "build", "--output", NULL };
    const char *expected = "option '--output' requires a value\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_MISSING_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unexpected_value_names_the_long_name_of_the_option)
{
    char *argv[] = { "tool", "--color=yes", NULL };
    const char *expected = "option '--color' does not take a value\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unexpected_value_to_a_negated_flag_names_the_long_name_of_the_option)
{
    char *argv[] = { "tool", "--no-color=yes", NULL };
    const char *expected = "option '--color' does not take a value\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(unexpected_value_to_a_reserved_option_names_the_part_before_the_equals_sign)
{
    char *argv[] = { "tool", "--help=x", NULL };
    char *argv_version[] = { "tool", "--version=", NULL };
    const char *expected = "option '--help' does not take a value\n";
    const char *expected_version = "option '--version' does not take a value\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_version), argv_version, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected_version, strlen(expected_version));
    help_fixture_tool_free(&f);
}

NAOS_TEST(missing_option_names_the_long_name)
{
    char *argv[] = { "tool", "build", "SRC", NULL };
    const char *expected = "missing required option '--output'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(missing_option_without_a_long_name_names_the_short_name)
{
    /* A one-option declaration local to this test: -t VALUE required, no long name. */
    char *argv[] = { "short", NULL };
    const char *expected = "missing required option '-t'\n";
    clopts_program_t *program = clopts_program_new("short");
    clopts_option_t *target;
    message_run_t run;

    NAOS_REQUIRE_NOT_NULL(program);
    target = clopts_command_add_value(clopts_program_root(program), NULL, 't');
    clopts_option_set_required(target, true);
    message_parse_and_write(program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    clopts_program_free(program);
}

NAOS_TEST(missing_argument_names_the_positional)
{
    char *argv[] = { "tool", "copy", "a", NULL };
    const char *expected = "missing argument <DST>\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(missing_argument_of_a_program_without_subcommands)
{
    char *argv[] = { "filt", NULL };
    const char *expected = "missing argument <FILE>\n";
    help_fixture_filt_t f;
    message_run_t run;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_filt_free(&f);
}

NAOS_TEST(unexpected_argument_names_the_token)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", NULL };
    const char *expected = "unexpected argument 'c'\n";
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_UNEXPECTED_ARGUMENT);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(usage_error_line_has_no_usage_hint)
{
    char *argv[] = { "tool", "build", "-o", NULL };
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_NULL(strstr(run.rec.buf, "Run '"));
    NAOS_CHECK_NULL(strstr(run.rec.buf, "--help"));
    /* exactly one line, ending with LF */
    NAOS_REQUIRE_TRUE(run.rec.len > 0);
    NAOS_CHECK_INT_EQ(run.rec.buf[run.rec.len - 1], '\n');
    NAOS_CHECK_PTR_EQ(strchr(run.rec.buf, '\n'), run.rec.buf + run.rec.len - 1);
    help_fixture_tool_free(&f);
}

/* ---- out of memory and the other statuses (8.4, 8.5) ------------------- */

NAOS_TEST(null_result_is_out_of_memory)
{
    const char *expected = "out of memory\n";
    help_fixture_tool_t f;
    help_recorder_t rec;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(f.program, NULL, 0u, help_recorder_write, &rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(out_of_memory_takes_the_program_prefix)
{
    const char *expected = "tool: error: out of memory\n";
    help_fixture_tool_t f;
    help_recorder_t rec;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(f.program, NULL, MESSAGE_PREFIX_BOTH, help_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

NAOS_TEST(out_of_memory_omits_an_empty_program_name)
{
    const char *expected = "error: out of memory\n";
    const char *expected_program_only = "out of memory\n";
    clopts_program_t *program = clopts_program_new("");
    help_recorder_t rec;

    NAOS_REQUIRE_NOT_NULL(program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(program, NULL, MESSAGE_PREFIX_BOTH, help_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));

    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(program, NULL, CLOPTS_PREFIX_PROGRAM, help_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected_program_only, strlen(expected_program_only));
    clopts_program_free(program);
}

NAOS_TEST(out_of_memory_omits_a_missing_program_name)
{
    const char *expected = "error: out of memory\n";
    clopts_program_t *program = clopts_program_new(NULL);
    help_recorder_t rec;

    NAOS_REQUIRE_NOT_NULL(program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(program, NULL, MESSAGE_PREFIX_BOTH, help_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    clopts_program_free(program);
}

NAOS_TEST(statuses_without_an_error_are_unavailable)
{
    char *argv_ok[] = { "tool", "build", "-o", "out", "SRC", NULL };
    char *argv_help[] = { "tool", "--help", NULL };
    char *argv_version[] = { "tool", "--version", NULL };
    char *argv_no_command[] = { "tool", NULL };
    help_fixture_tool_t f;
    message_run_t run;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_ok), argv_ok, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_help), argv_help, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_HELP);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_version), argv_version, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_VERSION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_no_command), argv_no_command, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    help_fixture_tool_free(&f);
}

/* ---- prefixes (9.1 to 9.6) --------------------------------------------- */

NAOS_TEST(prefixes_without_an_error_command)
{
    /* The unknown command is reported for the root: no command in the prefix. */
    char *argv[] = { "tool", "bulid", NULL };
    const char *expected[4] = {
        "unknown command 'bulid'\n",
        "tool: unknown command 'bulid'\n",
        "error: unknown command 'bulid'\n",
        "tool: error: unknown command 'bulid'\n",
    };
    help_fixture_tool_t f;
    message_run_t run;
    unsigned flags;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, flags, &run);
        NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
        NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected[flags], strlen(expected[flags]));
    }
    help_fixture_tool_free(&f);
}

NAOS_TEST(prefixes_with_an_error_command)
{
    char *argv[] = { "tool", "build", "-o", NULL };
    const char *expected[4] = {
        "option '-o' requires a value\n",
        "tool build: option '-o' requires a value\n",
        "error: option '-o' requires a value\n",
        "tool build: error: option '-o' requires a value\n",
    };
    help_fixture_tool_t f;
    message_run_t run;
    unsigned flags;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, flags, &run);
        NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
        NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected[flags], strlen(expected[flags]));
    }
    help_fixture_tool_free(&f);
}

NAOS_TEST(prefixes_of_a_program_without_subcommands)
{
    char *argv[] = { "filt", "--lines", NULL };
    const char *expected[4] = {
        "option '--lines' requires a value\n",
        "filt: option '--lines' requires a value\n",
        "error: option '--lines' requires a value\n",
        "filt: error: option '--lines' requires a value\n",
    };
    help_fixture_filt_t f;
    message_run_t run;
    unsigned flags;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, flags, &run);
        NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
        NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected[flags], strlen(expected[flags]));
    }
    help_fixture_filt_free(&f);
}

NAOS_TEST(undefined_prefix_bits_are_ignored)
{
    char *argv_command[] = { "tool", "build", "-o", NULL };
    char *argv_root[] = { "tool", "bulid", NULL };
    const unsigned extra[3] = { 0x4u, 0x80000000u, ~(unsigned)MESSAGE_PREFIX_BOTH };
    help_fixture_tool_t f;
    message_run_t plain;
    message_run_t run;
    unsigned flags;
    int i;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        for (i = 0; i < 3; i++)
        {
            message_parse_and_write(f.program, MESSAGE_ARGC(argv_command), argv_command, flags, &plain);
            message_parse_and_write(f.program, MESSAGE_ARGC(argv_command), argv_command, flags | extra[i], &run);
            NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
            NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, plain.rec.buf, plain.rec.len);

            message_parse_and_write(f.program, MESSAGE_ARGC(argv_root), argv_root, flags, &plain);
            message_parse_and_write(f.program, MESSAGE_ARGC(argv_root), argv_root, flags | extra[i], &run);
            NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
            NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, plain.rec.buf, plain.rec.len);
        }
    }

    /* ~0u holds both defined bits, so it reads as PROGRAM | ERROR. */
    message_parse_and_write(f.program, MESSAGE_ARGC(argv_command), argv_command, ~0u, &run);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, "tool build: error: option '-o' requires a value\n",
                      strlen("tool build: error: option '-o' requires a value\n"));
    help_fixture_tool_free(&f);
}

NAOS_TEST(undefined_prefix_bits_are_ignored_for_out_of_memory)
{
    const char *expected = "out of memory\n";
    help_fixture_tool_t f;
    help_recorder_t rec;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(f.program, NULL, ~(unsigned)MESSAGE_PREFIX_BOTH, help_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    help_fixture_tool_free(&f);
}

/* ---- declaration errors (8.1 to 8.3) ----------------------------------- */

/* Largest value of each kind enum; a new kind must raise them (12.2). */
#define MESSAGE_USAGE_KIND_MAX 7
#define MESSAGE_DECL_KIND_MAX 15

/*
 * Parses an argv of the program name alone against program, whose
 * declaration error is copied to the result, and writes its error line.
 */
static void message_decl_write(clopts_program_t *program, unsigned flags, message_run_t *run)
{
    char *argv[] = { "prog", NULL };

    message_parse_and_write(program, MESSAGE_ARGC(argv), argv, flags, run);
}

/* Root handler that causes MIXED_LAYOUT; never called. */
static int message_handler(const clopts_result_t *result, void *userdata)
{
    (void)result;
    (void)userdata;
    return 0;
}

/* Checks the line written for the declaration error recorded in program, then releases program. */
#define MESSAGE_CHECK_DECL(program, flags, kind, expected)                           \
    do                                                                               \
    {                                                                                \
        message_run_t run_;                                                          \
                                                                                     \
        message_decl_write((program), (flags), &run_);                               \
        NAOS_REQUIRE_INT_EQ(run_.status, CLOPTS_DECL_ERROR);                         \
        NAOS_CHECK_INT_EQ(run_.decl, (kind));                                        \
        NAOS_CHECK_INT_EQ(run_.write, CLOPTS_WRITE_OK);                              \
        NAOS_CHECK_MEM_EQ(run_.rec.buf, run_.rec.len, (expected), strlen(expected)); \
        clopts_program_free(program);                                                \
    } while (0)

NAOS_TEST(decl_invalid_name_of_an_option_in_a_command)
{
    clopts_program_t *program = clopts_program_new("tool");
    clopts_command_t *build;

    NAOS_REQUIRE_NOT_NULL(program);
    build = clopts_program_add_command(program, "build");
    clopts_command_add_flag(build, "bad name", '\0');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_INVALID_NAME,
                       "invalid declaration: invalid name (option '--bad name' in command 'build')\n");
}

NAOS_TEST(decl_invalid_program_name_omits_the_subject)
{
    clopts_program_t *program = clopts_program_new("");

    NAOS_REQUIRE_NOT_NULL(program);
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_INVALID_NAME, "invalid declaration: invalid name\n");

    program = clopts_program_new("");
    NAOS_REQUIRE_NOT_NULL(program);
    MESSAGE_CHECK_DECL(program, MESSAGE_PREFIX_BOTH, CLOPTS_DECL_INVALID_NAME,
                       "error: invalid declaration: invalid name\n");

    program = clopts_program_new(NULL);
    NAOS_REQUIRE_NOT_NULL(program);
    MESSAGE_CHECK_DECL(program, MESSAGE_PREFIX_BOTH, CLOPTS_DECL_INVALID_NAME,
                       "error: invalid declaration: invalid name\n");
}

NAOS_TEST(decl_invalid_command_name_omits_the_subject_and_the_command_prefix)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_add_command(program, "");
    MESSAGE_CHECK_DECL(program, MESSAGE_PREFIX_BOTH, CLOPTS_DECL_INVALID_NAME,
                       "tool: error: invalid declaration: invalid name\n");

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_add_command(program, NULL);
    MESSAGE_CHECK_DECL(program, MESSAGE_PREFIX_BOTH, CLOPTS_DECL_INVALID_NAME,
                       "tool: error: invalid declaration: invalid name\n");
}

NAOS_TEST(decl_invalid_empty_option_name_omits_the_subject)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_command_add_flag(clopts_program_root(program), "", '\0');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_INVALID_NAME, "invalid declaration: invalid name\n");
}

NAOS_TEST(decl_no_name_omits_the_subject)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_command_add_flag(clopts_program_root(program), NULL, '\0');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_NO_NAME,
                       "invalid declaration: option has neither a long nor a short name\n");
}

NAOS_TEST(decl_no_name_in_a_command_names_the_command)
{
    clopts_program_t *program = clopts_program_new("tool");
    clopts_command_t *build;

    NAOS_REQUIRE_NOT_NULL(program);
    build = clopts_program_add_command(program, "build");
    clopts_command_add_value(build, NULL, '\0');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_NO_NAME,
                       "invalid declaration: option has neither a long nor a short name (command 'build')\n");
}

NAOS_TEST(decl_duplicate_command)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_add_command(program, "build");
    clopts_program_add_command(program, "build");
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_DUPLICATE_COMMAND,
                       "invalid declaration: duplicate command name (command 'build')\n");
}

NAOS_TEST(decl_duplicate_option_in_a_command)
{
    clopts_program_t *program = clopts_program_new("tool");
    clopts_command_t *build;

    NAOS_REQUIRE_NOT_NULL(program);
    build = clopts_program_add_command(program, "build");
    clopts_command_add_value(build, "out", 'o');
    clopts_command_add_flag(build, "out", '\0');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_DUPLICATE_OPTION,
                       "invalid declaration: duplicate option name (option '--out' in command 'build')\n");
}

NAOS_TEST(decl_reserved_option_name)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_command_add_flag(clopts_program_root(program), "help", '\0');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_RESERVED_NAME,
                       "invalid declaration: reserved name (option '--help')\n");
}

NAOS_TEST(decl_reserved_command_name)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_add_command(program, "help");
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_RESERVED_NAME,
                       "invalid declaration: reserved name (command 'help')\n");
}

NAOS_TEST(decl_not_negatable)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_option_set_negatable(clopts_command_add_value(clopts_program_root(program), "level", '\0'), true);
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_NOT_NEGATABLE,
                       "invalid declaration: only a flag can be negatable (option '--level')\n");
}

NAOS_TEST(decl_negation_conflict)
{
    clopts_program_t *program = clopts_program_new("tool");
    clopts_command_t *root;

    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    clopts_option_set_negatable(clopts_command_add_flag(root, "color", 'c'), true);
    clopts_command_add_flag(root, "no-color", '\0');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_NEGATION_CONFLICT,
                       "invalid declaration: conflicts with the negated form of a flag (option '--no-color')\n");
}

NAOS_TEST(decl_default_not_allowed_names_a_short_only_option)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_option_set_default(clopts_command_add_count(clopts_program_root(program), NULL, 'v'), "1");
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_DEFAULT_NOT_ALLOWED,
                       "invalid declaration: a flag or count cannot have a default value (option '-v')\n");
}

NAOS_TEST(decl_env_not_allowed)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_option_set_env(clopts_command_add_flag(clopts_program_root(program), "quiet", 'q'), "TOOL_QUIET");
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_ENV_NOT_ALLOWED,
                       "invalid declaration: a flag or count cannot have an environment variable (option '--quiet')\n");
}

NAOS_TEST(decl_required_with_default)
{
    clopts_program_t *program = clopts_program_new("tool");
    clopts_option_t *out;

    NAOS_REQUIRE_NOT_NULL(program);
    out = clopts_command_add_value(clopts_program_root(program), "out", '\0');
    clopts_option_set_default(out, "a.out");
    clopts_option_set_required(out, true);
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_REQUIRED_WITH_DEFAULT,
                       "invalid declaration: a required option cannot have a default value (option '--out')\n");
}

NAOS_TEST(decl_required_not_allowed)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_option_set_required(clopts_command_add_flag(clopts_program_root(program), "force", '\0'), true);
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_REQUIRED_NOT_ALLOWED,
                       "invalid declaration: a flag or count cannot be required (option '--force')\n");
}

NAOS_TEST(decl_positional_order_in_a_command)
{
    clopts_program_t *program = clopts_program_new("tool");
    clopts_command_t *copy;

    NAOS_REQUIRE_NOT_NULL(program);
    copy = clopts_program_add_command(program, "copy");
    clopts_command_add_positional(copy, "SRC");
    clopts_positional_set_required(clopts_command_add_positional(copy, "DST"), true);
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_POSITIONAL_ORDER,
                       "invalid declaration: a required argument cannot follow an optional one"
                       " (argument <DST> in command 'copy')\n");
}

NAOS_TEST(decl_variadic_position)
{
    clopts_program_t *program = clopts_program_new("filt");
    clopts_command_t *root;
    clopts_positional_t *first;

    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    first = clopts_command_add_positional(root, "FILE");
    clopts_command_add_positional(root, "OUT");
    clopts_positional_set_variadic(first, true);
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_VARIADIC_POSITION,
                       "invalid declaration: only the last argument can be variadic (argument <FILE>)\n");
}

NAOS_TEST(decl_attribute_kind)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_option_set_delimiter(clopts_command_add_value(clopts_program_root(program), "mode", 'm'), ',');
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_ATTRIBUTE_KIND,
                       "invalid declaration: attribute does not apply to this kind of option (option '--mode')\n");
}

NAOS_TEST(decl_mixed_layout_by_a_root_handler_omits_the_subject)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_add_command(program, "build");
    clopts_command_set_handler(clopts_program_root(program), message_handler, NULL);
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_MIXED_LAYOUT,
                       "invalid declaration: a program with subcommands cannot have root arguments or a root handler\n");
}

NAOS_TEST(decl_mixed_layout_by_a_root_positional_or_a_command)
{
    clopts_program_t *program = clopts_program_new("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_add_command(program, "build");
    clopts_command_add_positional(clopts_program_root(program), "FILE");
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_MIXED_LAYOUT,
                       "invalid declaration: a program with subcommands cannot have root arguments or a root handler"
                       " (argument <FILE>)\n");

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_command_add_positional(clopts_program_root(program), "FILE");
    clopts_program_add_command(program, "run");
    MESSAGE_CHECK_DECL(program, 0u, CLOPTS_DECL_MIXED_LAYOUT,
                       "invalid declaration: a program with subcommands cannot have root arguments or a root handler"
                       " (command 'run')\n");
}

NAOS_TEST(decl_error_prefixes)
{
    const char *expected[4] = {
        "invalid declaration: duplicate option name (option '--out' in command 'build')\n",
        "tool build: invalid declaration: duplicate option name (option '--out' in command 'build')\n",
        "error: invalid declaration: duplicate option name (option '--out' in command 'build')\n",
        "tool build: error: invalid declaration: duplicate option name (option '--out' in command 'build')\n",
    };
    clopts_program_t *program;
    clopts_command_t *build;
    unsigned flags;

    for (flags = 0u; flags < 4u; flags++)
    {
        program = clopts_program_new("tool");
        NAOS_REQUIRE_NOT_NULL(program);
        build = clopts_program_add_command(program, "build");
        clopts_command_add_value(build, "out", 'o');
        clopts_command_add_flag(build, "out", '\0');
        MESSAGE_CHECK_DECL(program, flags, CLOPTS_DECL_DUPLICATE_OPTION, expected[flags]);
    }

    /* An error of the root scope: the program name alone. */
    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_command_add_flag(clopts_program_root(program), "help", '\0');
    MESSAGE_CHECK_DECL(program, MESSAGE_PREFIX_BOTH, CLOPTS_DECL_RESERVED_NAME,
                       "tool: error: invalid declaration: reserved name (option '--help')\n");
}

/* ---- coverage of the text tables (12.1, 12.2) -------------------------- */

NAOS_TEST(usage_text_covers_every_kind)
{
    const char *texts[MESSAGE_USAGE_KIND_MAX + 1];
    int i;
    int j;

    NAOS_CHECK_NULL(clopts_help_usage_text(CLOPTS_USAGE_NONE));
    NAOS_CHECK_NULL(clopts_help_usage_text((clopts_usage_error_t)(MESSAGE_USAGE_KIND_MAX + 1)));
    for (i = 1; i <= MESSAGE_USAGE_KIND_MAX; i++)
    {
        texts[i] = clopts_help_usage_text((clopts_usage_error_t)i);
        NAOS_REQUIRE_NOT_NULL(texts[i]);
        NAOS_CHECK_TRUE(texts[i][0] != '\0');
        for (j = 1; j < i; j++)
        {
            NAOS_CHECK_TRUE(strcmp(texts[i], texts[j]) != 0);
        }
    }
}

NAOS_TEST(decl_reason_covers_every_kind)
{
    const char *reasons[MESSAGE_DECL_KIND_MAX + 1];
    int i;
    int j;

    NAOS_CHECK_NULL(clopts_help_decl_reason(CLOPTS_DECL_NONE));
    NAOS_CHECK_NULL(clopts_help_decl_reason((clopts_decl_error_t)(MESSAGE_DECL_KIND_MAX + 1)));
    for (i = 1; i <= MESSAGE_DECL_KIND_MAX; i++)
    {
        reasons[i] = clopts_help_decl_reason((clopts_decl_error_t)i);
        NAOS_REQUIRE_NOT_NULL(reasons[i]);
        NAOS_CHECK_TRUE(reasons[i][0] != '\0');
        for (j = 1; j < i; j++)
        {
            NAOS_CHECK_TRUE(strcmp(reasons[i], reasons[j]) != 0);
        }
    }
}

/* ---- the failing callback of the fixtures ------------------------------ */

NAOS_TEST(failer_fails_on_the_nth_call_and_counts_later_calls)
{
    help_failer_t failer;

    help_failer_init(&failer, 3);
    NAOS_CHECK_UINT_EQ(failer.fail_at, 3);
    NAOS_CHECK_UINT_EQ(failer.calls, 0);
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "ab", 2), 0);
    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "c", 1), 0);
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "d", 1), 1);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 3);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "e", 1), 1);
    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "f", 0), 1);
    NAOS_CHECK_UINT_EQ(failer.calls, 5);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 2);

    /* only the bytes before the failing call were accepted */
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "abc", (size_t)3);
}

NAOS_TEST(failer_fails_on_the_first_call)
{
    help_failer_t failer;

    help_failer_init(&failer, 1);
    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "a", 1), 1);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 1);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_UINT_EQ(failer.written.len, 0);
}

NAOS_TEST(failer_with_zero_never_fails)
{
    clopts_write_fn fn = help_failer_write;
    help_failer_t failer;
    int i;

    help_failer_init(&failer, 0);
    for (i = 0; i < 10; i++)
    {
        NAOS_CHECK_INT_EQ(fn(&failer, "x", 1), 0);
    }
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 10);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "xxxxxxxxxx", (size_t)10);
}

NAOS_SUITE(message,
    NAOS_ADD_TEST(unknown_command_names_the_token),
    NAOS_ADD_TEST(unknown_long_option_names_the_part_before_the_equals_sign),
    NAOS_ADD_TEST(unknown_long_option_without_value_names_the_whole_name),
    NAOS_ADD_TEST(unknown_short_option_in_a_group_names_the_character),
    NAOS_ADD_TEST(unknown_non_ascii_byte_in_a_group_names_the_whole_token),
    NAOS_ADD_TEST(unknown_non_ascii_byte_first_in_a_group_names_the_whole_token),
    NAOS_ADD_TEST(missing_value_names_the_short_form_as_written),
    NAOS_ADD_TEST(missing_value_in_a_group_names_the_short_form_as_written),
    NAOS_ADD_TEST(missing_value_names_the_long_form_as_written),
    NAOS_ADD_TEST(unexpected_value_names_the_long_name_of_the_option),
    NAOS_ADD_TEST(unexpected_value_to_a_negated_flag_names_the_long_name_of_the_option),
    NAOS_ADD_TEST(unexpected_value_to_a_reserved_option_names_the_part_before_the_equals_sign),
    NAOS_ADD_TEST(missing_option_names_the_long_name),
    NAOS_ADD_TEST(missing_option_without_a_long_name_names_the_short_name),
    NAOS_ADD_TEST(missing_argument_names_the_positional),
    NAOS_ADD_TEST(missing_argument_of_a_program_without_subcommands),
    NAOS_ADD_TEST(unexpected_argument_names_the_token),
    NAOS_ADD_TEST(usage_error_line_has_no_usage_hint),
    NAOS_ADD_TEST(null_result_is_out_of_memory),
    NAOS_ADD_TEST(out_of_memory_takes_the_program_prefix),
    NAOS_ADD_TEST(out_of_memory_omits_an_empty_program_name),
    NAOS_ADD_TEST(out_of_memory_omits_a_missing_program_name),
    NAOS_ADD_TEST(statuses_without_an_error_are_unavailable),
    NAOS_ADD_TEST(prefixes_without_an_error_command),
    NAOS_ADD_TEST(prefixes_with_an_error_command),
    NAOS_ADD_TEST(prefixes_of_a_program_without_subcommands),
    NAOS_ADD_TEST(undefined_prefix_bits_are_ignored),
    NAOS_ADD_TEST(undefined_prefix_bits_are_ignored_for_out_of_memory),
    NAOS_ADD_TEST(decl_invalid_name_of_an_option_in_a_command),
    NAOS_ADD_TEST(decl_invalid_program_name_omits_the_subject),
    NAOS_ADD_TEST(decl_invalid_command_name_omits_the_subject_and_the_command_prefix),
    NAOS_ADD_TEST(decl_invalid_empty_option_name_omits_the_subject),
    NAOS_ADD_TEST(decl_no_name_omits_the_subject),
    NAOS_ADD_TEST(decl_no_name_in_a_command_names_the_command),
    NAOS_ADD_TEST(decl_duplicate_command),
    NAOS_ADD_TEST(decl_duplicate_option_in_a_command),
    NAOS_ADD_TEST(decl_reserved_option_name),
    NAOS_ADD_TEST(decl_reserved_command_name),
    NAOS_ADD_TEST(decl_not_negatable),
    NAOS_ADD_TEST(decl_negation_conflict),
    NAOS_ADD_TEST(decl_default_not_allowed_names_a_short_only_option),
    NAOS_ADD_TEST(decl_env_not_allowed),
    NAOS_ADD_TEST(decl_required_with_default),
    NAOS_ADD_TEST(decl_required_not_allowed),
    NAOS_ADD_TEST(decl_positional_order_in_a_command),
    NAOS_ADD_TEST(decl_variadic_position),
    NAOS_ADD_TEST(decl_attribute_kind),
    NAOS_ADD_TEST(decl_mixed_layout_by_a_root_handler_omits_the_subject),
    NAOS_ADD_TEST(decl_mixed_layout_by_a_root_positional_or_a_command),
    NAOS_ADD_TEST(decl_error_prefixes),
    NAOS_ADD_TEST(usage_text_covers_every_kind),
    NAOS_ADD_TEST(decl_reason_covers_every_kind),
    NAOS_ADD_TEST(failer_fails_on_the_nth_call_and_counts_later_calls),
    NAOS_ADD_TEST(failer_fails_on_the_first_call),
    NAOS_ADD_TEST(failer_with_zero_never_fails)
)
