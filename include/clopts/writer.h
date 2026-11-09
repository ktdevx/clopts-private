/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_WRITER_H
#define CLOPTS_WRITER_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_WRITER_H */
