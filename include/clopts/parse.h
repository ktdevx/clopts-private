#ifndef CLOPTS_PARSE_H
#define CLOPTS_PARSE_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/decl.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Outcome of a parse. Equal to the return value of clopts_parse*(). */
typedef enum clopts_status
{
    /* Parsed successfully. Values, sources and positionals are available. */
    CLOPTS_OK,
    /*
     * Help was requested. The help target is the whole program when
     * result->command is NULL and that command otherwise.
     */
    CLOPTS_HELP,
    /* --version was requested (only when program->version is not NULL). */
    CLOPTS_VERSION,
    /*
     * The program has subcommands but none appeared. The values of global
     * options are available.
     */
    CLOPTS_NO_COMMAND,
    /* A usage error in argv. result->error.usage is valid. */
    CLOPTS_USAGE_ERROR,
    /* The declaration is invalid. result->error.decl is valid; argv was not read. */
    CLOPTS_DECL_ERROR,
    /* A memory allocation or the environment lookup failed. */
    CLOPTS_OUT_OF_MEMORY
} clopts_status_t;

/*
 * Kind of a usage error. The comment on each value lists the clopts_error_t
 * fields it sets besides command and argv_index.
 */
typedef enum clopts_usage_error
{
    /* token: the unknown command name. */
    CLOPTS_USAGE_UNKNOWN_COMMAND,
    /* token, and name_len (long form) or short_name (short form). */
    CLOPTS_USAGE_UNKNOWN_OPTION,
    /* option, token: an option that needs a value got none. */
    CLOPTS_USAGE_MISSING_VALUE,
    /* option (NULL for a reserved option), token: a value given to a FLAG / COUNT. */
    CLOPTS_USAGE_UNEXPECTED_VALUE,
    /* option: a required option got no value. */
    CLOPTS_USAGE_MISSING_OPTION,
    /* positional: a required positional got no token. */
    CLOPTS_USAGE_MISSING_ARGUMENT,
    /* token: the first token beyond the declared positionals. */
    CLOPTS_USAGE_UNEXPECTED_ARGUMENT
} clopts_usage_error_t;

/*
 * Kind of a declaration error. The offending command, option or positional is
 * recorded in clopts_error_t when there is one.
 */
typedef enum clopts_decl_error
{
    /* Empty program / command name, malformed option name or empty env name. */
    CLOPTS_DECL_INVALID_NAME,
    /* An option has neither a long nor a short name. */
    CLOPTS_DECL_NO_NAME,
    /* Two commands share a name. */
    CLOPTS_DECL_DUPLICATE_COMMAND,
    /* Option names collide within a scope or between global and command scope. */
    CLOPTS_DECL_DUPLICATE_OPTION,
    /* help / h / version (when program->version is set) or the help command. */
    CLOPTS_DECL_RESERVED_NAME,
    /* negatable set on an option that is not a FLAG. */
    CLOPTS_DECL_NOT_NEGATABLE,
    /* A negatable FLAG x and an option named no-x in the same scope. */
    CLOPTS_DECL_NEGATION_CONFLICT,
    /* default_value set on a FLAG / COUNT. */
    CLOPTS_DECL_DEFAULT_NOT_ALLOWED,
    /* env set on a FLAG / COUNT. */
    CLOPTS_DECL_ENV_NOT_ALLOWED,
    /* required and default_value both set. */
    CLOPTS_DECL_REQUIRED_WITH_DEFAULT,
    /* required set on a FLAG / COUNT. */
    CLOPTS_DECL_REQUIRED_NOT_ALLOWED,
    /* A required positional follows an optional one. */
    CLOPTS_DECL_POSITIONAL_ORDER,
    /* A variadic positional that is not last, or more than one variadic. */
    CLOPTS_DECL_VARIADIC_POSITION
} clopts_decl_error_t;

/*
 * Structured error. Contains no formatted text and owns no memory: pointer
 * fields point into the declaration table or argv and stay valid as long as
 * the caller keeps those alive.
 */
typedef struct clopts_error
{
    /* Valid when status == CLOPTS_USAGE_ERROR. */
    clopts_usage_error_t usage;
    /* Valid when status == CLOPTS_DECL_ERROR. */
    clopts_decl_error_t decl;
    /* Offending command. NULL at program level or before a command is selected. */
    const clopts_command_t *command;
    /* Offending option, or NULL. */
    const clopts_option_t *option;
    /* Offending positional, or NULL. */
    const clopts_positional_t *positional;
    /* Usage errors: the offending argv element, or NULL. */
    const char *token;
    /* Usage errors: the argv index of token, or -1 when there is none. */
    int argv_index;
    /*
     * CLOPTS_USAGE_UNKNOWN_OPTION in long form: length of "--name" in token
     * (up to, not including, any "="). 0 otherwise.
     */
    size_t name_len;
    /* The offending character inside a short option group, or '\0'. */
    char short_name;
} clopts_error_t;

/* Where an option's value came from. */
typedef enum clopts_source
{
    /* Not given anywhere. */
    CLOPTS_SOURCE_NONE,
    /* From default_value. */
    CLOPTS_SOURCE_DEFAULT,
    /* From the environment variable named by env. */
    CLOPTS_SOURCE_ENV,
    /* From argv. */
    CLOPTS_SOURCE_EXPLICIT
} clopts_source_t;

struct clopts_parser_store;

/*
 * Parse result. A zero-initialized result ({0}) is a valid empty result. Once
 * passed to clopts_parse*(), it must be released with clopts_result_free().
 */
typedef struct clopts_result
{
    /* Same as the return value of the last clopts_parse*() call. */
    clopts_status_t status;
    /*
     * The selected command, or NULL without subcommands or when none was
     * selected. With CLOPTS_HELP it is the help target.
     */
    const clopts_command_t *command;
    /* Valid when status is CLOPTS_USAGE_ERROR or CLOPTS_DECL_ERROR. */
    clopts_error_t error;
    /* Internal storage owned by the library. NULL is a valid empty state. */
    struct clopts_parser_store *impl;
} clopts_result_t;

/* Return value of a clopts_getenv_fn. */
typedef enum clopts_env_status
{
    /*
     * Found. *out holds a copy allocated with malloc() (or a compatible
     * allocator); the library releases it with free().
     */
    CLOPTS_ENV_FOUND,
    /* Not set. *out is left untouched. */
    CLOPTS_ENV_UNSET,
    /*
     * Lookup failed (out of memory, conversion failure). *out is left
     * untouched and the parse ends with CLOPTS_OUT_OF_MEMORY.
     */
    CLOPTS_ENV_FAILED
} clopts_env_status_t;

/*
 * Environment lookup callback. Looks up name in the environment identified by
 * ctx and reports the outcome as described for clopts_env_status_t.
 */
typedef clopts_env_status_t (*clopts_getenv_fn)(void *ctx, const char *name, char **out);

/*
 * Validates the declaration only, without reading any argv. Returns CLOPTS_OK
 * when it is valid. Otherwise returns CLOPTS_DECL_ERROR and, when error is not
 * NULL, stores the first violation in *error. program must not be NULL.
 */
CLOPTS_API clopts_status_t clopts_decl_validate(const clopts_program_t *program, clopts_error_t *error);

/*
 * Parses argv[1..argc-1] (argv[0] is skipped) against program and stores the
 * outcome in *result. Environment variables are looked up with getenv() and
 * copied. Returns result->status.
 *
 * program and result must not be NULL and argc must be >= 0; argv[0..argc-1]
 * must be NUL-terminated strings (argv is not read when argc == 0). result must
 * be zero-initialized (e.g. clopts_result_t r = {0};) or a previous result of
 * clopts_parse*(); a previous result's storage is released first. An
 * uninitialized result is not allowed. Neither program nor argv is modified,
 * and both must outlive result. Nothing is written to any stream and exit() is
 * never called.
 */
CLOPTS_API clopts_status_t clopts_parse(const clopts_program_t *program, int argc, char *const *argv,
                                        clopts_result_t *result);

/*
 * Same as clopts_parse() (including the preconditions on program, argc, argv
 * and result), but environment variables are looked up with
 * getenv_fn(env_ctx, ...). A NULL getenv_fn selects the default lookup used by
 * clopts_parse().
 */
CLOPTS_API clopts_status_t clopts_parse_with_env(const clopts_program_t *program, int argc, char *const *argv,
                                                 clopts_getenv_fn getenv_fn, void *env_ctx,
                                                 clopts_result_t *result);

/*
 * Value accessors. Valid only when result->status is CLOPTS_OK or
 * CLOPTS_NO_COMMAND. option must be an element of a declaration table used
 * for the parse (the selected command's or the global options); anything else
 * is a contract violation caught by assert.
 *
 * Returned strings point into argv or the declaration table, or into storage
 * owned by result (list items and values from the environment), and stay valid
 * until clopts_result_free().
 */

/* FLAG: true when set. false when not given or negated. */
CLOPTS_API bool clopts_result_flag(const clopts_result_t *result, const clopts_option_t *option);
/* COUNT: number of occurrences, clamped to the limit. 0 when not given. */
CLOPTS_API unsigned clopts_result_count(const clopts_result_t *result, const clopts_option_t *option);
/* VALUE: the value, or NULL when not given. */
CLOPTS_API const char *clopts_result_value(const clopts_result_t *result, const clopts_option_t *option);
/* LIST: number of items. 0 when not given. */
CLOPTS_API size_t clopts_result_list_count(const clopts_result_t *result, const clopts_option_t *option);
/* LIST: item at index, which must be less than clopts_result_list_count(). */
CLOPTS_API const char *clopts_result_list_at(const clopts_result_t *result, const clopts_option_t *option,
                                             size_t index);
/* Where the option's value came from. */
CLOPTS_API clopts_source_t clopts_result_source(const clopts_result_t *result, const clopts_option_t *option);
/* Number of positional tokens. */
CLOPTS_API size_t clopts_result_positional_count(const clopts_result_t *result);
/* Positional token at index (an argv element), which must be less than the count. */
CLOPTS_API const char *clopts_result_positional_at(const clopts_result_t *result, size_t index);

/*
 * Calls the handler of the selected command (of the program without
 * subcommands) with result and its userdata, and returns the handler's return
 * value. result->status must be CLOPTS_OK and the handler must not be NULL;
 * otherwise it is a contract violation caught by assert.
 */
CLOPTS_API int clopts_result_run(const clopts_result_t *result);

/*
 * Releases the storage owned by result and sets result->impl to NULL. Safe on
 * a zero-initialized or already released result and after any status,
 * including CLOPTS_OUT_OF_MEMORY. status, command and error are not changed.
 * result must not be NULL.
 */
CLOPTS_API void clopts_result_free(clopts_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PARSE_H */
