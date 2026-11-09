/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_TYPES_H
#define CLOPTS_DECL_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declaration API: the handle types, the result of the builder functions and
 * the rules they share. A declaration is built at run time with the builder
 * functions of clopts/option.h, clopts/positional.h, clopts/command.h and
 * clopts/program.h (all included by clopts/decl.h) and is referred to
 * through opaque handles only; their layout is private to the library.
 *
 * A declaration is built from the bottom up. Options and positionals are
 * created on their own, configured, and then attached to a command; commands
 * are created on their own, filled, and then handed to clopts_program_create()
 * together with the root:
 *
 *     clopts_option_t *verbose;
 *     if (clopts_option_create(CLOPTS_OPTION_COUNT, "verbose", 'v', &verbose) != CLOPTS_DECL_NONE) ...
 *     clopts_option_set_help(verbose, "Print more details");
 *     if (clopts_command_add_option(root, verbose) != CLOPTS_DECL_NONE) {
 *         clopts_option_destroy(verbose);  // a failed attach leaves it with the caller
 *         ...
 *     }
 *
 * An option or positional does not know the command it is attached to, and
 * a command does not know the program it belongs to.
 *
 * Ownership and lifetime:
 *   - Every string passed to a builder function is copied; the caller may
 *     change or release it right after the call.
 *   - A new option, positional or command is owned by the caller until it is
 *     attached successfully; the caller releases an unattached one with
 *     clopts_option_destroy() / clopts_positional_destroy() /
 *     clopts_command_destroy(). A successful attach (clopts_command_add_option(),
 *     clopts_command_add_positional(), clopts_program_create()) moves ownership
 *     to the command or the program; a failed one changes nothing.
 *   - The program owns its root, its commands and everything attached to
 *     them. These handles stay valid, at the same address, until
 *     clopts_program_destroy().
 *
 * Errors: every builder function returns CLOPTS_DECL_NONE on success and
 * another clopts_decl_error_t right away on failure, leaving every handle
 * unchanged. Nothing is recorded for later: a program built only from
 * successful calls is always valid. Checks that need only the option or
 * positional itself are made by its constructor and setters; checks against
 * the other options and positionals of the same command are made when it is
 * attached to the command; checks between commands (command names, the
 * layout of the root) are made by clopts_program_create(). Option names are not
 * checked between commands: a name of a command shadows the same name of the
 * root (clopts/parse.h).
 *
 * Other rules:
 *   - A NULL handle or out pointer is a contract violation, caught by assert:
 *     the builder functions return no error for it. On failure, the out
 *     pointer is set to NULL.
 *   - Attached handles are read-only, and a handle is attached at most once.
 *     The library does not check either: calling a setter or an attach on an
 *     attached option, positional or command, attaching the same option or
 *     positional twice, or listing the same command twice (or root among the
 *     commands) in clopts_program_create() is a contract violation (the
 *     behavior is undefined). A program never changes after
 *     clopts_program_create().
 *   - Setting a string attribute again replaces the previous copy (last call
 *     wins). NULL, false, '\0' or 0 resets an attribute.
 *   - The read functions (clopts_option_long_name() and the others) accept
 *     attached and unattached handles alike; a NULL option is a
 *     contract violation (caught by assert), and a NULL command, positional
 *     or program reads as empty (NULL, 0, false or '\0').
 */

typedef struct clopts_program clopts_program_t;
typedef struct clopts_command clopts_command_t;
typedef struct clopts_option clopts_option_t;
typedef struct clopts_positional clopts_positional_t;
typedef struct clopts_result clopts_result_t;

/* Outcome of a builder function. */
typedef enum clopts_decl_error
{
    CLOPTS_DECL_NONE = 0,
    /* Malformed command / option / positional name or empty env name. */
    CLOPTS_DECL_INVALID_NAME = 1,
    /* An option has neither a long nor a short name. */
    CLOPTS_DECL_NO_NAME = 2,
    /* Two commands of a program share a name. */
    CLOPTS_DECL_DUPLICATE_COMMAND = 3,
    /* Option names collide within a command. */
    CLOPTS_DECL_DUPLICATE_OPTION = 4,
    /* negatable set on an option that is not a FLAG. */
    CLOPTS_DECL_NOT_NEGATABLE = 5,
    /* A negatable FLAG x and an option named no-x in the same command. */
    CLOPTS_DECL_NEGATION_CONFLICT = 6,
    /* A default value set on a FLAG / COUNT. */
    CLOPTS_DECL_DEFAULT_NOT_ALLOWED = 7,
    /* An environment variable set on a FLAG / COUNT. */
    CLOPTS_DECL_ENV_NOT_ALLOWED = 8,
    /* required and a default value both set. */
    CLOPTS_DECL_REQUIRED_WITH_DEFAULT = 9,
    /* required set on a FLAG / COUNT. */
    CLOPTS_DECL_REQUIRED_NOT_ALLOWED = 10,
    /* A required positional attached after an optional one. */
    CLOPTS_DECL_POSITIONAL_ORDER = 11,
    /* A positional attached after a variadic one. */
    CLOPTS_DECL_VARIADIC_POSITION = 12,
    /* A delimiter on a non-LIST option, a count limit on a non-COUNT option or terminal on a non-FLAG option. */
    CLOPTS_DECL_ATTRIBUTE_KIND = 13,
    /* Positionals on the root of a program with subcommands. */
    CLOPTS_DECL_MIXED_LAYOUT = 14,
    /* An option kind outside clopts_option_kind_t. */
    CLOPTS_DECL_INVALID_KIND = 15,
    /* Not returned: a NULL handle or out pointer is a contract violation (assert). The value is kept stable. */
    CLOPTS_DECL_INVALID_ARGUMENT = 16,
    /* An allocation failed. */
    CLOPTS_DECL_OUT_OF_MEMORY = 17
} clopts_decl_error_t;

/*
 * Returns a static English description of error (for example "duplicate
 * option name"), for the messages of the program that declares the options.
 * NULL for CLOPTS_DECL_NONE and for values outside clopts_decl_error_t.
 */
CLOPTS_API const char *clopts_decl_error_message(clopts_decl_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_TYPES_H */
