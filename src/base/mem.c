/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Allocation gateway: the only place of the library that calls the
 * functions of the allocator in use (allocator.c).
 */

#include "base/mem.h"

#include "base/allocator.h"
#include "base/size.h"

#include <assert.h>
#include <string.h>

void *clopts_mem_alloc(size_t size)
{
    return clopts_allocator_get()->malloc_fn((size == 0) ? 1 : size);
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
        return clopts_allocator_get()->calloc_fn(1, 1);
    }
    return clopts_allocator_get()->calloc_fn(count, size);
}

void *clopts_mem_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return clopts_mem_alloc(size);
    }
    return clopts_allocator_get()->realloc_fn(ptr, (size == 0) ? 1 : size);
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
        clopts_allocator_get()->free_fn(ptr);
    }
}
