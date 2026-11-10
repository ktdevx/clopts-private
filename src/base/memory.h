/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_BASE_MEMORY_H
#define CLOPTS_SRC_BASE_MEMORY_H

#include "clopts/base/memory.h"

#include <stddef.h>

/*
 * Allocation gateway.
 *
 * Every allocation and release of the library goes through these functions,
 * which call the allocator in use (clopts_set_allocator, memory.c). A size of
 * 0 is allocated as 1 byte, so NULL always means failure. Products and sums are
 * checked with clopts_size_mul / clopts_size_add; on overflow nothing is
 * allocated and NULL is returned.
 */

/* Returns a block of size bytes, or NULL. */
void *clopts_mem_alloc(size_t size);

/* Returns a block of count * size bytes, or NULL. */
void *clopts_mem_alloc_array(size_t count, size_t size);

/* Returns a zero-filled block of count * size bytes, or NULL. */
void *clopts_mem_calloc(size_t count, size_t size);

/*
 * Resizes ptr (NULL: allocates) to size bytes. Returns the new block, or NULL
 * leaving ptr valid and unchanged.
 */
void *clopts_mem_realloc(void *ptr, size_t size);

/* Returns a copy of s, or NULL on failure. s == NULL gives NULL without allocating. */
char *clopts_mem_strdup(const char *s);

/*
 * Replaces the owned string *field with a copy of value (NULL resets it to
 * NULL), releasing the old one. Returns 0, or -1 when the copy cannot be
 * allocated, leaving *field unchanged.
 */
int clopts_mem_replace_string(char **field, const char *value);

/*
 * Returns a NUL-terminated copy of the len bytes at s, or NULL. s must not be
 * NULL and must point to at least len bytes.
 */
char *clopts_mem_strndup(const char *s, size_t len);

/* Releases ptr, allocated by the functions above. NULL is ignored. */
void clopts_mem_free(void *ptr);

#endif /* CLOPTS_SRC_BASE_MEMORY_H */
