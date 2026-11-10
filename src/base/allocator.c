/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * The allocator in use (clopts_set_allocator, include/clopts/base/allocator.h).
 * The allocation gateway (mem.c) is the only caller of its functions.
 */

#include "base/allocator.h"

#include <assert.h>
#include <stdlib.h>

/*
 * The standard functions, wrapped: the address of a CRT function imported
 * from a DLL is not an address constant (MSVC C4232), so the static
 * initializer below uses these instead.
 */
static void *std_malloc(size_t size)
{
    return malloc(size);
}

static void *std_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *std_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void std_free(void *ptr)
{
    free(ptr);
}

static const clopts_allocator_t std_allocator = {
    std_malloc, std_calloc, std_realloc, std_free
};

/* The allocator in use. Starts as the standard functions. */
static clopts_allocator_t current = {
    std_malloc, std_calloc, std_realloc, std_free
};

void clopts_set_allocator(const clopts_allocator_t *allocator)
{
    if (allocator == NULL)
    {
        current = std_allocator;
        return;
    }

    assert(allocator->malloc_fn != NULL);
    assert(allocator->calloc_fn != NULL);
    assert(allocator->realloc_fn != NULL);
    assert(allocator->free_fn != NULL);
    current = *allocator;
}

const clopts_allocator_t *clopts_allocator_get(void)
{
    return &current;
}
