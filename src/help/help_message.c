/*
 * HelpMessage: error line of a result, its prefix, and the message tables.
 *
 * The line is [prefix] + body + "\n". The status of the result selects the
 * body (usage error text, declaration error reason, out of memory); for any
 * other status nothing is written. The prefix is written only once the body
 * is known, so an unavailable line never calls write_fn. No usage hint is
 * part of the line.
 *
 * Strings of the declaration and of argv are written as they are; nothing is
 * allocated and neither the declaration nor the result is modified.
 */

#include "help/help_internal.h"

#include <assert.h>
#include <string.h>

/* The prefix flags this version defines; any other bit is ignored. */
#define MESSAGE_PREFIX_DEFINED (CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR)

/* A string "has text": it is neither NULL nor empty. */
static bool message_has_text(const char *s)
{
    return (s != NULL) && (s[0] != '\0');
}

/* A short_name reported by the scanner that is a single ASCII character (0x01 to 0x7F). */
static bool message_is_ascii(char c)
{
    const unsigned char byte = (unsigned char)c;

    return (byte >= 0x01u) && (byte <= 0x7Fu);
}

/* Writes s, or nothing when s is NULL. */
static void message_write_str(clopts_help_out_t *out, const char *s)
{
    if (s != NULL)
    {
        clopts_help_out_str(out, s);
    }
}

/* Writes "-<c>". */
static void message_write_short(clopts_help_out_t *out, char c)
{
    clopts_help_out_char(out, '-');
    clopts_help_out_char(out, c);
}

/* Writes the part of token before the first "=" (all of it without one). */
static void message_write_token_name(clopts_help_out_t *out, const char *token)
{
    if (token != NULL)
    {
        clopts_help_out_bytes(out, token, strcspn(token, "="));
    }
}

/* Writes "--<long>" of option, or "-<c>" when it has no long name. */
static void message_write_option_name(clopts_help_out_t *out, const clopts_option_t *option)
{
    if (message_has_text(option->long_name))
    {
        clopts_help_out_str(out, "--");
        clopts_help_out_str(out, option->long_name);
    }
    else
    {
        message_write_short(out, option->short_name);
    }
}

/*
 * Writes the option as the user wrote it: "-<c>" for an ASCII short_name,
 * else "--<long>" or "-<c>" of the option, else the token before "=".
 */
static void message_write_written_option(clopts_help_out_t *out, const clopts_result_t *result)
{
    const char short_name = clopts_result_error_short_name(result);
    const clopts_option_t *option = clopts_result_error_option(result);

    if (message_is_ascii(short_name))
    {
        message_write_short(out, short_name);
    }
    else if ((option != NULL) && (message_has_text(option->long_name) || (option->short_name != '\0')))
    {
        message_write_option_name(out, option);
    }
    else
    {
        message_write_token_name(out, clopts_result_error_token(result));
    }
}

/* Writes the argument of an UNKNOWN_OPTION error. */
static void message_write_unknown_option(clopts_help_out_t *out, const clopts_result_t *result)
{
    const char short_name = clopts_result_error_short_name(result);
    const size_t name_len = clopts_result_error_name_len(result);
    const char *token = clopts_result_error_token(result);

    if (message_is_ascii(short_name))
    {
        message_write_short(out, short_name);
    }
    else if ((name_len > 0) && (token != NULL))
    {
        clopts_help_out_bytes(out, token, name_len);
    }
    else
    {
        /* A non-ASCII byte inside a short option group: the whole token. */
        message_write_str(out, token);
    }
}

/* Writes the argument of an UNEXPECTED_VALUE error. */
static void message_write_unexpected_value(clopts_help_out_t *out, const clopts_result_t *result)
{
    const clopts_option_t *option = clopts_result_error_option(result);

    if (option == NULL)
    {
        /* A reserved option (--help, --version). */
        message_write_token_name(out, clopts_result_error_token(result));
    }
    else if (message_has_text(option->long_name))
    {
        message_write_option_name(out, option);
    }
    else
    {
        message_write_written_option(out, result);
    }
}

/* Writes the argument of the usage error kind of result: '<argument>', or <NAME> for a missing argument. */
static void message_write_usage_argument(clopts_help_out_t *out, const clopts_result_t *result,
                                         clopts_usage_error_t kind)
{
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const bool positional_name = (kind == CLOPTS_USAGE_MISSING_ARGUMENT);

    clopts_help_out_char(out, positional_name ? '<' : '\'');
    switch (kind)
    {
    case CLOPTS_USAGE_NONE:
        break;
    case CLOPTS_USAGE_UNKNOWN_COMMAND:
    case CLOPTS_USAGE_UNEXPECTED_ARGUMENT:
        message_write_str(out, clopts_result_error_token(result));
        break;
    case CLOPTS_USAGE_UNKNOWN_OPTION:
        message_write_unknown_option(out, result);
        break;
    case CLOPTS_USAGE_MISSING_VALUE:
        message_write_written_option(out, result);
        break;
    case CLOPTS_USAGE_UNEXPECTED_VALUE:
        message_write_unexpected_value(out, result);
        break;
    case CLOPTS_USAGE_MISSING_OPTION:
        option = clopts_result_error_option(result);
        if (option != NULL)
        {
            message_write_option_name(out, option);
        }
        break;
    case CLOPTS_USAGE_MISSING_ARGUMENT:
        positional = clopts_result_error_positional(result);
        if (positional != NULL)
        {
            message_write_str(out, positional->name);
        }
        break;
    }
    clopts_help_out_char(out, positional_name ? '>' : '\'');
}

/*
 * Writes the body of a usage error: text is clopts_help_usage_text(kind).
 *   - MISSING_VALUE / UNEXPECTED_VALUE: "option '<opt>' <text>"
 *   - any other kind: "<text> <argument>"
 */
static void message_write_usage_body(clopts_help_out_t *out, const clopts_result_t *result,
                                     clopts_usage_error_t kind, const char *text)
{
    if ((kind == CLOPTS_USAGE_MISSING_VALUE) || (kind == CLOPTS_USAGE_UNEXPECTED_VALUE))
    {
        clopts_help_out_str(out, "option ");
        message_write_usage_argument(out, result, kind);
        clopts_help_out_char(out, ' ');
        clopts_help_out_str(out, text);
    }
    else
    {
        clopts_help_out_str(out, text);
        clopts_help_out_char(out, ' ');
        message_write_usage_argument(out, result, kind);
    }
}

/* Writes " in command '<name>'" when command is a subcommand with a name. */
static void message_write_in_command(clopts_help_out_t *out, const clopts_command_t *command)
{
    if ((command != NULL) && message_has_text(command->name))
    {
        clopts_help_out_str(out, " in command '");
        clopts_help_out_str(out, command->name);
        clopts_help_out_char(out, '\'');
    }
}

/*
 * Writes the subject of a declaration error, " (<subject>)", from the first
 * of these that has a name: the option, the positional, the command. An
 * option or a positional of a subcommand is followed by the command. Nothing
 * is written when none has a name.
 */
static void message_write_decl_subject(clopts_help_out_t *out, const clopts_result_t *result)
{
    const clopts_command_t *command = clopts_result_error_command(result);
    const clopts_option_t *option = clopts_result_error_option(result);
    const clopts_positional_t *positional = clopts_result_error_positional(result);

    if ((option != NULL) && (message_has_text(option->long_name) || (option->short_name != '\0')))
    {
        clopts_help_out_str(out, " (option '");
        message_write_option_name(out, option);
        clopts_help_out_char(out, '\'');
        message_write_in_command(out, command);
        clopts_help_out_char(out, ')');
    }
    else if ((positional != NULL) && message_has_text(positional->name))
    {
        clopts_help_out_str(out, " (argument <");
        clopts_help_out_str(out, positional->name);
        clopts_help_out_char(out, '>');
        message_write_in_command(out, command);
        clopts_help_out_char(out, ')');
    }
    else if ((command != NULL) && message_has_text(command->name))
    {
        clopts_help_out_str(out, " (command '");
        clopts_help_out_str(out, command->name);
        clopts_help_out_str(out, "')");
    }
}

/*
 * Writes the prefix selected by the defined bits of flags:
 * "<program>: " or "<program> <command>: " (PROGRAM), then "error: " (ERROR).
 * A program name without text drops the program part; a command name without
 * text leaves the program name alone.
 */
static void message_write_prefix(clopts_help_out_t *out, const clopts_program_t *program,
                                 const clopts_command_t *command, unsigned flags)
{
    flags &= MESSAGE_PREFIX_DEFINED;

    if (((flags & CLOPTS_PREFIX_PROGRAM) != 0u) && message_has_text(program->root.name))
    {
        clopts_help_out_str(out, program->root.name);
        if ((command != NULL) && message_has_text(command->name))
        {
            clopts_help_out_char(out, ' ');
            clopts_help_out_str(out, command->name);
        }
        clopts_help_out_str(out, ": ");
    }
    if ((flags & CLOPTS_PREFIX_ERROR) != 0u)
    {
        clopts_help_out_str(out, "error: ");
    }
}

clopts_write_status_t clopts_write_error(const clopts_program_t *program, const clopts_result_t *result,
                                         unsigned flags, clopts_write_fn write_fn, void *ctx)
{
    const clopts_command_t *command;
    clopts_usage_error_t kind;
    const char *text;
    clopts_help_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);

    command = clopts_result_error_command(result);
    assert((command == NULL) || (command->program == program));

    switch (clopts_result_status(result))
    {
    case CLOPTS_USAGE_ERROR:
        kind = clopts_result_usage_error(result);
        text = clopts_help_usage_text(kind);
        if (text == NULL)
        {
            return CLOPTS_WRITE_UNAVAILABLE;
        }
        clopts_help_out_init(&out, write_fn, ctx);
        message_write_prefix(&out, program, command, flags);
        message_write_usage_body(&out, result, kind, text);
        break;
    case CLOPTS_OUT_OF_MEMORY:
        clopts_help_out_init(&out, write_fn, ctx);
        message_write_prefix(&out, program, NULL, flags);
        clopts_help_out_str(&out, "out of memory");
        break;
    case CLOPTS_DECL_ERROR:
        text = clopts_help_decl_reason(clopts_result_decl_error(result));
        if (text == NULL)
        {
            return CLOPTS_WRITE_UNAVAILABLE;
        }
        clopts_help_out_init(&out, write_fn, ctx);
        message_write_prefix(&out, program, command, flags);
        clopts_help_out_str(&out, "invalid declaration: ");
        clopts_help_out_str(&out, text);
        message_write_decl_subject(&out, result);
        break;
    default:
        /* OK / HELP / VERSION / NO_COMMAND: no error to describe. */
        return CLOPTS_WRITE_UNAVAILABLE;
    }
    clopts_help_out_char(&out, '\n');
    return clopts_help_out_status(&out);
}

const char *clopts_help_usage_text(clopts_usage_error_t kind)
{
    switch (kind)
    {
    case CLOPTS_USAGE_NONE:
        return NULL;
    case CLOPTS_USAGE_UNKNOWN_COMMAND:
        return "unknown command";
    case CLOPTS_USAGE_UNKNOWN_OPTION:
        return "unknown option";
    case CLOPTS_USAGE_MISSING_VALUE:
        return "requires a value";
    case CLOPTS_USAGE_UNEXPECTED_VALUE:
        return "does not take a value";
    case CLOPTS_USAGE_MISSING_OPTION:
        return "missing required option";
    case CLOPTS_USAGE_MISSING_ARGUMENT:
        return "missing argument";
    case CLOPTS_USAGE_UNEXPECTED_ARGUMENT:
        return "unexpected argument";
    }
    return NULL;
}

const char *clopts_help_decl_reason(clopts_decl_error_t kind)
{
    switch (kind)
    {
    case CLOPTS_DECL_NONE:
        return NULL;
    case CLOPTS_DECL_INVALID_NAME:
        return "invalid name";
    case CLOPTS_DECL_NO_NAME:
        return "option has neither a long nor a short name";
    case CLOPTS_DECL_DUPLICATE_COMMAND:
        return "duplicate command name";
    case CLOPTS_DECL_DUPLICATE_OPTION:
        return "duplicate option name";
    case CLOPTS_DECL_RESERVED_NAME:
        return "reserved name";
    case CLOPTS_DECL_NOT_NEGATABLE:
        return "only a flag can be negatable";
    case CLOPTS_DECL_NEGATION_CONFLICT:
        return "conflicts with the negated form of a flag";
    case CLOPTS_DECL_DEFAULT_NOT_ALLOWED:
        return "a flag or count cannot have a default value";
    case CLOPTS_DECL_ENV_NOT_ALLOWED:
        return "a flag or count cannot have an environment variable";
    case CLOPTS_DECL_REQUIRED_WITH_DEFAULT:
        return "a required option cannot have a default value";
    case CLOPTS_DECL_REQUIRED_NOT_ALLOWED:
        return "a flag or count cannot be required";
    case CLOPTS_DECL_POSITIONAL_ORDER:
        return "a required argument cannot follow an optional one";
    case CLOPTS_DECL_VARIADIC_POSITION:
        return "only the last argument can be variadic";
    case CLOPTS_DECL_ATTRIBUTE_KIND:
        return "attribute does not apply to this kind of option";
    case CLOPTS_DECL_MIXED_LAYOUT:
        return "a program with subcommands cannot have root arguments or a root handler";
    }
    return NULL;
}
