/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_TEXT_WRITE_H
#define CLOPTS_TEXT_WRITE_H

#include <stddef.h>
#include <stdio.h>

#include "clopts/base/export.h"
#include "clopts/decl/program.h"
#include "clopts/parse/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Text API: help, Usage line, version line, usage hint and error line of a
 * program. --help and --version are ordinary terminal FLAG options
 * (clopts_program_add_help_option(), clopts_program_add_version_option(),
 * clopts/decl/program.h), and a help command is an ordinary subcommand that
 * the program declares itself. Every function writes its
 * text through write_fn(ctx, ...) only; nothing is written to a stream
 * directly. To write
 * to a FILE*, pass the ready-made callback clopts_file_write below, e.g.
 * clopts_write_help(program, NULL, clopts_file_write, stdout).
 *
 * This module reads the declaration through the read functions of
 * clopts/decl/program.h only, like any other user of the library.
 *
 * Preconditions (caught by assert): program and write_fn are not NULL;
 * program has no declaration error (clopts_program_get_error()); a
 * command is NULL (the program itself) or a subcommand of program; a result
 * was obtained from program and program has not been released yet. A NULL result is read like
 * the out-of-memory sentinel, as in clopts/parse/result.h.
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

/*
 * Write callback. Writes the len bytes at data to the destination identified
 * by ctx. Returns 0 when all len bytes were written and nonzero otherwise.
 *
 * Library functions take the callback and its caller-owned context as two
 * separate arguments.
 */
typedef int (*clopts_write_fn)(void *ctx, const char *data, size_t len);

/*
 * Ready-made write callback for a FILE*. Pass the FILE* as ctx.
 *
 * Defined here as static inline so that it is compiled in the caller's
 * translation unit and is not part of the library: a FILE* never crosses the
 * library boundary, even when the library uses a different C runtime.
 *
 * Writes the len bytes at data to the stream in order and returns 0 when
 * fwrite() writes all of them, 1 otherwise. With len == 0 it writes nothing
 * and returns 0. ctx must be a valid FILE*, and data must not be NULL when
 * len > 0; otherwise the behavior is undefined.
 */
static inline int clopts_file_write(void *ctx, const char *data, size_t len)
{
    if (len == 0)
    {
        return 0;
    }
    return (fwrite(data, 1, len, (FILE *)ctx) == len) ? 0 : 1;
}

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
 * command: NULL for the program help, a subcommand for its help. In the help
 * of a subcommand, the global options it shadows (every form of the global
 * option, --no-<name> of a negatable one included, is found in the
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

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_TEXT_WRITE_H */
