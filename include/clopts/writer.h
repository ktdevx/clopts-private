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
 */
typedef int (*clopts_write_fn)(void *ctx, const char *data, size_t len);

/* A write callback paired with the caller-owned context passed to it. */
typedef struct clopts_writer
{
    clopts_write_fn write;
    void *ctx;
} clopts_writer_t;

/*
 * Returns a writer that writes to stream. The stream is owned by the caller
 * and must outlive the writer. stream must not be NULL.
 *
 * The returned writer's write returns 0 when fwrite() writes all len bytes
 * and nonzero otherwise. With len == 0 it writes nothing and returns 0.
 */
clopts_writer_t clopts_writer_file(FILE *stream);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_WRITER_H */
