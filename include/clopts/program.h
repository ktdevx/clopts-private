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
 * Program: creation from a root and its subcommands, release, and the read
 * functions. A program is created in one call and never changes afterwards,
 * so it can be parsed and read from several threads at the same time. The
 * rules shared by every builder function are in clopts/decl_types.h.
 */

/*
 * Creates a program from root and the command_count subcommands at commands
 * (NULL is fine when command_count is 0) into *out. The name of root is the
 * name of the program, its description the description of the program, and
 * its options are the global options. On success the program owns root and
 * every command, and they become read-only; on failure nothing changes and
 * the caller still owns them. DUPLICATE_COMMAND, or MIXED_LAYOUT when there
 * are subcommands and root has positionals. root, every command and out must
 * not be NULL (contract violation, caught by assert). root and the commands
 * must not belong to a program already, and each must appear once (the
 * behavior is undefined otherwise).
 */
CLOPTS_API clopts_decl_error_t clopts_program_create(clopts_command_t *root,
                                                     clopts_command_t *const *commands,
                                                     size_t command_count, clopts_program_t **out);

/* Releases program and everything attached to it. NULL does nothing. */
CLOPTS_API void clopts_program_destroy(clopts_program_t *program);

/* The root of program (NULL for NULL). */
CLOPTS_API const clopts_command_t *clopts_program_root(const clopts_program_t *program);

/* Number of subcommands, in the order given to clopts_program_create(). */
CLOPTS_API size_t clopts_program_command_count(const clopts_program_t *program);

/* Subcommand index; index must be less than clopts_program_command_count() (caught by assert). */
CLOPTS_API const clopts_command_t *clopts_program_command_at(const clopts_program_t *program, size_t index);

/* The subcommand named name (exact, case-sensitive), or NULL. */
CLOPTS_API const clopts_command_t *clopts_program_find_command(const clopts_program_t *program, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PROGRAM_H */
