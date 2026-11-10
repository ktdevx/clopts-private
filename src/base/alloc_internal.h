/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_BASE_ALLOC_INTERNAL_H
#define CLOPTS_SRC_BASE_ALLOC_INTERNAL_H

#include "clopts/base/alloc.h"

#include <stddef.h>

/*
 * Allocation gateway.
 *
 * Every allocation and release of the library goes through these functions,
 * which call the allocator in use (clopts_set_allocator, alloc.c). A size of
 * 0 is allocated as 1 byte, so NULL always means failure. The product
 * count * size is checked with clopts_size_mul; on overflow nothing is
 * allocated and NULL is returned.
 */

/* Returns a block of size bytes, or NULL. */
void *clopts_malloc(size_t size);

/* Returns a zero-filled block of count * size bytes, or NULL. */
void *clopts_calloc(size_t count, size_t size);

/*
 * Resizes ptr (NULL: allocates) to size bytes. Returns the new block, or NULL
 * leaving ptr valid and unchanged.
 */
void *clopts_realloc(void *ptr, size_t size);

/* Releases ptr, allocated by the functions above. NULL is ignored. */
void clopts_free(void *ptr);

#endif /* CLOPTS_SRC_BASE_ALLOC_INTERNAL_H */
