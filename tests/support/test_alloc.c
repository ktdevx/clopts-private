/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include "test_alloc.h"

#include <clopts/base/allocator.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

test_alloc_state_t test_alloc;

/* Marks a live block and a released one. */
#define TEST_ALLOC_LIVE ((uint32_t)0x4c495645u)
#define TEST_ALLOC_DEAD ((uint32_t)0x44454144u)

/* Header in front of every block; the union keeps the user part aligned. */
typedef union test_alloc_header
{
    struct
    {
        uint32_t magic;
        size_t size;
    } info;
    long double align_ld;
    void *align_ptr;
    long long align_ll;
} test_alloc_header_t;

/* Passes one fault point. Returns nonzero when the call may go ahead. */
static int test_alloc_pass(void)
{
    test_alloc.calls++;
    return (test_alloc.fail_at == 0) || (test_alloc.calls < test_alloc.fail_at);
}

/* Returns the header of ptr when it is a live block of this allocator, or NULL. */
static test_alloc_header_t *test_alloc_header(void *ptr)
{
    test_alloc_header_t *header = (test_alloc_header_t *)ptr - 1;

    if (header->info.magic != TEST_ALLOC_LIVE)
    {
        test_alloc.bad_frees++;
        return NULL;
    }
    return header;
}

/* Allocates a block of size bytes with its header (one fault point). */
static void *test_alloc_block(size_t size)
{
    test_alloc_header_t *header;

    if (!test_alloc_pass())
    {
        return NULL;
    }
    if (size > (SIZE_MAX - sizeof(*header)))
    {
        return NULL;
    }
    header = (test_alloc_header_t *)malloc(sizeof(*header) + size);
    if (header == NULL)
    {
        return NULL;
    }
    header->info.magic = TEST_ALLOC_LIVE;
    header->info.size = size;
    test_alloc.live++;
    return header + 1;
}

static void *test_alloc_malloc(size_t size)
{
    if (size == 0)
    {
        test_alloc.zero_sizes++;
    }
    return test_alloc_block(size);
}

static void *test_alloc_calloc(size_t count, size_t size)
{
    void *ptr;

    if ((count == 0) || (size == 0))
    {
        test_alloc.zero_sizes++;
    }
    if ((size != 0) && (count > (SIZE_MAX / size)))
    {
        /* The library checks the product first, so this is never reached by it. */
        test_alloc.calls++;
        return NULL;
    }
    ptr = test_alloc_block(count * size);
    if (ptr != NULL)
    {
        memset(ptr, 0, count * size);
    }
    return ptr;
}

static void *test_alloc_realloc(void *ptr, size_t size)
{
    test_alloc_header_t *header;
    test_alloc_header_t *grown;

    if (size == 0)
    {
        test_alloc.zero_sizes++;
    }
    if (ptr == NULL)
    {
        return test_alloc_block(size);
    }
    header = test_alloc_header(ptr);
    if (header == NULL)
    {
        return NULL;
    }
    if (!test_alloc_pass())
    {
        return NULL;
    }
    if (size > (SIZE_MAX - sizeof(*header)))
    {
        return NULL;
    }
    grown = (test_alloc_header_t *)realloc(header, sizeof(*header) + size);
    if (grown == NULL)
    {
        return NULL;
    }
    grown->info.size = size;
    return grown + 1;
}

static void test_alloc_free(void *ptr)
{
    test_alloc_header_t *header;

    if (ptr == NULL)
    {
        /* The library never passes NULL; count it as a bad release. */
        test_alloc.bad_frees++;
        return;
    }
    header = test_alloc_header(ptr);
    if (header == NULL)
    {
        return;
    }
    header->info.magic = TEST_ALLOC_DEAD;
    test_alloc.live--;
    free(header);
}

void test_alloc_begin(size_t fail_at)
{
    static const clopts_allocator_t allocator = {
        test_alloc_malloc, test_alloc_calloc, test_alloc_realloc, test_alloc_free
    };

    clopts_set_allocator(&allocator);
    test_alloc.fail_at = fail_at;
    test_alloc.calls = 0;
    test_alloc.live = 0;
    test_alloc.bad_frees = 0;
    test_alloc.zero_sizes = 0;
}

void test_alloc_set_fail_at(size_t fail_at)
{
    test_alloc.fail_at = fail_at;
}

int test_alloc_end(void)
{
    test_alloc.fail_at = 0;
    return (test_alloc.live == 0) && (test_alloc.bad_frees == 0) && (test_alloc.zero_sizes == 0);
}
