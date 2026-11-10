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
 * Subcommand: texts, the options and positionals added to it, and the read
 * functions. A command is created by clopts_program_add_command()
 * (clopts/decl/program.h) and belongs to that program. The add functions
 * check the new option or positional against the other options and
 * positionals of the same command. The rules shared by every builder
 * function (errors, NULL handles, ownership) are in clopts/decl/program.h.
 */

typedef struct clopts_command clopts_command_t;

/* One-line description in the program help. */
CLOPTS_API void clopts_command_set_summary(clopts_command_t *command, const char *summary);

/* Description in the command help. */
CLOPTS_API void clopts_command_set_description(clopts_command_t *command, const char *description);

/*
 * Adds an option of kind to command and returns it, or NULL on failure, as
 * clopts_program_add_option() does for a global option: INVALID_ARGUMENT,
 * INVALID_NAME, DUPLICATE (a name already used within command) or CONFLICT
 * (the negated form of a flag within command), in that order.
 */
CLOPTS_API clopts_option_t *clopts_command_add_option(clopts_command_t *command, clopts_option_kind_t kind,
                                                      const char *long_name, char short_name);

/*
 * Adds a positional named name (not empty) to command after the positionals
 * already added, and returns it, or NULL on failure. INVALID_NAME, then
 * ORDER (the last one is variadic).
 */
CLOPTS_API clopts_positional_t *clopts_command_add_positional(clopts_command_t *command, const char *name);

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
