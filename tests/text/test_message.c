/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite message: the version line and the usage hint, the texts of every
 * usage error kind, the out-of-memory line, the prefixes, the texts of clopts_decl_error_get_message() and the
 * coverage of the text tables.
 *
 * The usage errors are caused by parsing real argv against the fictional
 * declarations (the result handles are never forged). The expected lines are
 * derived by hand from the table of the design (TextMessage).
 *
 * It also checks the failing callback of the shared fixtures, which the
 * write failure tests rely on.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include "text/text_internal.h"

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
    clopts_parse_status_t status;       /* status of the result */
    clopts_usage_error_kind_t usage;   /* usage error kind of the result */
    clopts_write_status_t write;  /* return value of clopts_write_error */
    text_recorder_t rec;          /* the bytes written */
} message_run_t;

/*
 * Parses argv against program (no environment variable is set), writes the
 * error line of the result with flags into run->rec and releases the result.
 */
static void message_parse_and_write(clopts_program_t *program, int argc, char *const *argv, unsigned flags,
                                    message_run_t *run)
{
    clopts_result_t *result = clopts_parse_with_env(program, argc, argv, message_env_unset, NULL);

    run->status = clopts_result_get_status(result);
    run->usage = clopts_usage_error_get_kind(clopts_result_get_usage_error(result));
    text_recorder_init(&run->rec);
    run->write = clopts_write_error(program, result, flags, text_recorder_write, &run->rec);
    clopts_result_destroy(result);
}

/* ---- usage errors (7.1 to 7.9) ----------------------------------------- */

NAOS_TEST(unknown_command_names_the_token)
{
    char *argv[] = { "tool", "bulid", NULL };
    const char *expected = "unknown command 'bulid'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNKNOWN_COMMAND);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unknown_long_option_names_the_part_before_the_equals_sign)
{
    char *argv[] = { "tool", "build", "--bad=1", NULL };
    const char *expected = "unknown option '--bad'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unknown_long_option_without_value_names_the_whole_name)
{
    char *argv[] = { "tool", "--bad", NULL };
    const char *expected = "unknown option '--bad'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unknown_short_option_in_a_group_names_the_character)
{
    char *argv[] = { "tool", "build", "-vx", NULL };
    const char *expected = "unknown option '-x'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unknown_non_ascii_byte_in_a_group_names_the_whole_token)
{
    /* -v then U+00E9 (C3 A9): the offending byte is 0xC3, which is not ASCII. */
    char *argv[] = { "tool", "build", "-v\xC3\xA9", NULL };
    const char *expected = "unknown option '-v\xC3\xA9'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unknown_non_ascii_byte_first_in_a_group_names_the_whole_token)
{
    char *argv[] = { "tool", "-\xE6\x97\xA5", NULL };
    const char *expected = "unknown option '-\xE6\x97\xA5'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(missing_value_names_the_short_form_as_written)
{
    char *argv[] = { "tool", "build", "-o", NULL };
    const char *expected = "option '-o' requires a value\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_MISSING_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(missing_value_in_a_group_names_the_short_form_as_written)
{
    char *argv[] = { "tool", "build", "-vo", NULL };
    const char *expected = "option '-o' requires a value\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_MISSING_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(missing_value_names_the_long_form_as_written)
{
    char *argv[] = { "tool", "build", "--output", NULL };
    const char *expected = "option '--output' requires a value\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_MISSING_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unexpected_value_names_the_long_name_of_the_option)
{
    char *argv[] = { "tool", "--color=yes", NULL };
    const char *expected = "option '--color' does not take a value\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unexpected_value_to_a_negated_flag_names_the_long_name_of_the_option)
{
    char *argv[] = { "tool", "--no-color=yes", NULL };
    const char *expected = "option '--color' does not take a value\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(unexpected_value_to_a_terminal_option_names_the_part_before_the_equals_sign)
{
    /* --help and --version are terminal FLAGs: "=<value>" is an error, not a recovery. */
    char *argv[] = { "tool", "--help=x", NULL };
    char *argv_version[] = { "tool", "--version=", NULL };
    const char *expected = "option '--help' does not take a value\n";
    const char *expected_version = "option '--version' does not take a value\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_version), argv_version, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected_version, strlen(expected_version));
    text_fixture_tool_free(&f);
}

NAOS_TEST(missing_option_names_the_long_name)
{
    char *argv[] = { "tool", "build", "SRC", NULL };
    const char *expected = "missing required option '--output'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_MISSING_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(missing_option_without_a_long_name_names_the_short_name)
{
    /* A one-option declaration local to this test: -t VALUE required, no long name. */
    char *argv[] = { "short", NULL };
    const char *expected = "missing required option '-t'\n";
    clopts_program_t *program = test_decl_begin("short");
    clopts_option_t *target;
    message_run_t run;

    target = test_decl_option(CLOPTS_OPTION_VALUE, NULL, 't');
    NAOS_CHECK_INT_EQ(clopts_option_set_required(target, true), CLOPTS_DECL_ERROR_NONE);
    target = test_decl_global_attach(program, target);
    program = test_decl_finish(program);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_NOT_NULL(target);
    message_parse_and_write(program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_MISSING_OPTION);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    clopts_program_destroy(program);
}

/* Environment lookup that reports every variable as set to a value that cannot be read. */
static clopts_env_status_t message_env_invalid(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_INVALID;
}

NAOS_TEST(invalid_env_value_names_the_variable)
{
    /* A one-option declaration local to this test: --format VALUE with the variable TOOL_FORMAT. */
    char *argv[] = { "tool", NULL };
    const char *expected = "tool: error: invalid value in environment variable 'TOOL_FORMAT'\n";
    clopts_program_t *program = test_decl_begin("tool");
    clopts_option_t *format;
    clopts_result_t *result;
    text_recorder_t rec;

    format = test_decl_option(CLOPTS_OPTION_VALUE, "format", '\0');
    NAOS_CHECK_INT_EQ(clopts_option_set_env(format, "TOOL_FORMAT"), CLOPTS_DECL_ERROR_NONE);
    format = test_decl_global_attach(program, format);
    program = test_decl_finish(program);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_NOT_NULL(format);
    result = clopts_parse_with_env(program, MESSAGE_ARGC(argv), argv, message_env_invalid, NULL);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_usage_error_get_kind(clopts_result_get_usage_error(result)), CLOPTS_USAGE_ERROR_INVALID_ENV_VALUE);
    text_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(program, result, MESSAGE_PREFIX_BOTH, text_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    clopts_result_destroy(result);
    clopts_program_destroy(program);
}

NAOS_TEST(missing_argument_names_the_positional)
{
    char *argv[] = { "tool", "copy", "a", NULL };
    const char *expected = "missing argument <DST>\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_MISSING_ARGUMENT);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(missing_argument_of_a_program_without_subcommands)
{
    char *argv[] = { "filt", NULL };
    const char *expected = "missing argument <FILE>\n";
    text_fixture_filt_t f;
    message_run_t run;

    text_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_MISSING_ARGUMENT);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_filt_free(&f);
}

NAOS_TEST(unexpected_argument_names_the_token)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", NULL };
    const char *expected = "unexpected argument 'c'\n";
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, 0u, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(run.usage, CLOPTS_USAGE_ERROR_UNEXPECTED_ARGUMENT);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(usage_error_line_has_no_usage_hint)
{
    char *argv[] = { "tool", "build", "-o", NULL };
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
    NAOS_CHECK_NULL(strstr(run.rec.buf, "Run '"));
    NAOS_CHECK_NULL(strstr(run.rec.buf, "--help"));
    /* exactly one line, ending with LF */
    NAOS_REQUIRE_TRUE(run.rec.len > 0);
    NAOS_CHECK_INT_EQ(run.rec.buf[run.rec.len - 1], '\n');
    NAOS_CHECK_PTR_EQ(strchr(run.rec.buf, '\n'), run.rec.buf + run.rec.len - 1);
    text_fixture_tool_free(&f);
}

/* ---- out of memory and the other statuses (8.4, 8.5) ------------------- */

NAOS_TEST(null_result_is_out_of_memory)
{
    const char *expected = "out of memory\n";
    text_fixture_tool_t f;
    text_recorder_t rec;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    text_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(f.program, NULL, 0u, text_recorder_write, &rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(out_of_memory_takes_the_program_prefix)
{
    const char *expected = "tool: error: out of memory\n";
    text_fixture_tool_t f;
    text_recorder_t rec;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    text_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(f.program, NULL, MESSAGE_PREFIX_BOTH, text_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

NAOS_TEST(statuses_without_an_error_are_unavailable)
{
    char *argv_ok[] = { "tool", "build", "-o", "out", "SRC", NULL };
    char *argv_help[] = { "tool", "--help", NULL };
    char *argv_version[] = { "tool", "--version", NULL };
    char *argv_recovered[] = { "tool", "--bad", "--help", NULL };
    char *argv_no_command[] = { "tool", NULL };
    text_fixture_tool_t f;
    message_run_t run;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_ok), argv_ok, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_help), argv_help, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_version), argv_version, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    /* A terminal option after a usage error replaces the error: there is no error line. */
    message_parse_and_write(f.program, MESSAGE_ARGC(argv_recovered), argv_recovered, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    message_parse_and_write(f.program, MESSAGE_ARGC(argv_no_command), argv_no_command, MESSAGE_PREFIX_BOTH, &run);
    NAOS_CHECK_INT_EQ(run.status, CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(run.rec.calls, 0);

    text_fixture_tool_free(&f);
}

/* ---- prefixes (9.1 to 9.6) --------------------------------------------- */

NAOS_TEST(prefixes_without_an_error_command)
{
    /* The unknown command is reported for the program: no command in the prefix. */
    char *argv[] = { "tool", "bulid", NULL };
    const char *expected[4] = {
        "unknown command 'bulid'\n",
        "tool: unknown command 'bulid'\n",
        "error: unknown command 'bulid'\n",
        "tool: error: unknown command 'bulid'\n",
    };
    text_fixture_tool_t f;
    message_run_t run;
    unsigned flags;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, flags, &run);
        NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
        NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected[flags], strlen(expected[flags]));
    }
    text_fixture_tool_free(&f);
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
    text_fixture_tool_t f;
    message_run_t run;
    unsigned flags;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, flags, &run);
        NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
        NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected[flags], strlen(expected[flags]));
    }
    text_fixture_tool_free(&f);
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
    text_fixture_filt_t f;
    message_run_t run;
    unsigned flags;

    text_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        message_parse_and_write(f.program, MESSAGE_ARGC(argv), argv, flags, &run);
        NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
        NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, expected[flags], strlen(expected[flags]));
    }
    text_fixture_filt_free(&f);
}

NAOS_TEST(undefined_prefix_bits_are_ignored)
{
    char *argv_command[] = { "tool", "build", "-o", NULL };
    char *argv_no_command[] = { "tool", "bulid", NULL };
    const unsigned extra[3] = { 0x4u, 0x80000000u, ~(unsigned)MESSAGE_PREFIX_BOTH };
    text_fixture_tool_t f;
    message_run_t plain;
    message_run_t run;
    unsigned flags;
    int i;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    for (flags = 0u; flags < 4u; flags++)
    {
        for (i = 0; i < 3; i++)
        {
            message_parse_and_write(f.program, MESSAGE_ARGC(argv_command), argv_command, flags, &plain);
            message_parse_and_write(f.program, MESSAGE_ARGC(argv_command), argv_command, flags | extra[i], &run);
            NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
            NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, plain.rec.buf, plain.rec.len);

            message_parse_and_write(f.program, MESSAGE_ARGC(argv_no_command), argv_no_command, flags, &plain);
            message_parse_and_write(f.program, MESSAGE_ARGC(argv_no_command), argv_no_command, flags | extra[i], &run);
            NAOS_CHECK_INT_EQ(run.write, CLOPTS_WRITE_OK);
            NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, plain.rec.buf, plain.rec.len);
        }
    }

    /* ~0u holds both defined bits, so it reads as PROGRAM | ERROR. */
    message_parse_and_write(f.program, MESSAGE_ARGC(argv_command), argv_command, ~0u, &run);
    NAOS_CHECK_MEM_EQ(run.rec.buf, run.rec.len, "tool build: error: option '-o' requires a value\n",
                      strlen("tool build: error: option '-o' requires a value\n"));
    text_fixture_tool_free(&f);
}

NAOS_TEST(undefined_prefix_bits_are_ignored_for_out_of_memory)
{
    const char *expected = "out of memory\n";
    text_fixture_tool_t f;
    text_recorder_t rec;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    text_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_error(f.program, NULL, ~(unsigned)MESSAGE_PREFIX_BOTH, text_recorder_write, &rec),
                      CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));
    text_fixture_tool_free(&f);
}

/* ---- version line and usage hint --------------------------------------- */

/* The version line of program with version is exactly expected. */
#define CHECK_VERSION(program, version, expected)                                                 \
    do                                                                                            \
    {                                                                                             \
        static text_recorder_t rec_;                                                              \
        text_recorder_init(&rec_);                                                                \
        NAOS_CHECK_INT_EQ(clopts_write_version((program), (version), text_recorder_write, &rec_), \
                          CLOPTS_WRITE_OK);                                                       \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, (expected), strlen(expected));                      \
    } while (0)

NAOS_TEST(version_line)
{
    text_fixture_tool_t f;
    text_fixture_bare_t b;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_VERSION(f.program, "1.2.3", "tool 1.2.3\n");
    text_fixture_tool_free(&f);

    text_fixture_bare_init(&b);
    NAOS_REQUIRE_NOT_NULL(b.program);
    CHECK_VERSION(b.program, "0.1", "bare 0.1\n");
    text_fixture_bare_free(&b);
}

NAOS_TEST(version_line_does_not_need_a_version_option)
{
    /* The version string is the caller's: a program without --version has a version line too. */
    text_fixture_filt_t f;
    clopts_program_t *program;

    text_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_VERSION(f.program, "2.0", "filt 2.0\n");
    text_fixture_filt_free(&f);

    /* An empty version is written as is. */
    program = test_decl_program("empty");
    NAOS_REQUIRE_NOT_NULL(program);
    CHECK_VERSION(program, "", "empty \n");
    clopts_program_destroy(program);
}

NAOS_TEST(version_line_unavailable_without_version)
{
    static text_recorder_t rec;
    text_fixture_tool_t f;
    text_fixture_filt_t g;

    /* A NULL version gives UNAVAILABLE, even when the program declares --version. */
    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    text_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(f.program, NULL, text_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    text_fixture_tool_free(&f);

    text_fixture_filt_init(&g);
    NAOS_REQUIRE_NOT_NULL(g.program);
    text_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(g.program, NULL, text_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    text_fixture_filt_free(&g);
}

/* The usage hint of command (NULL: the program) of program is exactly expected. */
#define CHECK_HINT(program, command, expected)                                                       \
    do                                                                                               \
    {                                                                                                \
        static text_recorder_t rec_;                                                                 \
        text_recorder_init(&rec_);                                                                   \
        NAOS_CHECK_INT_EQ(clopts_write_usage_hint((program), (command), text_recorder_write, &rec_), \
                          CLOPTS_WRITE_OK);                                                          \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, (expected), strlen(expected));                         \
    } while (0)

NAOS_TEST(usage_hint_without_command)
{
    text_fixture_tool_t f;
    text_fixture_filt_t g;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HINT(f.program, NULL, "Run 'tool --help' for usage.\n");
    text_fixture_tool_free(&f);

    text_fixture_filt_init(&g);
    NAOS_REQUIRE_NOT_NULL(g.program);
    CHECK_HINT(g.program, NULL, "Run 'filt --help' for usage.\n");
    text_fixture_filt_free(&g);
}

NAOS_TEST(usage_hint_with_command)
{
    text_fixture_tool_t f;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HINT(f.program, f.build, "Run 'tool build --help' for usage.\n");
    CHECK_HINT(f.program, f.copy, "Run 'tool copy --help' for usage.\n");
    text_fixture_tool_free(&f);
}

/* ---- declaration error texts ------------------------------------------ */

/* Largest value of each kind enum; a new kind must raise them (12.2). */
#define MESSAGE_USAGE_KIND_MAX 8
#define MESSAGE_DECL_KIND_MAX 17

NAOS_TEST(decl_error_message_of_every_kind)
{
    static const struct
    {
        clopts_decl_error_t kind;
        const char *text;
    } cases[] = {
        { CLOPTS_DECL_ERROR_INVALID_NAME, "invalid name" },
        { CLOPTS_DECL_ERROR_NO_NAME, "option has neither a long nor a short name" },
        { CLOPTS_DECL_ERROR_DUPLICATE_COMMAND, "duplicate command name" },
        { CLOPTS_DECL_ERROR_DUPLICATE_OPTION, "duplicate option name" },
        { CLOPTS_DECL_ERROR_NOT_NEGATABLE, "only a flag can be negatable" },
        { CLOPTS_DECL_ERROR_NEGATION_CONFLICT, "conflicts with the negated form of a flag" },
        { CLOPTS_DECL_ERROR_DEFAULT_NOT_ALLOWED, "a flag or count cannot have a default value" },
        { CLOPTS_DECL_ERROR_ENV_NOT_ALLOWED, "a flag or count cannot have an environment variable" },
        { CLOPTS_DECL_ERROR_REQUIRED_WITH_DEFAULT, "a required option cannot have a default value" },
        { CLOPTS_DECL_ERROR_REQUIRED_NOT_ALLOWED, "a flag or count cannot be required" },
        { CLOPTS_DECL_ERROR_POSITIONAL_ORDER, "a required argument cannot follow an optional one" },
        { CLOPTS_DECL_ERROR_VARIADIC_POSITION, "only the last argument can be variadic" },
        { CLOPTS_DECL_ERROR_ATTRIBUTE_KIND, "attribute does not apply to this kind of option" },
        { CLOPTS_DECL_ERROR_MIXED_LAYOUT, "a program cannot have both arguments and subcommands" },
        { CLOPTS_DECL_ERROR_INVALID_KIND, "invalid option kind" },
        { CLOPTS_DECL_ERROR_INVALID_ARGUMENT, "invalid argument" },
        { CLOPTS_DECL_ERROR_OUT_OF_MEMORY, "out of memory" },
    };
    size_t i;

    NAOS_CHECK_UINT_EQ(sizeof(cases) / sizeof(cases[0]), MESSAGE_DECL_KIND_MAX);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const char *text = clopts_decl_error_get_message(cases[i].kind);

        NAOS_TRACE_PUSH("kind %d", (int)cases[i].kind);
        NAOS_REQUIRE_NOT_NULL(text);
        NAOS_CHECK_STR_EQ(text, cases[i].text);
        NAOS_TRACE_POP();
    }
}

NAOS_TEST(decl_error_message_of_a_builder_error)
{
    /* The kind returned by a failed builder call has its text. */
    clopts_program_t *program = test_decl_begin("tool");
    clopts_option_t *first;
    clopts_option_t *second;

    NAOS_REQUIRE_NOT_NULL(program);
    first = test_decl_global_add(program, CLOPTS_OPTION_VALUE, "out", 'o');
    NAOS_REQUIRE_NOT_NULL(first);
    second = test_decl_global_add(program, CLOPTS_OPTION_FLAG, "out", '\0');
    NAOS_CHECK_NULL(second);
    NAOS_REQUIRE_INT_EQ(test_decl_last, CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    NAOS_CHECK_STR_EQ(clopts_decl_error_get_message(test_decl_last), "duplicate option name");
    test_decl_discard(program);
}

NAOS_TEST(decl_error_message_of_a_mixed_layout)
{
    /* A subcommand added to a program with positionals is refused with the layout text. */
    clopts_program_t *program = test_decl_begin("tool");
    clopts_command_t *command = NULL;

    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add_positional(program, "FILE"));
    NAOS_REQUIRE_INT_EQ(clopts_command_create("run", &command), CLOPTS_DECL_ERROR_NONE);
    NAOS_REQUIRE_INT_EQ(clopts_program_add_command(program, command), CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    NAOS_CHECK_STR_EQ(clopts_decl_error_get_message(CLOPTS_DECL_ERROR_MIXED_LAYOUT),
                      "a program cannot have both arguments and subcommands");
    /* The failed call left the command with the caller. */
    clopts_command_destroy(command);
    test_decl_discard(program);
}

/* ---- coverage of the text tables (12.1, 12.2) -------------------------- */

NAOS_TEST(usage_text_covers_every_kind)
{
    const char *texts[MESSAGE_USAGE_KIND_MAX + 1];
    int i;
    int j;

    NAOS_CHECK_NULL(clopts_text_usage_text(CLOPTS_USAGE_ERROR_NONE));
    NAOS_CHECK_NULL(clopts_text_usage_text((clopts_usage_error_kind_t)(MESSAGE_USAGE_KIND_MAX + 1)));
    for (i = 1; i <= MESSAGE_USAGE_KIND_MAX; i++)
    {
        texts[i] = clopts_text_usage_text((clopts_usage_error_kind_t)i);
        NAOS_REQUIRE_NOT_NULL(texts[i]);
        NAOS_CHECK_TRUE(texts[i][0] != '\0');
        for (j = 1; j < i; j++)
        {
            NAOS_CHECK_TRUE(strcmp(texts[i], texts[j]) != 0);
        }
    }
}

NAOS_TEST(decl_error_message_covers_every_kind)
{
    const char *texts[MESSAGE_DECL_KIND_MAX + 1];
    int i;
    int j;

    NAOS_CHECK_NULL(clopts_decl_error_get_message(CLOPTS_DECL_ERROR_NONE));
    NAOS_CHECK_NULL(clopts_decl_error_get_message((clopts_decl_error_t)(MESSAGE_DECL_KIND_MAX + 1)));
    for (i = 1; i <= MESSAGE_DECL_KIND_MAX; i++)
    {
        texts[i] = clopts_decl_error_get_message((clopts_decl_error_t)i);
        NAOS_REQUIRE_NOT_NULL(texts[i]);
        NAOS_CHECK_TRUE(texts[i][0] != '\0');
        for (j = 1; j < i; j++)
        {
            NAOS_CHECK_TRUE(strcmp(texts[i], texts[j]) != 0);
        }
    }
}

/* ---- the failing callback of the fixtures ------------------------------ */

NAOS_TEST(failer_fails_on_the_nth_call_and_counts_later_calls)
{
    text_failer_t failer;

    text_failer_init(&failer, 3);
    NAOS_CHECK_UINT_EQ(failer.fail_at, 3);
    NAOS_CHECK_UINT_EQ(failer.calls, 0);
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(text_failer_write(&failer, "ab", 2), 0);
    NAOS_CHECK_INT_EQ(text_failer_write(&failer, "c", 1), 0);
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(text_failer_write(&failer, "d", 1), 1);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 3);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(text_failer_write(&failer, "e", 1), 1);
    NAOS_CHECK_INT_EQ(text_failer_write(&failer, "f", 0), 1);
    NAOS_CHECK_UINT_EQ(failer.calls, 5);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 2);

    /* only the bytes before the failing call were accepted */
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "abc", (size_t)3);
}

NAOS_TEST(failer_fails_on_the_first_call)
{
    text_failer_t failer;

    text_failer_init(&failer, 1);
    NAOS_CHECK_INT_EQ(text_failer_write(&failer, "a", 1), 1);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 1);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_UINT_EQ(failer.written.len, 0);
}

NAOS_TEST(failer_with_zero_never_fails)
{
    clopts_write_fn fn = text_failer_write;
    text_failer_t failer;
    int i;

    text_failer_init(&failer, 0);
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
    NAOS_ADD_TEST(unexpected_value_to_a_terminal_option_names_the_part_before_the_equals_sign),
    NAOS_ADD_TEST(missing_option_names_the_long_name),
    NAOS_ADD_TEST(invalid_env_value_names_the_variable),
    NAOS_ADD_TEST(missing_option_without_a_long_name_names_the_short_name),
    NAOS_ADD_TEST(missing_argument_names_the_positional),
    NAOS_ADD_TEST(missing_argument_of_a_program_without_subcommands),
    NAOS_ADD_TEST(unexpected_argument_names_the_token),
    NAOS_ADD_TEST(usage_error_line_has_no_usage_hint),
    NAOS_ADD_TEST(null_result_is_out_of_memory),
    NAOS_ADD_TEST(out_of_memory_takes_the_program_prefix),
    NAOS_ADD_TEST(statuses_without_an_error_are_unavailable),
    NAOS_ADD_TEST(prefixes_without_an_error_command),
    NAOS_ADD_TEST(prefixes_with_an_error_command),
    NAOS_ADD_TEST(prefixes_of_a_program_without_subcommands),
    NAOS_ADD_TEST(undefined_prefix_bits_are_ignored),
    NAOS_ADD_TEST(undefined_prefix_bits_are_ignored_for_out_of_memory),
    NAOS_ADD_TEST(version_line),
    NAOS_ADD_TEST(version_line_does_not_need_a_version_option),
    NAOS_ADD_TEST(version_line_unavailable_without_version),
    NAOS_ADD_TEST(usage_hint_without_command),
    NAOS_ADD_TEST(usage_hint_with_command),
    NAOS_ADD_TEST(decl_error_message_of_every_kind),
    NAOS_ADD_TEST(decl_error_message_of_a_builder_error),
    NAOS_ADD_TEST(decl_error_message_of_a_mixed_layout),
    NAOS_ADD_TEST(usage_text_covers_every_kind),
    NAOS_ADD_TEST(decl_error_message_covers_every_kind),
    NAOS_ADD_TEST(failer_fails_on_the_nth_call_and_counts_later_calls),
    NAOS_ADD_TEST(failer_fails_on_the_first_call),
    NAOS_ADD_TEST(failer_with_zero_never_fails)
)
