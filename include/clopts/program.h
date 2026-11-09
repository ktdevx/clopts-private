#ifndef CLOPTS_PROGRAM_H
#define CLOPTS_PROGRAM_H

#include "clopts/decl_types.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Program: creation and release, the version, the root and the subcommands.
 * The rules shared by every builder function are in clopts/decl_types.h.
 */

/*
 * Creates a program named name (copied) into *out. The name must not be
 * NULL or empty, start with '-' or contain white space (INVALID_NAME).
 */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_program_new(const char *name, clopts_program_t **out);

/* Releases program and everything attached to it. NULL does nothing. */
CLOPTS_API void clopts_program_free(clopts_program_t *program);

/*
 * Sets the version string (copied). NULL resets it: --version is not
 * reserved. RESERVED_NAME when an option named version is attached.
 */
CLOPTS_API clopts_decl_error_t clopts_program_set_version(clopts_program_t *program, const char *version);

/*
 * Returns the root scope: global options, and for a program without
 * subcommands its positionals and handler; its description is the program
 * description. NULL when program is NULL. Not a builder call: the same handle
 * is returned whether or not the program is frozen.
 */
CLOPTS_API clopts_command_t *clopts_program_root(clopts_program_t *program);

/*
 * Adds a subcommand named name (copied) into *out. The program owns it.
 * INVALID_NAME, RESERVED_NAME (help), DUPLICATE_COMMAND, or MIXED_LAYOUT when
 * the root has positionals or a handler.
 */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_program_add_command(clopts_program_t *program,
                                                                           const char *name,
                                                                           clopts_command_t **out);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PROGRAM_H */
