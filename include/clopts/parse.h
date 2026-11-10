/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSE_H
#define CLOPTS_PARSE_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/decl_types.h"
#include "clopts/env.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse API. clopts_parse*() return a result handle that is read with the
 * accessors below and released with clopts_result_destroy(). A program built
 * with the builder functions of clopts/decl.h is always valid, so a parse
 * never reports a declaration error.
 *
 * Lookup: an option token is looked up in the selected command first and
 * then in the program (the global options). In each of them the exact long
 * name comes first and --no-<name> of a negatable FLAG second, so a name of
 * a command shadows the same global option. The parser knows no built-in
 * option or command: --help, --version and a help command are ordinary
 * declarations (clopts/help.h).
 *
 * Terminal options (clopts_option_set_terminal()): when argv gives one, the
 * scan stops right after it, the parse ends with CLOPTS_PARSE_TERMINATED and
 * clopts_result_get_terminal_option() returns it. The tokens after it are not read and
 * the required options and positionals are not checked. A usage error found
 * before it is still replaced: after the first usage error, the following
 * tokens up to "--" are searched for an exact "--<long>" or "-<c>" of a
 * terminal option of the scope, and the first one found gives
 * CLOPTS_PARSE_TERMINATED instead.
 *
 * String lifetime: a string obtained from a result (values, list items,
 * positionals, error tokens) is valid until the result is released, and only
 * while the caller keeps both the program and argv alive. Accessors may only
 * be called before the program that produced the result is released;
 * clopts_result_destroy() may be called before or after clopts_program_destroy().
 *
 * Threads: parsing never writes to any stream, never calls exit() and never
 * writes to the program, so the same program may be parsed from several
 * threads at the same time.
 *
 * NULL handling: a NULL result is read like the out-of-memory sentinel
 * (status CLOPTS_PARSE_OUT_OF_MEMORY). A NULL option makes a value accessor return
 * the unset value. Neither is a contract violation.
 *
 * An option that is attached neither to the program nor to the selected command
 * (an option of another command or program, or an unattached one) reads as
 * unset.
 *
 * Contract violations (caught by assert): an index out of range.
 */

/* Outcome of a parse. */
typedef enum clopts_parse_status
{
    /* Parsed successfully. */
    CLOPTS_PARSE_OK = 0,
    /* A terminal option was given (clopts_result_get_terminal_option()). Required ones were not checked. */
    CLOPTS_PARSE_TERMINATED = 1,
    /* The program has subcommands but none appeared. Only global option values are available. */
    CLOPTS_PARSE_NO_COMMAND = 2,
    /* A usage error in argv. */
    CLOPTS_PARSE_USAGE_ERROR = 3,
    /* An allocation failed, or the environment lookup reported CLOPTS_ENV_FAILED. */
    CLOPTS_PARSE_OUT_OF_MEMORY = 4
} clopts_parse_status_t;

/* Kind of a usage error. The comment lists the error accessors it sets besides argv_index. */
typedef enum clopts_usage_error
{
    CLOPTS_USAGE_NONE = 0,
    /* token: the unknown command name. */
    CLOPTS_USAGE_UNKNOWN_COMMAND = 1,
    /* token, and name_len (long form) or short_name (short form). */
    CLOPTS_USAGE_UNKNOWN_OPTION = 2,
    /* option, token: an option that needs a value got none. */
    CLOPTS_USAGE_MISSING_VALUE = 3,
    /* option, token: a value given to an option that takes none. */
    CLOPTS_USAGE_UNEXPECTED_VALUE = 4,
    /* option: a required option got no value. */
    CLOPTS_USAGE_MISSING_OPTION = 5,
    /* positional: a required positional got no token. */
    CLOPTS_USAGE_MISSING_ARGUMENT = 6,
    /* token: the first token beyond the declared positionals. */
    CLOPTS_USAGE_UNEXPECTED_ARGUMENT = 7,
    /*
     * option: its environment variable is set but its value cannot be read
     * in UTF-8 (Windows: not valid UTF-16; a lookup callback returned
     * CLOPTS_ENV_INVALID). argv_index is -1.
     */
    CLOPTS_USAGE_INVALID_ENV_VALUE = 8
} clopts_usage_error_t;

/* Where an option's value came from. */
typedef enum clopts_source
{
    CLOPTS_SOURCE_NONE = 0,
    CLOPTS_SOURCE_DEFAULT = 1,
    CLOPTS_SOURCE_ENV = 2,
    CLOPTS_SOURCE_EXPLICIT = 3
} clopts_source_t;

/*
 * Parses argv[1..argc-1] (argv[0] is skipped) against program. argc must be
 * >= 0 and argv[0..argc-1] NUL-terminated (argv is not read when argc == 0).
 * Neither program's declaration nor argv is modified. Environment variables
 * are read from the process environment in UTF-8: on Windows with
 * GetEnvironmentVariableW() and converted, elsewhere with getenv() and
 * passed through as is. Never returns NULL: when an allocation fails (or
 * program is NULL) the shared out-of-memory sentinel is returned.
 */
CLOPTS_API clopts_result_t *clopts_parse(const clopts_program_t *program, int argc, char *const *argv);

/*
 * Same as clopts_parse(), with getenv_fn(env_ctx, ...) as the source of the
 * environment values instead of the process environment. NULL selects the
 * process environment, as clopts_parse() does.
 */
CLOPTS_API clopts_result_t *clopts_parse_with_env(const clopts_program_t *program, int argc, char *const *argv,
                                                  clopts_getenv_fn getenv_fn, void *env_ctx);

CLOPTS_API clopts_parse_status_t clopts_result_get_status(const clopts_result_t *result);

/* OK / TERMINATED / NO_COMMAND / USAGE_ERROR: the selected command. NULL without one. */
CLOPTS_API const clopts_command_t *clopts_result_get_command(const clopts_result_t *result);

/* TERMINATED: the terminal option that ended the parse. NULL for any other status. */
CLOPTS_API const clopts_option_t *clopts_result_get_terminal_option(const clopts_result_t *result);

/*
 * Error accessors. Meaningful when the status is USAGE_ERROR;
 * otherwise (and for fields an error does not set) they return NONE, NULL,
 * -1, 0 or '\0'. error_command is NULL when no command was selected.
 */
CLOPTS_API clopts_usage_error_t clopts_result_get_usage_error(const clopts_result_t *result);
CLOPTS_API const clopts_command_t *clopts_result_get_error_command(const clopts_result_t *result);
CLOPTS_API const clopts_option_t *clopts_result_get_error_option(const clopts_result_t *result);
CLOPTS_API const clopts_positional_t *clopts_result_get_error_positional(const clopts_result_t *result);
/* The offending argv element. */
CLOPTS_API const char *clopts_result_get_error_token(const clopts_result_t *result);
/* The argv index of the token, or -1. */
CLOPTS_API int clopts_result_get_error_argv_index(const clopts_result_t *result);
/* UNKNOWN_OPTION in long form: length of "--name" in the token, up to any "=". */
CLOPTS_API size_t clopts_result_get_error_name_len(const clopts_result_t *result);
/* The offending character inside a short option group. */
CLOPTS_API char clopts_result_get_error_short_name(const clopts_result_t *result);

/*
 * Value accessors. Meaningful when the status is OK, TERMINATED (the values
 * recorded up to the terminal option, then the environment and defaults) or
 * NO_COMMAND (global options only); otherwise, and for an option of a
 * command that was not selected, they return the unset value (false, 0,
 * NULL, 0 items, CLOPTS_SOURCE_NONE).
 */
CLOPTS_API bool clopts_result_get_flag(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API unsigned clopts_result_get_count(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API const char *clopts_result_get_value(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API size_t clopts_result_get_list_count(const clopts_result_t *result, const clopts_option_t *option);
/* index must be less than clopts_result_get_list_count(). */
CLOPTS_API const char *clopts_result_get_list_at(const clopts_result_t *result, const clopts_option_t *option,
                                                  size_t index);
CLOPTS_API clopts_source_t clopts_result_get_source(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API size_t clopts_result_get_positional_count(const clopts_result_t *result);
/* index must be less than clopts_result_get_positional_count(). Returns an argv element. */
CLOPTS_API const char *clopts_result_get_positional_at(const clopts_result_t *result, size_t index);

/* Releases result. NULL and the sentinel do nothing. May be called before or after clopts_program_destroy(). */
CLOPTS_API void clopts_result_destroy(clopts_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PARSE_H */
