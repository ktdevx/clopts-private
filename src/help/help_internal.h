#ifndef CLOPTS_HELP_INTERNAL_H
#define CLOPTS_HELP_INTERNAL_H

#include "clopts/decl.h"
#include "clopts/help.h"
#include "core/core_internal.h"

/*
 * Internal declarations of the help module, shared by its source files and
 * its tests (tests/help/). The help module reads the declaration through the
 * public read functions of clopts/decl.h only; it does not see the internal
 * model of the parser.
 */

/*
 * Output sink of the text writers (help_out.c). The same call sequence runs
 * in one of two modes:
 *   - write mode (fn != NULL): the bytes are passed to fn(ctx, ...) through
 *     clopts_core_write. The first failure sets failed; after that fn is not
 *     called again. A write of 0 bytes never calls fn.
 *   - measure mode (fn == NULL): nothing is written; the display width
 *     (clopts_help_width) of the bytes is added to width.
 *
 * State: failed changes from false to true at most once.
 */
typedef struct clopts_help_out
{
    clopts_write_fn fn;  /* NULL: measure mode */
    void *ctx;
    bool failed;         /* write mode: a callback failed; no further calls */
    size_t width;        /* measure mode: code points measured so far */
} clopts_help_out_t;

/* Starts an output in write mode (fn != NULL) or measure mode (fn == NULL). */
void clopts_help_out_init(clopts_help_out_t *out, clopts_write_fn fn, void *ctx);
/* Outputs the len bytes at data. */
void clopts_help_out_bytes(clopts_help_out_t *out, const char *data, size_t len);
/* Outputs the strlen(s) bytes of s. */
void clopts_help_out_str(clopts_help_out_t *out, const char *s);
/* Outputs the single byte c. */
void clopts_help_out_char(clopts_help_out_t *out, char c);
/* Outputs count spaces. */
void clopts_help_out_spaces(clopts_help_out_t *out, size_t count);
/* CLOPTS_WRITE_OK, or CLOPTS_WRITE_FAILED once a callback has failed. */
clopts_write_status_t clopts_help_out_status(const clopts_help_out_t *out);

/*
 * Display width of the len bytes at data: the number of code points of a
 * strict UTF-8 decoding. A valid sequence (no overlong form, no surrogate,
 * nothing above U+10FFFF) counts as 1; every byte that is not part of a
 * valid sequence counts as 1.
 */
size_t clopts_help_width(const char *data, size_t len);

/*
 * One row of a help list (help_layout.c): an option of the declaration, or a
 * fixed row whose left column and description are given as strings (the
 * reserved rows, for example).
 *   - option != NULL: the left column and the notes follow the rules of the
 *     option row; left and help are not read.
 *   - option == NULL: left is written as is; help is the description (NULL
 *     or "" for none).
 */
typedef struct clopts_help_row
{
    const clopts_option_t *option;
    const char *left;
    const char *help;
} clopts_help_row_t;

/*
 * Writes the count rows at rows to out as one aligned list: every row starts
 * with 2 spaces, and the description and notes start at column 2 + W + 2,
 * where W is the largest display width of the left columns of the list. A
 * row with neither description nor notes ends right after its left column.
 * Every row ends with '\n'. The left columns are measured with the same
 * procedure that writes them, on a measure-mode output.
 */
void clopts_help_write_list(clopts_help_out_t *out, const clopts_help_row_t *rows, size_t count);

/* True when command is the root or a subcommand of program. */
bool clopts_help_has_command(const clopts_program_t *program, const clopts_command_t *command);

/* Body template of a usage error kind; NULL for CLOPTS_USAGE_NONE and values outside the enum. */
const char *clopts_help_usage_text(clopts_usage_error_t kind);

#endif /* CLOPTS_HELP_INTERNAL_H */
