/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_ERROR_H
#define CLOPTS_ERROR_H

#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Outcome of a builder function. */
typedef enum clopts_error
{
    CLOPTS_ERROR_NONE = 0,
    /* Malformed command / option / positional name or empty env name. */
    CLOPTS_ERROR_INVALID_NAME = 1,
    /* An option has neither a long nor a short name. */
    CLOPTS_ERROR_NO_NAME = 2,
    /* Two commands of a program share a name. */
    CLOPTS_ERROR_DUPLICATE_COMMAND = 3,
    /* Option names collide within a command. */
    CLOPTS_ERROR_DUPLICATE_OPTION = 4,
    /* negatable set on an option that is not a FLAG. */
    CLOPTS_ERROR_NOT_NEGATABLE = 5,
    /* A negatable FLAG x and an option named no-x in the same command. */
    CLOPTS_ERROR_NEGATION_CONFLICT = 6,
    /* A default value set on a FLAG / COUNT. */
    CLOPTS_ERROR_DEFAULT_NOT_ALLOWED = 7,
    /* An environment variable set on a FLAG / COUNT. */
    CLOPTS_ERROR_ENV_NOT_ALLOWED = 8,
    /* required and a default value both set. */
    CLOPTS_ERROR_REQUIRED_WITH_DEFAULT = 9,
    /* required set on a FLAG / COUNT. */
    CLOPTS_ERROR_REQUIRED_NOT_ALLOWED = 10,
    /* A required positional attached after an optional one. */
    CLOPTS_ERROR_POSITIONAL_ORDER = 11,
    /* A positional attached after a variadic one. */
    CLOPTS_ERROR_VARIADIC_POSITION = 12,
    /* A delimiter on a non-LIST option, a count limit on a non-COUNT option or terminal on a non-FLAG option. */
    CLOPTS_ERROR_ATTRIBUTE_KIND = 13,
    /* Positionals on the root of a program with subcommands. */
    CLOPTS_ERROR_MIXED_LAYOUT = 14,
    /* An option kind outside clopts_option_kind_t. */
    CLOPTS_ERROR_INVALID_KIND = 15,
    /* Not returned: a NULL handle or out pointer is a contract violation (assert). The value is kept stable. */
    CLOPTS_ERROR_INVALID_ARGUMENT = 16,
    /* An allocation failed. */
    CLOPTS_ERROR_OUT_OF_MEMORY = 17
} clopts_error_t;

/*
 * Returns a static English description of error (for example "duplicate
 * option name"), for the messages of the program that declares the options.
 * NULL for CLOPTS_ERROR_NONE and for values outside clopts_error_t.
 */
CLOPTS_API const char *clopts_decl_error_message(clopts_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_ERROR_H */
