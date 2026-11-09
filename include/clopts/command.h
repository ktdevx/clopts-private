/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_COMMAND_H
#define CLOPTS_COMMAND_H

#include <stddef.h>

#include "clopts/decl_types.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Command (the root included): creation, release, texts, the attach of
 * options and positionals, and the read functions. A command is created on
 * its own, filled, and then handed to clopts_program_create() (clopts/program.h),
 * as the root or as a subcommand; it does not know its program. The attach
 * functions make the checks against the other options and positionals of the
 * same command. The rules shared by every builder function are in
 * clopts/decl_types.h.
 */

/*
 * Creates an unattached command named name (copied) into *out. The name must
 * not be NULL or empty, start with '-' or contain white space
 * (INVALID_NAME). For the root, the name is the program name.
 */
CLOPTS_API clopts_decl_error_t clopts_command_create(const char *name, clopts_command_t **out);

/*
 * Releases an unattached command with the options and positionals attached
 * to it. NULL does nothing. A command handed to a program is owned by it and
 * must not be passed (the behavior is undefined).
 */
CLOPTS_API void clopts_command_destroy(clopts_command_t *command);

/* One-line description in the program help. Not used for the root. */
CLOPTS_API clopts_decl_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary);

/* Description in the command help. For the root, the program description. */
CLOPTS_API clopts_decl_error_t clopts_command_set_description(clopts_command_t *command, const char *description);

/*
 * Attaches option to command. On success command owns option, and option
 * stays valid (read-only) as long as command does. On failure nothing
 * changes and the caller still owns option: DUPLICATE_OPTION (within
 * command) or NEGATION_CONFLICT (within command). Neither command nor option
 * may be attached already (the behavior is undefined, clopts/decl_types.h).
 */
CLOPTS_API clopts_decl_error_t clopts_command_add_option(clopts_command_t *command,
                                                         clopts_option_t *option);

/*
 * Attaches positional to command, after the positionals already attached.
 * Ownership and preconditions as with clopts_command_add_option().
 * VARIADIC_POSITION (the last attached one is variadic) or POSITIONAL_ORDER
 * (positional is required and the last attached one is optional).
 */
CLOPTS_API clopts_decl_error_t clopts_command_add_positional(clopts_command_t *command,
                                                             clopts_positional_t *positional);

/* ---- read functions ---- */

CLOPTS_API const char *clopts_command_name(const clopts_command_t *command);
/* NULL when not set. */
CLOPTS_API const char *clopts_command_summary(const clopts_command_t *command);
/* NULL when not set. */
CLOPTS_API const char *clopts_command_description(const clopts_command_t *command);
/* Number of attached options, in the order of attachment. */
CLOPTS_API size_t clopts_command_option_count(const clopts_command_t *command);
/* index must be less than clopts_command_option_count() (caught by assert). */
CLOPTS_API const clopts_option_t *clopts_command_option_at(const clopts_command_t *command, size_t index);
/* Number of attached positionals, in the order of attachment. */
CLOPTS_API size_t clopts_command_positional_count(const clopts_command_t *command);
/* index must be less than clopts_command_positional_count() (caught by assert). */
CLOPTS_API const clopts_positional_t *clopts_command_positional_at(const clopts_command_t *command, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_COMMAND_H */
