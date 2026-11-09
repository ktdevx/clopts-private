#ifndef CLOPTS_PARSE_H
#define CLOPTS_PARSE_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/decl.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse API. clopts_parse*() return a result handle that is read with the
 * accessors below and released with clopts_result_free(). A program built
 * with the builder functions of clopts/decl.h is always valid, so a parse
 * never reports a declaration error.
 *
 * String lifetime: a string obtained from a result (values, list items,
 * positionals, error tokens) is valid until the result is released, and only
 * while the caller keeps both the program and argv alive. Accessors and
 * clopts_result_run() may only be called before the program that produced
 * the result is released; clopts_result_free() may be called before or after
 * clopts_program_free().
 *
 * Threads: parsing never writes to any stream and never calls exit(). The
 * first parse freezes the program (it writes to the program), so the same
 * program must not be parsed from several threads at the same time. Distinct
 * programs may be used concurrently.
 *
 * NULL handling: a NULL result is read like the out-of-memory sentinel
 * (status CLOPTS_OUT_OF_MEMORY). A NULL option makes a value accessor return
 * the unset value. Neither is a contract violation.
 *
 * Options are looked up by their address among the options of the root and
 * of the selected command, so an option that is not attached there (an
 * option of another command or program, or an unattached one) reads as
 * unset.
 *
 * Contract violations (caught by assert): an index out of range, and
 * clopts_result_run() on a result whose status is not CLOPTS_OK or for a
 * scope without a handler.
 */

/* Outcome of a parse. */
typedef enum clopts_status
{
    /* Parsed successfully. */
    CLOPTS_OK = 0,
    /* Help was requested: the whole program when clopts_result_command() is NULL, that command otherwise. */
    CLOPTS_HELP = 1,
    /* --version was requested (only when a version is set). */
    CLOPTS_VERSION = 2,
    /* The program has subcommands but none appeared. Only global option values are available. */
    CLOPTS_NO_COMMAND = 3,
    /* A usage error in argv. */
    CLOPTS_USAGE_ERROR = 4,
    /* An allocation or the environment lookup failed. */
    CLOPTS_OUT_OF_MEMORY = 5
} clopts_status_t;

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
    /* option (NULL for a reserved option), token: a value given to an option that takes none. */
    CLOPTS_USAGE_UNEXPECTED_VALUE = 4,
    /* option: a required option got no value. */
    CLOPTS_USAGE_MISSING_OPTION = 5,
    /* positional: a required positional got no token. */
    CLOPTS_USAGE_MISSING_ARGUMENT = 6,
    /* token: the first token beyond the declared positionals. */
    CLOPTS_USAGE_UNEXPECTED_ARGUMENT = 7
} clopts_usage_error_t;

/* Where an option's value came from. */
typedef enum clopts_source
{
    CLOPTS_SOURCE_NONE = 0,
    CLOPTS_SOURCE_DEFAULT = 1,
    CLOPTS_SOURCE_ENV = 2,
    CLOPTS_SOURCE_EXPLICIT = 3
} clopts_source_t;

/* Return value of a clopts_getenv_fn. */
typedef enum clopts_env_status
{
    /* Found: *value holds a string borrowed from the callback. */
    CLOPTS_ENV_FOUND = 0,
    /* Not set. *value is not read. */
    CLOPTS_ENV_UNSET = 1,
    /* Lookup failed. The parse ends with CLOPTS_OUT_OF_MEMORY. */
    CLOPTS_ENV_FAILED = 2
} clopts_env_status_t;

/*
 * Environment lookup callback. The string stored in *value is owned by the
 * callback and needs to stay valid only until the next call of the callback
 * or until the parse function returns. The library copies it right after the
 * callback returns and never reads or releases it afterwards.
 */
typedef clopts_env_status_t (*clopts_getenv_fn)(void *ctx, const char *name, const char **value);

/*
 * Parses argv[1..argc-1] (argv[0] is skipped) against program. argc must be
 * >= 0 and argv[0..argc-1] NUL-terminated (argv is not read when argc == 0).
 * Neither program's declaration nor argv is modified. Environment variables
 * are looked up with getenv(). Never returns NULL: when an allocation fails
 * (or program is NULL) the shared out-of-memory sentinel is returned.
 */
CLOPTS_API clopts_result_t *clopts_parse(clopts_program_t *program, int argc, char *const *argv);

/* Same as clopts_parse(), with getenv_fn(env_ctx, ...) as lookup. NULL selects the default lookup. */
CLOPTS_API clopts_result_t *clopts_parse_with_env(clopts_program_t *program, int argc, char *const *argv,
                                                  clopts_getenv_fn getenv_fn, void *env_ctx);

CLOPTS_API clopts_status_t clopts_result_status(const clopts_result_t *result);

/* OK / NO_COMMAND / USAGE_ERROR: the selected command. HELP: the help target. NULL without one. */
CLOPTS_API const clopts_command_t *clopts_result_command(const clopts_result_t *result);

/*
 * Error accessors. Meaningful when the status is USAGE_ERROR;
 * otherwise (and for fields an error does not set) they return NONE, NULL,
 * -1, 0 or '\0'. error_command is NULL for the root.
 */
CLOPTS_API clopts_usage_error_t clopts_result_usage_error(const clopts_result_t *result);
CLOPTS_API const clopts_command_t *clopts_result_error_command(const clopts_result_t *result);
CLOPTS_API const clopts_option_t *clopts_result_error_option(const clopts_result_t *result);
CLOPTS_API const clopts_positional_t *clopts_result_error_positional(const clopts_result_t *result);
/* The offending argv element. */
CLOPTS_API const char *clopts_result_error_token(const clopts_result_t *result);
/* The argv index of the token, or -1. */
CLOPTS_API int clopts_result_error_argv_index(const clopts_result_t *result);
/* UNKNOWN_OPTION in long form: length of "--name" in the token, up to any "=". */
CLOPTS_API size_t clopts_result_error_name_len(const clopts_result_t *result);
/* The offending character inside a short option group. */
CLOPTS_API char clopts_result_error_short_name(const clopts_result_t *result);

/*
 * Value accessors. Meaningful when the status is OK or NO_COMMAND (global
 * options only); otherwise, and for an option of a command that was not
 * selected, they return the unset value (false, 0, NULL, 0 items,
 * CLOPTS_SOURCE_NONE).
 */
CLOPTS_API bool clopts_result_flag(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API unsigned clopts_result_count(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API const char *clopts_result_value(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API size_t clopts_result_list_count(const clopts_result_t *result, const clopts_option_t *option);
/* index must be less than clopts_result_list_count(). */
CLOPTS_API const char *clopts_result_list_at(const clopts_result_t *result, const clopts_option_t *option, size_t index);
CLOPTS_API clopts_source_t clopts_result_source(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API size_t clopts_result_positional_count(const clopts_result_t *result);
/* index must be less than clopts_result_positional_count(). Returns an argv element. */
CLOPTS_API const char *clopts_result_positional_at(const clopts_result_t *result, size_t index);

/* Calls the handler of the selected scope with result and its userdata and returns its value. */
CLOPTS_API int clopts_result_run(const clopts_result_t *result);

/* Releases result. NULL and the sentinel do nothing. May be called before or after clopts_program_free(). */
CLOPTS_API void clopts_result_free(clopts_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PARSE_H */
