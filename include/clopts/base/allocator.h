/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_BASE_ALLOCATOR_H
#define CLOPTS_BASE_ALLOCATOR_H

#include <stddef.h>

#include "clopts/base/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocator API: replaces the functions the library allocates and releases
 * memory with. One allocator is used by the whole library.
 *
 * Every function has the arguments and the meaning of its standard
 * counterpart (malloc, calloc, realloc, free). The library never calls them
 * with a size of 0 (it asks for 1 byte instead), so NULL from malloc_fn,
 * calloc_fn and realloc_fn always means failure. On failure realloc_fn must
 * leave the old block valid, as realloc does. free_fn is never called with
 * NULL. count * size is checked for overflow before calloc_fn is called.
 */
typedef struct clopts_allocator
{
    void *(*malloc_fn)(size_t size);
    void *(*calloc_fn)(size_t count, size_t size);
    void *(*realloc_fn)(void *ptr, size_t size);
    void (*free_fn)(void *ptr);
} clopts_allocator_t;

/*
 * Copies the four functions of allocator and uses them for every later
 * allocation and release. NULL restores malloc / calloc / realloc / free.
 * All four functions must be set (not NULL).
 *
 * Call it only while nothing allocated by the library is alive (no program,
 * result, argv handle or environment context), usually once at the start of
 * main: a block is always released with the free_fn that is set at that
 * time. The call is not synchronized; no other thread may use the library
 * while it runs.
 */
CLOPTS_API void clopts_set_allocator(const clopts_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_BASE_ALLOCATOR_H */
