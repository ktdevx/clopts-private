/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PROGRAM_H
#define CLOPTS_PROGRAM_H

#include <stddef.h>

#include "clopts/decl_types.h"
#include "clopts/export.h"

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
 * their own (clopts/command.h), filled, and then added. The attach functions
 * make the checks against the other options, positionals and subcommands of
 * the program. The rules shared by every builder function are in
 * clopts/decl_types.h.
 *
 * Once the program is first parsed (clopts/parse.h) or written
 * (clopts/text.h), it must not change any more: calling a builder function on
 * it or on anything attached to it is a contract violation (the behavior is
 * undefined; it is not detected). It can then be parsed and read from several
 * threads at the same time.
 */

/*
 * Creates an empty program named name (copied) into *out. The name must not
 * be NULL or empty, start with '-' or contain white space (INVALID_NAME).
 * out must not be NULL (contract violation, caught by assert).
 */
CLOPTS_API clopts_decl_error_t clopts_program_create(const char *name, clopts_program_t **out);

/* Releases program and everything attached to it. NULL does nothing. */
CLOPTS_API void clopts_program_destroy(clopts_program_t *program);

/* Description in the program help. */
CLOPTS_API clopts_decl_error_t clopts_program_set_description(clopts_program_t *program, const char *description);

/*
 * Attaches option to program as a global option. On success program owns
 * option, and option stays valid (read-only) as long as program does. On
 * failure nothing changes and the caller still owns option: DUPLICATE_OPTION
 * or NEGATION_CONFLICT (within the global options). option must not be
 * attached already (the behavior is undefined, clopts/decl_types.h).
 */
CLOPTS_API clopts_decl_error_t clopts_program_add_option(clopts_program_t *program, clopts_option_t *option);

/*
 * Attaches positional to program, after the positionals already attached.
 * Ownership and preconditions as with clopts_program_add_option().
 * MIXED_LAYOUT (program has subcommands), VARIADIC_POSITION (the last
 * attached one is variadic) or POSITIONAL_ORDER (positional is required and
 * the last attached one is optional), in that order.
 */
CLOPTS_API clopts_decl_error_t clopts_program_add_positional(clopts_program_t *program,
                                                        clopts_positional_t *positional);

/*
 * Adds command to program as a subcommand, after the subcommands already
 * added. On success program owns command, and command stays valid
 * (read-only) as long as program does. On failure nothing changes and the
 * caller still owns command: DUPLICATE_COMMAND (a subcommand of the same
 * name) or MIXED_LAYOUT (program has positionals), in that order. command
 * must not be added already (the behavior is undefined,
 * clopts/decl_types.h).
 */
CLOPTS_API clopts_decl_error_t clopts_program_add_command(clopts_program_t *program, clopts_command_t *command);

/* ---- read functions: a NULL program is a contract violation (assert) ---- */

CLOPTS_API const char *clopts_program_get_name(const clopts_program_t *program);
/* NULL when not set. */
CLOPTS_API const char *clopts_program_get_description(const clopts_program_t *program);
/* Number of global options, in the order of attachment. */
CLOPTS_API size_t clopts_program_get_option_count(const clopts_program_t *program);
/* index must be less than clopts_program_get_option_count() (caught by assert). */
CLOPTS_API const clopts_option_t *clopts_program_get_option_at(const clopts_program_t *program, size_t index);
/* Number of positionals of the program, in the order of attachment. */
CLOPTS_API size_t clopts_program_get_positional_count(const clopts_program_t *program);
/* index must be less than clopts_program_get_positional_count() (caught by assert). */
CLOPTS_API const clopts_positional_t *clopts_program_get_positional_at(const clopts_program_t *program, size_t index);
/* Number of subcommands, in the order they were added. */
CLOPTS_API size_t clopts_program_get_command_count(const clopts_program_t *program);
/* index must be less than clopts_program_get_command_count() (caught by assert). */
CLOPTS_API const clopts_command_t *clopts_program_get_command_at(const clopts_program_t *program, size_t index);
/* The subcommand named name (exact, case-sensitive), or NULL. name must not be NULL (caught by assert). */
CLOPTS_API const clopts_command_t *clopts_program_find_command(const clopts_program_t *program, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PROGRAM_H */
