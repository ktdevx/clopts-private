/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Messages: the one-line texts: version line, usage hint, and the error
 * line of a result with its prefix and message tables.
 *
 * The version line is "<program> <version>\n" and the usage hint is
 * "Run '<program> [<command>] --help' for usage.\n". The error line is
 * [prefix] + body + "\n"; the status of the result selects the body (usage
 * error text, out of memory). Whether there is a line at all is decided by
 * write.c before anything is written. No usage hint is part of the line.
 *
 * Strings of the declaration and of argv are written as they are; nothing is
 * allocated and neither the declaration nor the result is modified.
 */

#include "text/message.h"

#include <assert.h>
#include <string.h>

/* The prefix flags this version defines; any other bit is ignored. */
#define PREFIX_DEFINED (CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR)

/* A string "has text": it is neither NULL nor empty. */
static bool has_text(const char *s)
{
    return (s != NULL) && (s[0] != '\0');
}

/* A short_name reported by the scanner that is a single ASCII character (0x01 to 0x7F). */
static bool is_ascii(char c)
{
    const unsigned char byte = (unsigned char)c;

    return (byte >= 0x01u) && (byte <= 0x7Fu);
}

/* Writes s, or nothing when s is NULL. */
static void write_str(clopts_out_t *out, const char *s)
{
    if (s != NULL)
    {
        clopts_out_str(out, s);
    }
}

/* Writes "-<c>". */
static void write_short(clopts_out_t *out, char c)
{
    clopts_out_char(out, '-');
    clopts_out_char(out, c);
}

/* Writes the part of token before the first "=" (all of it without one). */
static void write_token_name(clopts_out_t *out, const char *token)
{
    if (token != NULL)
    {
        clopts_out_bytes(out, token, strcspn(token, "="));
    }
}

/* Writes "--<long>" of option, or "-<c>" when it has no long name. */
static void write_option_name(clopts_out_t *out, const clopts_option_t *option)
{
    if (has_text(clopts_option_get_long_name(option)))
    {
        clopts_out_str(out, "--");
        clopts_out_str(out, clopts_option_get_long_name(option));
    }
    else
    {
        write_short(out, clopts_option_get_short_name(option));
    }
}

/*
 * Writes the option as the user wrote it: "-<c>" for an ASCII short_name,
 * else "--<long>" or "-<c>" of the option, else the token before "=".
 */
static void write_written_option(clopts_out_t *out, const clopts_usage_error_t *error)
{
    const char short_name = clopts_usage_error_get_short_name(error);
    const clopts_option_t *option = clopts_usage_error_get_option(error);

    if (is_ascii(short_name))
    {
        write_short(out, short_name);
    }
    else if ((option != NULL) && (has_text(clopts_option_get_long_name(option)) || (clopts_option_get_short_name(option) != '\0')))
    {
        write_option_name(out, option);
    }
    else
    {
        write_token_name(out, clopts_usage_error_get_token(error));
    }
}

/* Writes the argument of an UNKNOWN_OPTION error. */
static void write_unknown_option(clopts_out_t *out, const clopts_usage_error_t *error)
{
    const char short_name = clopts_usage_error_get_short_name(error);
    const size_t name_len = clopts_usage_error_get_name_len(error);
    const char *token = clopts_usage_error_get_token(error);

    if (is_ascii(short_name))
    {
        write_short(out, short_name);
    }
    else if ((name_len > 0) && (token != NULL))
    {
        clopts_out_bytes(out, token, name_len);
    }
    else
    {
        /* A non-ASCII byte inside a short option group: the whole token. */
        write_str(out, token);
    }
}

/* Writes the argument of an UNEXPECTED_VALUE error. */
static void write_unexpected_value(clopts_out_t *out, const clopts_usage_error_t *error)
{
    const clopts_option_t *option = clopts_usage_error_get_option(error);

    if (option == NULL)
    {
        /* No option recorded: the token before "=". */
        write_token_name(out, clopts_usage_error_get_token(error));
    }
    else if (has_text(clopts_option_get_long_name(option)))
    {
        write_option_name(out, option);
    }
    else
    {
        write_written_option(out, error);
    }
}

/* Writes the argument of error, of kind: '<argument>', or <NAME> for a missing argument. */
static void write_usage_argument(clopts_out_t *out, const clopts_usage_error_t *error,
                                         clopts_usage_error_kind_t kind)
{
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const bool positional_name = (kind == CLOPTS_USAGE_ERROR_MISSING_ARGUMENT);

    clopts_out_char(out, positional_name ? '<' : '\'');
    switch (kind)
    {
    case CLOPTS_USAGE_ERROR_NONE:
        break;
    case CLOPTS_USAGE_ERROR_UNKNOWN_COMMAND:
    case CLOPTS_USAGE_ERROR_UNEXPECTED_ARGUMENT:
        write_str(out, clopts_usage_error_get_token(error));
        break;
    case CLOPTS_USAGE_ERROR_UNKNOWN_OPTION:
        write_unknown_option(out, error);
        break;
    case CLOPTS_USAGE_ERROR_MISSING_VALUE:
        write_written_option(out, error);
        break;
    case CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE:
        write_unexpected_value(out, error);
        break;
    case CLOPTS_USAGE_ERROR_MISSING_OPTION:
        option = clopts_usage_error_get_option(error);
        if (option != NULL)
        {
            write_option_name(out, option);
        }
        break;
    case CLOPTS_USAGE_ERROR_MISSING_ARGUMENT:
        positional = clopts_usage_error_get_positional(error);
        if (positional != NULL)
        {
            write_str(out, clopts_positional_get_name(positional));
        }
        break;
    case CLOPTS_USAGE_ERROR_INVALID_ENV_VALUE:
        option = clopts_usage_error_get_option(error);
        if (option != NULL)
        {
            write_str(out, clopts_option_get_env(option));
        }
        break;
    }
    clopts_out_char(out, positional_name ? '>' : '\'');
}

/*
 * Writes the body of a usage error: text is clopts_message_usage_text(kind).
 *   - MISSING_VALUE / UNEXPECTED_VALUE: "option '<opt>' <text>"
 *   - any other kind: "<text> <argument>"
 */
static void write_usage_body(clopts_out_t *out, const clopts_usage_error_t *error,
                                     clopts_usage_error_kind_t kind, const char *text)
{
    if ((kind == CLOPTS_USAGE_ERROR_MISSING_VALUE) || (kind == CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE))
    {
        clopts_out_str(out, "option ");
        write_usage_argument(out, error, kind);
        clopts_out_char(out, ' ');
        clopts_out_str(out, text);
    }
    else
    {
        clopts_out_str(out, text);
        clopts_out_char(out, ' ');
        write_usage_argument(out, error, kind);
    }
}

/*
 * Writes the prefix selected by the defined bits of flags:
 * "<program>: " or "<program> <command>: " (PROGRAM), then "error: " (ERROR).
 * A program name without text drops the program part; a command name without
 * text leaves the program name alone.
 */
static void write_prefix(clopts_out_t *out, const clopts_program_t *program,
                                 const clopts_command_t *command, unsigned flags)
{
    flags &= PREFIX_DEFINED;

    if (((flags & CLOPTS_PREFIX_PROGRAM) != 0u) && has_text(clopts_program_get_name(program)))
    {
        clopts_out_str(out, clopts_program_get_name(program));
        if ((command != NULL) && has_text(clopts_command_get_name(command)))
        {
            clopts_out_char(out, ' ');
            clopts_out_str(out, clopts_command_get_name(command));
        }
        clopts_out_str(out, ": ");
    }
    if ((flags & CLOPTS_PREFIX_ERROR) != 0u)
    {
        clopts_out_str(out, "error: ");
    }
}

void clopts_message_write_error(clopts_out_t *out, const clopts_program_t *program, const clopts_result_t *result,
                                unsigned flags)
{
    const clopts_usage_error_t *error = clopts_result_get_usage_error(result);
    clopts_usage_error_kind_t kind;

    assert((out != NULL) && (program != NULL));

    if (clopts_result_get_status(result) == CLOPTS_PARSE_USAGE_ERROR)
    {
        kind = clopts_usage_error_get_kind(error);
        assert(clopts_message_usage_text(kind) != NULL);
        write_prefix(out, program, clopts_usage_error_get_command(error), flags);
        write_usage_body(out, error, kind, clopts_message_usage_text(kind));
    }
    else
    {
        assert(clopts_result_get_status(result) == CLOPTS_PARSE_OUT_OF_MEMORY);
        write_prefix(out, program, NULL, flags);
        clopts_out_str(out, "out of memory");
    }
    clopts_out_char(out, '\n');
}

void clopts_message_write_version(clopts_out_t *out, const clopts_program_t *program, const char *version)
{
    assert((out != NULL) && (program != NULL) && (version != NULL));

    clopts_out_str(out, clopts_program_get_name(program));
    clopts_out_char(out, ' ');
    clopts_out_str(out, version);
    clopts_out_char(out, '\n');
}

void clopts_message_write_usage_hint(clopts_out_t *out, const clopts_program_t *program,
                                     const clopts_command_t *command)
{
    assert((out != NULL) && (program != NULL));

    clopts_out_str(out, "Run '");
    clopts_out_str(out, clopts_program_get_name(program));
    if (command != NULL)
    {
        clopts_out_char(out, ' ');
        clopts_out_str(out, clopts_command_get_name(command));
    }
    clopts_out_str(out, " --help' for usage.\n");
}

const char *clopts_message_usage_text(clopts_usage_error_kind_t kind)
{
    switch (kind)
    {
    case CLOPTS_USAGE_ERROR_NONE:
        return NULL;
    case CLOPTS_USAGE_ERROR_UNKNOWN_COMMAND:
        return "unknown command";
    case CLOPTS_USAGE_ERROR_UNKNOWN_OPTION:
        return "unknown option";
    case CLOPTS_USAGE_ERROR_MISSING_VALUE:
        return "requires a value";
    case CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE:
        return "does not take a value";
    case CLOPTS_USAGE_ERROR_MISSING_OPTION:
        return "missing required option";
    case CLOPTS_USAGE_ERROR_MISSING_ARGUMENT:
        return "missing argument";
    case CLOPTS_USAGE_ERROR_UNEXPECTED_ARGUMENT:
        return "unexpected argument";
    case CLOPTS_USAGE_ERROR_INVALID_ENV_VALUE:
        return "invalid value in environment variable";
    }
    return NULL;
}
