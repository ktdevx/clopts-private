#ifndef CLOPTS_HELP_H
#define CLOPTS_HELP_H

#include "clopts/decl.h"
#include "clopts/export.h"
#include "clopts/parse.h"
#include "clopts/writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Text API: help, Usage line, version line, usage hint and error line of a
 * program. Every function writes its text through write_fn(ctx, ...) only;
 * nothing is written to a stream directly. To write to a FILE*, pass the
 * ready-made callback of clopts/writer.h, e.g.
 * clopts_write_help(program, NULL, clopts_file_write, stdout).
 *
 * Preconditions (caught by assert): program and write_fn are not NULL; a
 * command belongs to program; a result was obtained from program and program
 * has not been released yet. A NULL result is read like the out-of-memory
 * sentinel, as in clopts/parse.h.
 *
 * Postconditions: with CLOPTS_WRITE_OK every byte of the text was passed to
 * the callback, in order. With CLOPTS_WRITE_FAILED the callback was not
 * called again after the first call that failed. With
 * CLOPTS_WRITE_UNAVAILABLE the callback was not called at all. How the text
 * is split into calls is not part of the contract; only the concatenated
 * bytes are.
 *
 * These functions never change the declaration or the result and never
 * allocate memory. The same program may be read from several threads at the
 * same time, as long as no builder function runs concurrently.
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

/* command: NULL or the root for the program help, a subcommand for its help. */
CLOPTS_API clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                                   clopts_write_fn write_fn, void *ctx);
/* The Usage line of clopts_write_help() alone. */
CLOPTS_API clopts_write_status_t clopts_write_usage(const clopts_program_t *program, const clopts_command_t *command,
                                                    clopts_write_fn write_fn, void *ctx);
/* "<program> <version>\n". UNAVAILABLE without a version. */
CLOPTS_API clopts_write_status_t clopts_write_version(const clopts_program_t *program, clopts_write_fn write_fn,
                                                      void *ctx);
/* "Run '<program> [<command>] --help' for usage.\n". */
CLOPTS_API clopts_write_status_t clopts_write_usage_hint(const clopts_program_t *program,
                                                         const clopts_command_t *command, clopts_write_fn write_fn,
                                                         void *ctx);
/* One line for a USAGE_ERROR / OUT_OF_MEMORY result of program. UNAVAILABLE otherwise. */
CLOPTS_API clopts_write_status_t clopts_write_error(const clopts_program_t *program, const clopts_result_t *result,
                                                    unsigned flags, clopts_write_fn write_fn, void *ctx);

/*
 * Returns a static English description of error (for example "duplicate
 * option name"), for the messages of the program that declares the options.
 * NULL for CLOPTS_DECL_NONE and for values outside clopts_decl_error_t.
 */
CLOPTS_API const char *clopts_decl_error_message(clopts_decl_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_HELP_H */
