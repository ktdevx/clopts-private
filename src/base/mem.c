/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Memory of the library: the allocator in use (clopts_set_allocator,
 * include/clopts/base/mem.h) and the allocation gateway, the only place of
 * the library that calls its functions.
 */

#include "base/mem.h"

#include "base/size.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

void *clopts_mem_alloc(size_t size)
{
    return current.malloc_fn((size == 0) ? 1 : size);
}

void *clopts_mem_alloc_array(size_t count, size_t size)
{
    size_t bytes;

    if (clopts_size_mul(count, size, &bytes) != 0)
    {
        return NULL;
    }
    return clopts_mem_alloc(bytes);
}

void *clopts_mem_calloc(size_t count, size_t size)
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

void *clopts_mem_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return clopts_mem_alloc(size);
    }
    return current.realloc_fn(ptr, (size == 0) ? 1 : size);
}

char *clopts_mem_strdup(const char *s)
{
    if (s == NULL)
    {
        return NULL;
    }
    return clopts_mem_strndup(s, strlen(s));
}

int clopts_mem_replace_string(char **field, const char *value)
{
    char *copy;

    assert(field != NULL);

    copy = clopts_mem_strdup(value);
    if ((value != NULL) && (copy == NULL))
    {
        return -1;
    }

    clopts_mem_free(*field);
    *field = copy;

    return 0;
}

char *clopts_mem_strndup(const char *s, size_t len)
{
    size_t bytes;
    char *copy;

    assert(s != NULL);

    if (clopts_size_add(len, 1, &bytes) != 0)
    {
        return NULL;
    }

    copy = (char *)clopts_mem_alloc(bytes);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

void clopts_mem_free(void *ptr)
{
    if (ptr != NULL)
    {
        current.free_fn(ptr);
    }
}
