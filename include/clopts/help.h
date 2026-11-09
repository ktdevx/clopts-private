/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_HELP_H
#define CLOPTS_HELP_H

#include "clopts/decl_types.h"
#include "clopts/export.h"
#include "clopts/parse.h"
#include "clopts/writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Text API: help, Usage line, version line, usage hint and error line of a
 * program, and the declarations the help texts go with (--help, --version
 * and a help command). Every function writes its text through
 * write_fn(ctx, ...) only; nothing is written to a stream directly. To write
 * to a FILE*, pass the ready-made callback of clopts/writer.h, e.g.
 * clopts_write_help(program, NULL, clopts_file_write, stdout).
 *
 * This module reads the declaration through the read functions of
 * clopts/decl.h only, like any other user of the library.
 *
 * Preconditions (caught by assert): program and write_fn are not NULL; a
 * command is the root or a subcommand of program; a result was obtained from
 * program and program has not been released yet. A NULL result is read like
 * the out-of-memory sentinel, as in clopts/parse.h.
 *
 * Postconditions: with CLOPTS_WRITE_OK every byte of the text was passed to
 * the callback, in order. With CLOPTS_WRITE_FAILED the callback was not
 * called again after the first call that failed. With
 * CLOPTS_WRITE_UNAVAILABLE the callback was not called at all. How the text
 * is split into calls is not part of the contract; only the concatenated
 * bytes are.
 *
 * The write functions never change the declaration or the result and never
 * allocate memory. The same program may be read from several threads at the
 * same time.
 */

/* Outcome of a write function. */
typedef enum clopts_write_status
{
    CLOPTS_WRITE_OK = 0,          /* every byte was written */
    CLOPTS_WRITE_FAILED = 1,      /* the callback failed; it was not called again */
    CLOPTS_WRITE_UNAVAILABLE = 2  /* nothing to write; the callback was not called */
} clopts_write_status_t;

/* Prefix flags of clopts_write_error(). Undefined bits are ignored. */
#define CLOPTS_PREFIX_PROGRAM 0x1u  /* "<program>: " or "<program> <command>: " */
#define CLOPTS_PREFIX_ERROR 0x2u    /* "error: " */

/*
 * command: NULL or the root for the program help, a subcommand for its help.
 * In the help of a subcommand, the global options it shadows (every form of
 * the root option, --no-<name> of a negatable one included, is found in the
 * subcommand first) are left out.
 */
CLOPTS_API clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                                   clopts_write_fn write_fn, void *ctx);
/* The Usage line of clopts_write_help() alone. */
CLOPTS_API clopts_write_status_t clopts_write_usage(const clopts_program_t *program, const clopts_command_t *command,
                                                    clopts_write_fn write_fn, void *ctx);
/* "<program> <version>\n". UNAVAILABLE when version is NULL. */
CLOPTS_API clopts_write_status_t clopts_write_version(const clopts_program_t *program, const char *version,
                                                      clopts_write_fn write_fn, void *ctx);
/* "Run '<program> [<command>] --help' for usage.\n". */
CLOPTS_API clopts_write_status_t clopts_write_usage_hint(const clopts_program_t *program,
                                                         const clopts_command_t *command, clopts_write_fn write_fn,
                                                         void *ctx);
/* One line for a USAGE_ERROR / OUT_OF_MEMORY result of program. UNAVAILABLE otherwise. */
CLOPTS_API clopts_write_status_t clopts_write_error(const clopts_program_t *program, const clopts_result_t *result,
                                                    unsigned flags, clopts_write_fn write_fn, void *ctx);

/* ---- declarations for the help texts ---- */

/*
 * Creates the terminal FLAG -h / --help ("Show this help") and attaches it to
 * command, usually the root: a global option is found in every subcommand
 * too. *out gets the option (NULL on failure); out may be NULL. Errors of
 * clopts_option_create() and clopts_command_add_option(); on failure nothing is
 * attached.
 */
CLOPTS_API clopts_decl_error_t clopts_help_add_help_option(clopts_command_t *command,
                                                           const clopts_option_t **out);

/* Same for the terminal FLAG --version ("Show version information"). */
CLOPTS_API clopts_decl_error_t clopts_help_add_version_option(clopts_command_t *command,
                                                              const clopts_option_t **out);

/*
 * Creates an unattached subcommand "help" ("Show help for a command") with
 * one optional positional COMMAND into *out, to be passed to
 * clopts_program_create() with the other subcommands. clopts_help_target()
 * reads a result that selected it.
 */
CLOPTS_API clopts_decl_error_t clopts_help_command_create(clopts_command_t **out);

/*
 * For a result of program whose status is OK: reads the first positional as
 * the name of the command to show the help of. Returns true and sets *target
 * to NULL (no positional: the program help) or to the subcommand of that
 * name; returns false and sets *target to NULL when no subcommand has that
 * name, or when the status is not OK.
 */
CLOPTS_API bool clopts_help_target(const clopts_program_t *program, const clopts_result_t *result,
                                   const clopts_command_t **target);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_HELP_H */
