/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_TEXT_OUT_H
#define CLOPTS_SRC_TEXT_OUT_H

#include "clopts/text/write.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Output (out.c): where the help and the messages write. The same call
 * sequence runs in one of two modes:
 *   - write mode (fn != NULL): the bytes are passed to fn(ctx, ...) in one
 *     call, without retry. The first failure sets failed; after that fn is not
 *     called again. A write of 0 bytes never calls fn.
 *   - measure mode (fn == NULL): nothing is written; the code points of the
 *     bytes (clopts_utf8_count, base/utf8.h) are added to width, so the same
 *     procedure first measures a column and then writes it.
 *
 * State: failed changes from false to true at most once. The fields may be
 * read freely; they are written only through the clopts_out_* functions.
 */
typedef struct clopts_out
{
    clopts_write_fn fn;  /* NULL: measure mode */
    void *ctx;
    bool failed;         /* write mode: a callback failed; no further calls */
    size_t width;        /* measure mode: code points measured so far */
} clopts_out_t;

/* Starts an output in write mode (fn != NULL) or measure mode (fn == NULL). */
void clopts_out_init(clopts_out_t *out, clopts_write_fn fn, void *ctx);
/* Outputs the len bytes at data. */
void clopts_out_bytes(clopts_out_t *out, const char *data, size_t len);
/* Outputs the strlen(s) bytes of s. */
void clopts_out_str(clopts_out_t *out, const char *s);
/* Outputs the single byte c. */
void clopts_out_char(clopts_out_t *out, char c);
/* Outputs count spaces. */
void clopts_out_spaces(clopts_out_t *out, size_t count);
/* CLOPTS_WRITE_OK, or CLOPTS_WRITE_FAILED once a callback has failed. */
clopts_write_status_t clopts_out_status(const clopts_out_t *out);

#endif /* CLOPTS_SRC_TEXT_OUT_H */
