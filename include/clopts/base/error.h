/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_BASE_ERROR_H
#define CLOPTS_BASE_ERROR_H

#include "clopts/base/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The list of errors: X(name, message). The enumerators are
 * CLOPTS_ERROR_<name>, numbered from 0 in the order of the list.
 * message is the static English description returned by
 * clopts_error_get_message() (NULL for CLOPTS_ERROR_NONE).
 */
#define CLOPTS_ERROR_LIST(X)                                                                                           \
    X(NONE, NULL)                                                                                                      \
    X(INVALID_ARGUMENT, "invalid argument")                                                                            \
    X(INVALID_NAME, "invalid name")                                                                                    \
    X(DUPLICATE, "duplicate name")                                                                                     \
    X(NOT_APPLICABLE, "does not apply to this kind")                                                                   \
    X(CONFLICT, "conflicts with another setting")                                                                      \
    X(ORDER, "out of order")                                                                                           \
    X(OUT_OF_MEMORY, "out of memory")

/* Outcome of a builder function. */
typedef enum clopts_error
{
#define CLOPTS_ERROR_ENUM(name, message) CLOPTS_ERROR_##name,
    CLOPTS_ERROR_LIST(CLOPTS_ERROR_ENUM)
#undef CLOPTS_ERROR_ENUM
} clopts_error_t;

/*
 * Returns a static English description of error (for example "duplicate
 * option name"), for the messages of the program that declares the options.
 * NULL for CLOPTS_ERROR_NONE and for values outside clopts_error_t.
 */
CLOPTS_API const char *clopts_error_get_message(clopts_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_BASE_ERROR_H */
