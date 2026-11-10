/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_COMMAND_H
#define CLOPTS_DECL_COMMAND_H

#include <stddef.h>

#include "clopts/base/error.h"
#include "clopts/base/export.h"
#include "clopts/decl/option.h"
#include "clopts/decl/positional.h"
#include "clopts/decl/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Subcommand: creation, release, texts, the attach of options and
 * positionals, and the read functions. A command is created on its own,
 * filled, and then added to a program with clopts_program_add_command()
 * (clopts/decl/program.h); it does not know its program. The attach functions make
 * the checks against the other options and positionals of the same command.
 * The rules shared by every builder function are in clopts/decl/program.h.
 */

typedef struct clopts_command clopts_command_t;

/*
 * Creates an unattached command named name (copied) into *out. The name must
 * not be NULL or empty, start with '-' or contain white space
 * (INVALID_NAME).
 */
CLOPTS_API clopts_error_t clopts_command_create(const char *name, clopts_command_t **out);

/*
 * Releases an unattached command with the options and positionals attached
 * to it. NULL does nothing. A command handed to a program is owned by it and
 * must not be passed (the behavior is undefined).
 */
CLOPTS_API void clopts_command_destroy(clopts_command_t *command);

/* One-line description in the program help. */
CLOPTS_API clopts_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary);

/* Description in the command help. */
CLOPTS_API clopts_error_t clopts_command_set_description(clopts_command_t *command, const char *description);

/*
 * Attaches option to command. On success command owns option, and option
 * stays valid (read-only) as long as command does. On failure nothing
 * changes and the caller still owns option: DUPLICATE (a name already used
 * within command) or CONFLICT (the negated form of a flag within command).
 * Neither command nor option
 * may be attached already (the behavior is undefined, clopts/decl/program.h).
 */
CLOPTS_API clopts_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option);

/*
 * Attaches positional to command, after the positionals already attached.
 * Ownership and preconditions as with clopts_command_add_option().
 * ORDER (the last attached one is variadic, or positional is required and
 * the last attached one is optional).
 */
CLOPTS_API clopts_error_t clopts_command_add_positional(clopts_command_t *command, clopts_positional_t *positional);

/* ---- read functions: a NULL command is a contract violation (assert) ---- */

CLOPTS_API const char *clopts_command_get_name(const clopts_command_t *command);
/* NULL when not set. */
CLOPTS_API const char *clopts_command_get_summary(const clopts_command_t *command);
/* NULL when not set. */
CLOPTS_API const char *clopts_command_get_description(const clopts_command_t *command);
/* The options and positionals of command (clopts/decl/scope.h). */
CLOPTS_API const clopts_scope_t *clopts_command_get_scope(const clopts_command_t *command);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_COMMAND_H */
