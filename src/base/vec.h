/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_BASE_VEC_H
#define CLOPTS_SRC_BASE_VEC_H

#include <stddef.h>

/*
 * Growable array of fixed-size elements.
 *
 * A zero-initialized struct ({0}) and a struct after clopts_vec_init() are
 * both valid empty states. A zero-initialized vec may be freed but must not
 * be pushed to (elem_size is 0).
 *
 * Invariants: len <= cap, and (cap == 0) if and only if (data == NULL).
 */
typedef struct clopts_vec
{
    void *data;        /* cap * elem_size bytes; NULL when cap == 0 */
    size_t len;        /* number of elements in use */
    size_t cap;        /* number of elements allocated */
    size_t elem_size;  /* bytes per element; > 0 for push */
} clopts_vec_t;

/* Sets data = NULL, len = cap = 0, elem_size = elem_size (> 0). */
void clopts_vec_init(clopts_vec_t *vec, size_t elem_size);

/*
 * Appends a copy of the elem_size bytes at elem. Grows the capacity to 8
 * and then doubles it when full. Returns 0 on success and -1 when the new
 * size overflows or the allocation fails; on failure data, len and cap are
 * left unchanged.
 */
int clopts_vec_push(clopts_vec_t *vec, const void *elem);

/* Returns a pointer to element index; requires index < len. */
void *clopts_vec_at(const clopts_vec_t *vec, size_t index);

/*
 * Releases the storage and resets to data = NULL, len = cap = 0, keeping
 * elem_size so the vec can be reused. Safe on a zero-initialized or already
 * freed vec.
 */
void clopts_vec_free(clopts_vec_t *vec);

#endif /* CLOPTS_SRC_BASE_VEC_H */
