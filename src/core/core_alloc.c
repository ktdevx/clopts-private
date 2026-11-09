/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Allocation gateway: the only place of the library that calls the
 * allocator functions (clopts_set_allocator, include/clopts/alloc.h).
 */

#include "core/core_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 * The standard functions, wrapped: the address of a CRT function imported
 * from a DLL is not an address constant (MSVC C4232), so the static
 * initializer below uses these instead.
 */
static void *core_std_malloc(size_t size)
{
    return malloc(size);
}

static void *core_std_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *core_std_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void core_std_free(void *ptr)
{
    free(ptr);
}

static const clopts_allocator_t core_std_allocator = {
    core_std_malloc, core_std_calloc, core_std_realloc, core_std_free
};

/* The allocator in use. Starts as the standard functions. */
static clopts_allocator_t core_allocator = {
    core_std_malloc, core_std_calloc, core_std_realloc, core_std_free
};

void clopts_set_allocator(const clopts_allocator_t *allocator)
{
    if (allocator == NULL)
    {
        core_allocator = core_std_allocator;
        return;
    }

    assert(allocator->malloc_fn != NULL);
    assert(allocator->calloc_fn != NULL);
    assert(allocator->realloc_fn != NULL);
    assert(allocator->free_fn != NULL);
    core_allocator = *allocator;
}

void *clopts_core_malloc(size_t size)
{
    return core_allocator.malloc_fn((size == 0) ? 1 : size);
}

void *clopts_core_malloc_array(size_t count, size_t size)
{
    size_t bytes;

    if (clopts_core_size_mul(count, size, &bytes) != 0)
    {
        return NULL;
    }
    return clopts_core_malloc(bytes);
}

void *clopts_core_calloc(size_t count, size_t size)
{
    size_t bytes;

    if (clopts_core_size_mul(count, size, &bytes) != 0)
    {
        return NULL;
    }
    if (bytes == 0)
    {
        return core_allocator.calloc_fn(1, 1);
    }
    return core_allocator.calloc_fn(count, size);
}

void *clopts_core_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return clopts_core_malloc(size);
    }
    return core_allocator.realloc_fn(ptr, (size == 0) ? 1 : size);
}

char *clopts_core_strdup(const char *s)
{
    if (s == NULL)
    {
        return NULL;
    }
    return clopts_core_strndup(s, strlen(s));
}

int clopts_core_replace_string(char **field, const char *value)
{
    char *copy;

    assert(field != NULL);

    copy = clopts_core_strdup(value);
    if ((value != NULL) && (copy == NULL))
    {
        return -1;
    }

    clopts_core_free(*field);
    *field = copy;

    return 0;
}

char *clopts_core_strndup(const char *s, size_t len)
{
    size_t bytes;
    char *copy;

    assert(s != NULL);

    if (clopts_core_size_add(len, 1, &bytes) != 0)
    {
        return NULL;
    }

    copy = (char *)clopts_core_malloc(bytes);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

void clopts_core_free(void *ptr)
{
    if (ptr != NULL)
    {
        core_allocator.free_fn(ptr);
    }
}
