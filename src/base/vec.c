/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include "base/vec.h"

#include "base/mem.h"
#include "base/size.h"

#include <assert.h>
#include <string.h>

/* Capacity of the first allocation, in elements. */
#define VEC_INITIAL_CAP ((size_t)8)

void clopts_vec_init(clopts_vec_t *vec, size_t elem_size)
{
    assert(vec != NULL);
    assert(elem_size > 0);

    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
    vec->elem_size = elem_size;
}

/*
 * Grows the storage to the next capacity (8, then doubling). Returns 0 on
 * success and -1 on overflow or allocation failure, leaving vec unchanged.
 */
static int vec_grow(clopts_vec_t *vec)
{
    size_t new_cap;
    size_t new_bytes;
    void *new_data;

    if (vec->cap == 0)
    {
        new_cap = VEC_INITIAL_CAP;
    }
    else if (clopts_size_mul(vec->cap, 2, &new_cap) != 0)
    {
        return -1;
    }

    if (clopts_size_mul(new_cap, vec->elem_size, &new_bytes) != 0)
    {
        return -1;
    }

    new_data = clopts_mem_realloc(vec->data, new_bytes);
    if (new_data == NULL)
    {
        return -1;
    }

    vec->data = new_data;
    vec->cap = new_cap;
    return 0;
}

int clopts_vec_push(clopts_vec_t *vec, const void *elem)
{
    assert(vec != NULL);
    assert(elem != NULL);
    assert(vec->elem_size > 0);

    if (vec->len == vec->cap)
    {
        if (vec_grow(vec) != 0)
        {
            return -1;
        }
    }

    /* len < cap here, and cap * elem_size was checked when it was allocated. */
    memcpy((unsigned char *)vec->data + (vec->len * vec->elem_size), elem, vec->elem_size);
    vec->len++;
    return 0;
}

void *clopts_vec_at(const clopts_vec_t *vec, size_t index)
{
    assert(vec != NULL);
    assert(index < vec->len);

    return (unsigned char *)vec->data + (index * vec->elem_size);
}

void clopts_vec_free(clopts_vec_t *vec)
{
    assert(vec != NULL);

    clopts_mem_free(vec->data);
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}
