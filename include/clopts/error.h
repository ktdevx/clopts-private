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
    /* Malformed command / option / positional name or empty env name. */                                              \
    X(INVALID_NAME, "invalid name")                                                                                    \
    /* An option has neither a long nor a short name. */                                                               \
    X(NO_NAME, "option has neither a long nor a short name")                                                           \
    /* Two commands of a program share a name. */                                                                      \
    X(DUPLICATE_COMMAND, "duplicate command name")                                                                     \
    /* Option names collide within a command. */                                                                       \
    X(DUPLICATE_OPTION, "duplicate option name")                                                                       \
    /* negatable set on an option that is not a FLAG. */                                                               \
    X(NOT_NEGATABLE, "only a flag can be negatable")                                                                   \
    /* A negatable FLAG x and an option named no-x in the same command. */                                             \
    X(NEGATION_CONFLICT, "conflicts with the negated form of a flag")                                                  \
    /* A default value set on a FLAG / COUNT. */                                                                       \
    X(DEFAULT_NOT_ALLOWED, "a flag or count cannot have a default value")                                              \
    /* An environment variable set on a FLAG / COUNT. */                                                               \
    X(ENV_NOT_ALLOWED, "a flag or count cannot have an environment variable")                                          \
    /* required and a default value both set. */                                                                       \
    X(REQUIRED_WITH_DEFAULT, "a required option cannot have a default value")                                          \
    /* required set on a FLAG / COUNT. */                                                                              \
    X(REQUIRED_NOT_ALLOWED, "a flag or count cannot be required")                                                      \
    /* A required positional attached after an optional one. */                                                        \
    X(POSITIONAL_ORDER, "a required argument cannot follow an optional one")                                           \
    /* A positional attached after a variadic one. */                                                                  \
    X(VARIADIC_POSITION, "only the last argument can be variadic")                                                     \
    /* A delimiter on a non-LIST option, a count limit on a non-COUNT option or terminal on a non-FLAG option. */      \
    X(ATTRIBUTE_KIND, "attribute does not apply to this kind of option")                                               \
    /* Positionals on the root of a program with subcommands. */                                                       \
    X(MIXED_LAYOUT, "a program with subcommands cannot have root arguments")                                           \
    /* An option kind outside clopts_option_kind_t. */                                                                 \
    X(INVALID_KIND, "invalid option kind")                                                                             \
    /* Not returned: a NULL handle or out pointer is a contract violation (assert). The value is kept stable. */       \
    X(INVALID_ARGUMENT, "invalid argument")                                                                            \
    /* An allocation failed. */                                                                                        \
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
