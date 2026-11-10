/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSE_USAGE_ERROR_H
#define CLOPTS_PARSE_USAGE_ERROR_H

#include <stddef.h>

#include "clopts/base/export.h"
#include "clopts/decl/program.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Usage error: what was wrong in argv (or in an environment value) when a
 * parse ends with CLOPTS_PARSE_USAGE_ERROR. The command it belongs to is the
 * selected command of the result (clopts_result_get_command(); NULL when the
 * error came before a command was selected). It is obtained with
 * clopts_result_get_usage_error() (clopts/parse/result.h) and belongs to its
 * result: it is valid until the result is released, and its strings only
 * while the caller keeps argv alive.
 *
 * Every accessor reads a NULL error as "no error" and returns NONE, NULL,
 * -1, 0 or '\0'; a field the kind does not set reads the same way.
 */

typedef struct clopts_usage_error clopts_usage_error_t;

/* Kind of a usage error. The comment lists the accessors it sets besides argv_index. */
typedef enum clopts_usage_error_kind
{
    CLOPTS_USAGE_ERROR_NONE = 0,
    /* token: the unknown command name. */
    CLOPTS_USAGE_ERROR_UNKNOWN_COMMAND = 1,
    /* token, and name_len (long form) or short_name (short form). */
    CLOPTS_USAGE_ERROR_UNKNOWN_OPTION = 2,
    /* option, token: an option that needs a value got none. */
    CLOPTS_USAGE_ERROR_MISSING_VALUE = 3,
    /* option, token: a value given to an option that takes none. */
    CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE = 4,
    /* option: a required option got no value. */
    CLOPTS_USAGE_ERROR_MISSING_OPTION = 5,
    /* positional: a required positional got no token. */
    CLOPTS_USAGE_ERROR_MISSING_ARGUMENT = 6,
    /* token: the first token beyond the declared positionals. */
    CLOPTS_USAGE_ERROR_UNEXPECTED_ARGUMENT = 7,
    /*
     * option: its environment variable is set but its value cannot be read
     * in UTF-8 (Windows: not valid UTF-16; a lookup callback returned
     * CLOPTS_ENV_INVALID). argv_index is -1.
     */
    CLOPTS_USAGE_ERROR_INVALID_ENV_VALUE = 8
} clopts_usage_error_kind_t;

CLOPTS_API clopts_usage_error_kind_t clopts_usage_error_get_kind(const clopts_usage_error_t *error);
CLOPTS_API const clopts_option_t *clopts_usage_error_get_option(const clopts_usage_error_t *error);
CLOPTS_API const clopts_positional_t *clopts_usage_error_get_positional(const clopts_usage_error_t *error);
/* The offending argv element. */
CLOPTS_API const char *clopts_usage_error_get_token(const clopts_usage_error_t *error);
/* The argv index of the token, or -1. */
CLOPTS_API int clopts_usage_error_get_argv_index(const clopts_usage_error_t *error);
/* UNKNOWN_OPTION in long form: length of "--name" in the token, up to any "=". */
CLOPTS_API size_t clopts_usage_error_get_name_len(const clopts_usage_error_t *error);
/* The offending character inside a short option group. */
CLOPTS_API char clopts_usage_error_get_short_name(const clopts_usage_error_t *error);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PARSE_USAGE_ERROR_H */
