#ifndef CLOPTS_PLATFORM_H
#define CLOPTS_PLATFORM_H

#include <stddef.h>

#include "clopts/export.h"
#include "clopts/parse.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform API: the process arguments (argv) and environment variables in
 * UTF-8, ready to be passed to clopts_parse() / clopts_parse_with_env().
 *
 * On Windows the arguments are taken from the process command line and
 * converted to UTF-8; the strings belong to the handle. Elsewhere the
 * caller's argv is returned as is, without copying or converting: its
 * strings stay valid only as long as the caller's argv does.
 *
 * A result of clopts_parse() borrows the argv strings, so release the argv
 * handle with clopts_argv_free() only after the result is no longer used.
 * Environment values are copied by the parser, so clopts_env_free() may be
 * called at any time after the parse.
 *
 * Enumerations are only extended by appending values.
 */

/* ---- argv ---------------------------------------------------------------- */

typedef struct clopts_argv clopts_argv_t;

/* Outcome of clopts_argv_get() / clopts_argv_from_wide(). Values are only appended. */
typedef enum clopts_argv_status
{
    CLOPTS_ARGV_OK = 0,
    /* An element is not valid UTF-16 (unpaired surrogate). clopts_argv_error_index() tells which. */
    CLOPTS_ARGV_INVALID_UTF16 = 1,
    /* Windows: the process command line could not be obtained. */
    CLOPTS_ARGV_CMDLINE_UNAVAILABLE = 2,
    CLOPTS_ARGV_OUT_OF_MEMORY = 3
} clopts_argv_status_t;

/*
 * Windows: ignores argc / argv (they are not read; 0 / NULL is fine), splits
 * GetCommandLineW() with the rules of the current MSVC CRT and converts every
 * element to UTF-8. The strings belong to the handle.
 * Elsewhere: returns argc and the very same argv strings, without copying or
 * converting; they stay valid only as long as the caller's argv does
 * (requires argc >= 0, argv[argc] == NULL, and argv != NULL unless argc == 0).
 * Never returns NULL: on allocation failure the shared out-of-memory sentinel
 * is returned. Release the handle with clopts_argv_free().
 */
CLOPTS_API clopts_argv_t *clopts_argv_get(int argc, char *const *argv);

#ifdef _WIN32
/* Converts wmain's arguments to UTF-8 (argc >= 0, wargv[0..argc-1] not NULL). Same handle as clopts_argv_get(). */
CLOPTS_API clopts_argv_t *clopts_argv_from_wide(int argc, wchar_t *const *wargv);
#endif

/* A NULL handle reads like the sentinel: OUT_OF_MEMORY, count 0, empty values, index -1. */
CLOPTS_API clopts_argv_status_t clopts_argv_status(const clopts_argv_t *args);
/* Number of elements; 0 unless the status is CLOPTS_ARGV_OK. */
CLOPTS_API int clopts_argv_count(const clopts_argv_t *args);
/* values[count] == NULL; never NULL itself. Valid until clopts_argv_free(). */
CLOPTS_API char *const *clopts_argv_values(const clopts_argv_t *args);
/* Index (argv[0] is 0) of the element with CLOPTS_ARGV_INVALID_UTF16; -1 for any other status. */
CLOPTS_API int clopts_argv_error_index(const clopts_argv_t *args);
/* Releases args. NULL and the sentinel are accepted and ignored. */
CLOPTS_API void clopts_argv_free(clopts_argv_t *args);

/* ---- environment ----------------------------------------------------------- */

typedef struct clopts_env clopts_env_t;

/* Why the last clopts_env_lookup() call returned CLOPTS_ENV_FAILED. Values are only appended. */
typedef enum clopts_env_error
{
    CLOPTS_ENV_ERROR_NONE = 0,
    /* Windows: the value is not valid UTF-16. */
    CLOPTS_ENV_ERROR_INVALID_UTF16 = 1,
    /* Windows: the name is not valid UTF-8. */
    CLOPTS_ENV_ERROR_INVALID_NAME = 2,
    CLOPTS_ENV_ERROR_OUT_OF_MEMORY = 3
} clopts_env_error_t;

/* Returns a new lookup context, or NULL when the allocation fails. */
CLOPTS_API clopts_env_t *clopts_env_new(void);
/* Releases env and every value it returned. NULL is ignored. */
CLOPTS_API void clopts_env_free(clopts_env_t *env);

/*
 * A clopts_getenv_fn: pass it with a clopts_env_t * as ctx to
 * clopts_parse_with_env(). Stores the UTF-8 value of name in *value (borrowed
 * from ctx, valid until the next call with the same ctx or clopts_env_free();
 * elsewhere than Windows it points into the process environment).
 * A NULL ctx returns CLOPTS_ENV_FAILED without reading name.
 * One context must not be used by several threads at the same time.
 */
CLOPTS_API clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value);

/* Failure of the last lookup with env; NONE after FOUND / UNSET. NULL env reads as OUT_OF_MEMORY. */
CLOPTS_API clopts_env_error_t clopts_env_error(const clopts_env_t *env);
/* Name given to the failed lookup (copy owned by env), or NULL when there is no failure or it could not be copied. */
CLOPTS_API const char *clopts_env_error_name(const clopts_env_t *env);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PLATFORM_H */
