/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_TYPES_H
#define CLOPTS_DECL_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/error.h"
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
 *     if (clopts_option_create(CLOPTS_OPTION_COUNT, "verbose", 'v', &verbose) != CLOPTS_ERROR_NONE) ...
 *     clopts_option_set_help(verbose, "Print more details");
 *     if (clopts_command_add_option(root, verbose) != CLOPTS_ERROR_NONE) {
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
 * Errors: every builder function that can fail returns CLOPTS_ERROR_NONE on
 * success and another clopts_error_t right away on failure, leaving every
 * handle unchanged. clopts_positional_set_required() and
 * clopts_positional_set_variadic() cannot fail and return nothing. Nothing is recorded for later: a program built only from
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
 *     attached and unattached handles alike; a NULL program, command,
 *     option or positional is a contract violation (caught by assert).
 */

typedef struct clopts_program clopts_program_t;
typedef struct clopts_command clopts_command_t;
typedef struct clopts_option clopts_option_t;
typedef struct clopts_positional clopts_positional_t;
typedef struct clopts_result clopts_result_t;

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_TYPES_H */
