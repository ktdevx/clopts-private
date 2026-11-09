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

/*
 * The list of errors: X(name, message). The enumerators are
 * CLOPTS_ERROR_<name>, numbered from 0 in the order of the list, so new errors
 * go at the end (the values are stable). message is the static English
 * description returned by clopts_error_message() (NULL for CLOPTS_ERROR_NONE).
 */
#define CLOPTS_ERROR_LIST(X)                                                                                           \
    X(NONE, NULL)                                                                                                      \
    X(INVALID_NAME, "invalid name")                                                                                    \
    X(NO_NAME, "option has neither a long nor a short name")                                                           \
    X(DUPLICATE_COMMAND, "duplicate command name")                                                                     \
    X(DUPLICATE_OPTION, "duplicate option name")                                                                       \
    X(NOT_NEGATABLE, "only a flag can be negatable")                                                                   \
    X(NEGATION_CONFLICT, "conflicts with the negated form of a flag")                                                  \
    X(DEFAULT_NOT_ALLOWED, "a flag or count cannot have a default value")                                              \
    X(ENV_NOT_ALLOWED, "a flag or count cannot have an environment variable")                                          \
    X(REQUIRED_WITH_DEFAULT, "a required option cannot have a default value")                                          \
    X(REQUIRED_NOT_ALLOWED, "a flag or count cannot be required")                                                      \
    X(POSITIONAL_ORDER, "a required argument cannot follow an optional one")                                           \
    X(VARIADIC_POSITION, "only the last argument can be variadic")                                                     \
    X(ATTRIBUTE_KIND, "attribute does not apply to this kind of option")                                               \
    X(MIXED_LAYOUT, "a program with subcommands cannot have root arguments")                                           \
    X(INVALID_KIND, "invalid option kind")                                                                             \
    X(INVALID_ARGUMENT, "invalid argument")                                                                            \
    X(OUT_OF_MEMORY, "out of memory")

/* Outcome of a builder function. */
#define CLOPTS_ERROR_ENUMERATOR(name, message) CLOPTS_ERROR_##name,
typedef enum clopts_error
{
    CLOPTS_ERROR_LIST(CLOPTS_ERROR_ENUMERATOR)
} clopts_error_t;
#undef CLOPTS_ERROR_ENUMERATOR

/*
 * Returns a static English description of error (for example "duplicate
 * option name"), for the messages of the program that declares the options.
 * NULL for CLOPTS_ERROR_NONE and for values outside clopts_error_t.
 */
CLOPTS_API const char *clopts_error_message(clopts_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_ERROR_H */
