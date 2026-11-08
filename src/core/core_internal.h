#ifndef CLOPTS_CORE_INTERNAL_H
#define CLOPTS_CORE_INTERNAL_H

#include "clopts/writer.h"

#include <stddef.h>

/*
 * Overflow-checked size arithmetic.
 *
 * Both functions return 0 and store the result in *out on success. On
 * overflow they return -1 and leave *out unchanged. out must not be NULL.
 */
int clopts_core_size_mul(size_t a, size_t b, size_t *out);
int clopts_core_size_add(size_t a, size_t b, size_t *out);

/*
 * Growable array of fixed-size elements.
 *
 * A zero-initialized struct ({0}) and a struct after clopts_core_vec_init()
 * are both valid empty states. A zero-initialized vec may be freed but must
 * not be pushed to (elem_size is 0).
 *
 * Invariants: len <= cap, and (cap == 0) if and only if (data == NULL).
 */
typedef struct clopts_core_vec
{
    void *data;        /* cap * elem_size bytes; NULL when cap == 0 */
    size_t len;        /* number of elements in use */
    size_t cap;        /* number of elements allocated */
    size_t elem_size;  /* bytes per element; > 0 for push */
} clopts_core_vec_t;

/* Sets data = NULL, len = cap = 0, elem_size = elem_size (> 0). */
void clopts_core_vec_init(clopts_core_vec_t *vec, size_t elem_size);

/*
 * Appends a copy of the elem_size bytes at elem. Grows the capacity to 8
 * and then doubles it when full. Returns 0 on success and -1 when the new
 * size overflows or the allocation fails; on failure data, len and cap are
 * left unchanged.
 */
int clopts_core_vec_push(clopts_core_vec_t *vec, const void *elem);

/* Returns a pointer to element index; requires index < len. */
void *clopts_core_vec_at(const clopts_core_vec_t *vec, size_t index);

/*
 * Releases the storage and resets to data = NULL, len = cap = 0, keeping
 * elem_size so the vec can be reused. Safe on a zero-initialized or already
 * freed vec.
 */
void clopts_core_vec_free(clopts_core_vec_t *vec);

/*
 * Writing through a clopts_write_fn.
 *
 * clopts_core_write calls fn(ctx, data, len) once and returns its result
 * unchanged (0 = success); a failure is not retried, so the caller decides
 * whether to continue. clopts_core_write_str writes the strlen(s) bytes of
 * the NUL-terminated string s the same way.
 *
 * fn must not be NULL, data must not be NULL when len > 0, and s must not be
 * NULL. ctx is passed through unchecked; it belongs to the callback.
 */
int clopts_core_write(clopts_write_fn fn, void *ctx, const char *data, size_t len);
int clopts_core_write_str(clopts_write_fn fn, void *ctx, const char *s);

#endif /* CLOPTS_CORE_INTERNAL_H */
