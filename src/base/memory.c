/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Memory of the library: the allocator in use (clopts_set_allocator,
 * include/clopts/base/memory.h) and the allocation gateway, the only place of
 * the library that calls its functions.
 */

#include "base/memory.h"

#include "base/size.h"

#include <assert.h>
#include <stdlib.h>

static void *malloc_default(size_t size);
static void *calloc_default(size_t count, size_t size);
static void *realloc_default(void *ptr, size_t size);
static void free_default(void *ptr);

/* The allocator in use. Starts as the standard functions. */
static clopts_allocator_t current =
{
    malloc_default,
    calloc_default,
    realloc_default,
    free_default
};

/*
 * The standard functions, wrapped: the address of a CRT function imported
 * from a DLL is not an address constant (MSVC C4232), so the static
 * initializer of current uses these instead.
 */
static void *malloc_default(size_t size)
{
    return malloc(size);
}

static void *calloc_default(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *realloc_default(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void free_default(void *ptr)
{
    free(ptr);
}

void clopts_set_allocator(const clopts_allocator_t *alloc)
{
    if (alloc == NULL)
    {
        current.malloc_fn = malloc_default;
        current.calloc_fn = calloc_default;
        current.realloc_fn = realloc_default;
        current.free_fn = free_default;

        return;
    }

    assert(alloc->malloc_fn != NULL);
    assert(alloc->calloc_fn != NULL);
    assert(alloc->realloc_fn != NULL);
    assert(alloc->free_fn != NULL);

    current = *alloc;
}

void *clopts_malloc(size_t size)
{
    return current.malloc_fn((size == 0) ? 1 : size);
}

void *clopts_calloc(size_t count, size_t size)
{
    size_t bytes;

    if (clopts_size_mul(count, size, &bytes) != 0)
    {
        return NULL;
    }
    if (bytes == 0)
    {
        return current.calloc_fn(1, 1);
    }
    return current.calloc_fn(count, size);
}

void *clopts_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return clopts_malloc(size);
    }
    return current.realloc_fn(ptr, (size == 0) ? 1 : size);
}

void clopts_free(void *ptr)
{
    if (ptr != NULL)
    {
        current.free_fn(ptr);
    }
}
