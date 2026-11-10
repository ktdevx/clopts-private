/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_PROGRAM_H
#define CLOPTS_DECL_PROGRAM_H

#include <stddef.h>

#include "clopts/base/error.h"
#include "clopts/base/export.h"
#include "clopts/decl/command.h"
#include "clopts/decl/option.h"
#include "clopts/decl/positional.h"
#include "clopts/decl/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Program: creation, release, its own texts, options and positionals, the
 * attach of subcommands, and the read functions. A program is the scope of
 * the program itself: its name is the program name, its options are the
 * global options (found in every subcommand too, unless the subcommand has
 * the same name), and it holds the positionals of a program without
 * subcommands. It is created by name and filled; subcommands are created on
 * their own (clopts/decl/command.h), filled, and then added. The attach functions
 * make the checks against the other options, positionals and subcommands of
 * the program. The rules shared by every builder function follow.
 *
 * Once the program is first parsed (clopts/parse/parse.h) or written
 * (clopts/text/write.h), it must not change any more: calling a builder function on
 * it or on anything attached to it is a contract violation (the behavior is
 * undefined; it is not detected). It can then be parsed and read from several
 * threads at the same time.
 */

/*
 * Declaration API: the rules every builder function shares. A declaration is
 * built at run time with the builder functions of clopts/decl/option.h,
 * clopts/decl/positional.h, clopts/decl/command.h and this header (this
 * header includes the others) and is referred to through opaque handles
 * only; their layout is private to the library.
 *
 * A program is created by name with clopts_program_create() and filled.
 * Options and positionals are created on their own, configured, and then
 * attached to the program (global options, positionals of a program without
 * subcommands) or to a command; commands are created on their own, filled,
 * and then added to the program:
 *
 *     clopts_option_t *verbose;
 *     if (clopts_option_create(CLOPTS_OPTION_COUNT, "verbose", 'v', &verbose) != CLOPTS_ERROR_NONE) ...
 *     clopts_option_set_help(verbose, "Print more details");
 *     if (clopts_program_add_option(program, verbose) != CLOPTS_ERROR_NONE) {
 *         clopts_option_destroy(verbose);  // a failed attach leaves it with the caller
 *         ...
 *     }
 *
 * An option or positional does not know the program or command it is
 * attached to, and a command does not know the program it belongs to.
 *
 * Ownership and lifetime:
 *   - Every string passed to a builder function is copied; the caller may
 *     change or release it right after the call.
 *   - A new option, positional or command is owned by the caller until it is
 *     attached successfully; the caller releases an unattached one with
 *     clopts_option_destroy() / clopts_positional_destroy() /
 *     clopts_command_destroy(). A successful attach (clopts_program_add_option(),
 *     clopts_program_add_positional(), clopts_program_add_command(),
 *     clopts_command_add_option(), clopts_command_add_positional()) moves
 *     ownership to the program or the command; a failed one changes nothing.
 *   - The program owns its options, positionals and commands and everything
 *     attached to them. These handles stay valid, at the same address, until
 *     clopts_program_destroy().
 *
 * Errors: every builder function that can fail returns CLOPTS_ERROR_NONE on
 * success and another clopts_error_t right away on failure, leaving every
 * handle unchanged. clopts_positional_set_required() and
 * clopts_positional_set_variadic() cannot fail and return nothing. Nothing
 * is recorded for later: a program built only from successful calls is
 * always valid. Checks that need only the option or positional itself are
 * made by its constructor and setters; checks against the other options and
 * positionals of the same program or command are made when it is attached;
 * checks between commands (command names, the layout of the program) are
 * made by clopts_program_add_command() and clopts_program_add_positional().
 * Option names are not checked between the program and its commands: a name
 * of a command shadows the same global option (clopts/parse/parse.h).
 *
 * Other rules:
 *   - A NULL handle or out pointer is a contract violation, caught by assert:
 *     the builder functions return no error for it. On failure, the out
 *     pointer is set to NULL.
 *   - Attached handles are read-only, and a handle is attached at most once.
 *     The library does not check either: calling a setter or an attach on an
 *     attached option, positional or command, or attaching the same option,
 *     positional or command twice, is a contract violation (the behavior is
 *     undefined).
 *   - A program must not change once it is first parsed or written
 *     (clopts/decl/program.h): calling a builder function on it or on anything
 *     attached to it from then on is a contract violation (the behavior is
 *     undefined; it is not detected). Keeping to this is the caller's
 *     responsibility.
 *   - Setting a string attribute again replaces the previous copy (last call
 *     wins). NULL, false, '\0' or 0 resets an attribute.
 *   - The read functions (clopts_option_get_long_name() and the others) accept
 *     attached and unattached handles alike; a NULL program, command,
 *     option or positional is a contract violation (caught by assert).
 */

typedef struct clopts_program clopts_program_t;

/*
 * Creates an empty program named name (copied) into *out. The name must not
 * be NULL or empty, start with '-' or contain white space (INVALID_NAME).
 * out must not be NULL (contract violation, caught by assert).
 */
CLOPTS_API clopts_error_t clopts_program_create(const char *name, clopts_program_t **out);

/* Releases program and everything attached to it. NULL does nothing. */
CLOPTS_API void clopts_program_destroy(clopts_program_t *program);

/* Description in the program help. */
CLOPTS_API clopts_error_t clopts_program_set_description(clopts_program_t *program, const char *description);

/*
 * Attaches option to program as a global option. On success program owns
 * option, and option stays valid (read-only) as long as program does. On
 * failure nothing changes and the caller still owns option: DUPLICATE or
 * CONFLICT (the negated form of a flag), within the global options. option
 * must not be attached already (the behavior is undefined, see above).
 */
CLOPTS_API clopts_error_t clopts_program_add_option(clopts_program_t *program, clopts_option_t *option);

/*
 * Attaches positional to program, after the positionals already attached.
 * Ownership and preconditions as with clopts_program_add_option().
 * CONFLICT (program has subcommands), then ORDER (the last attached one is
 * variadic, or positional is required and the last attached one is
 * optional).
 */
CLOPTS_API clopts_error_t clopts_program_add_positional(clopts_program_t *program, clopts_positional_t *positional);

/*
 * Adds command to program as a subcommand, after the subcommands already
 * added. On success program owns command, and command stays valid
 * (read-only) as long as program does. On failure nothing changes and the
 * caller still owns command: DUPLICATE (a subcommand of the same name) or
 * CONFLICT (program has positionals), in that order. command
 * must not be added already (the behavior is undefined, see above).
 */
CLOPTS_API clopts_error_t clopts_program_add_command(clopts_program_t *program, clopts_command_t *command);

/* ---- read functions: a NULL program is a contract violation (assert) ---- */

CLOPTS_API const char *clopts_program_get_name(const clopts_program_t *program);
/* NULL when not set. */
CLOPTS_API const char *clopts_program_get_description(const clopts_program_t *program);
/* The global options and the positionals of the program itself (clopts/decl/scope.h). */
CLOPTS_API const clopts_scope_t *clopts_program_get_scope(const clopts_program_t *program);
/* Number of subcommands, in the order they were added. */
CLOPTS_API size_t clopts_program_get_command_count(const clopts_program_t *program);
/* index must be less than clopts_program_get_command_count() (caught by assert). */
CLOPTS_API const clopts_command_t *clopts_program_get_command_at(const clopts_program_t *program, size_t index);
/* The subcommand named name (exact, case-sensitive), or NULL. name must not be NULL (caught by assert). */
CLOPTS_API const clopts_command_t *clopts_program_find_command(const clopts_program_t *program, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_PROGRAM_H */
