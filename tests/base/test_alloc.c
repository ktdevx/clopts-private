/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite alloc: the allocation gateway (clopts_malloc / calloc / realloc /
 * free) and clopts_set_allocator().
 *
 * Checked here: the gateway with the standard functions, that it calls the
 * functions set with clopts_set_allocator() and only them, that NULL
 * restores the standard functions, the rounding of a size of 0 to 1 byte,
 * the overflow check of count * size (nothing is allocated),
 * that a failed realloc leaves the block valid, and the fault points of a
 * vec (only when its buffer grows).
 */

#include "base/alloc.h"
#include "base/vec.h"
#include "test_alloc.h"

#include <naos/naos.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- a recording allocator --------------------------------------------- */

/* Counts the calls of each function and the last sizes, and forwards to the standard functions. */
static size_t rec_malloc_calls;
static size_t rec_calloc_calls;
static size_t rec_realloc_calls;
static size_t rec_free_calls;
static size_t rec_last_size;
static size_t rec_last_count;

static void *rec_malloc(size_t size)
{
    rec_malloc_calls++;
    rec_last_size = size;
    return malloc(size);
}

static void *rec_calloc(size_t count, size_t size)
{
    rec_calloc_calls++;
    rec_last_count = count;
    rec_last_size = size;
    return calloc(count, size);
}

static void *rec_realloc(void *ptr, size_t size)
{
    rec_realloc_calls++;
    rec_last_size = size;
    return realloc(ptr, size);
}

static void rec_free(void *ptr)
{
    rec_free_calls++;
    free(ptr);
}

/* Installs the recording allocator with zero counters. */
static void rec_install(void)
{
    static const clopts_allocator_t allocator = { rec_malloc, rec_calloc, rec_realloc, rec_free };

    rec_malloc_calls = 0;
    rec_calloc_calls = 0;
    rec_realloc_calls = 0;
    rec_free_calls = 0;
    rec_last_size = 0;
    rec_last_count = 0;
    clopts_set_allocator(&allocator);
}

/* ---- the standard functions ------------------------------------------- */

NAOS_TEST(standard_functions_allocate_and_free)
{
    unsigned char *block;
    unsigned char *zeroed;
    size_t i;

    clopts_set_allocator(NULL);

    block = (unsigned char *)clopts_malloc(16);
    NAOS_REQUIRE_NOT_NULL(block);
    memset(block, 0xab, 16);

    zeroed = (unsigned char *)clopts_calloc(4, 8);
    NAOS_REQUIRE_NOT_NULL(zeroed);
    for (i = 0; i < 32; i++)
    {
        NAOS_CHECK_UINT_EQ(zeroed[i], 0);
    }

    block = (unsigned char *)clopts_realloc(block, 64);
    NAOS_REQUIRE_NOT_NULL(block);
    NAOS_CHECK_UINT_EQ(block[15], 0xab);

    clopts_free(block);
    clopts_free(zeroed);
    clopts_free(NULL);
}

/* ---- clopts_set_allocator ---------------------------------------------- */

NAOS_TEST(set_allocator_routes_every_function)
{
    void *a;
    void *b;

    rec_install();

    a = clopts_malloc(10);
    NAOS_REQUIRE_NOT_NULL(a);
    NAOS_CHECK_UINT_EQ(rec_malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(rec_last_size, 10);

    b = clopts_calloc(3, 4);
    NAOS_REQUIRE_NOT_NULL(b);
    NAOS_CHECK_UINT_EQ(rec_calloc_calls, 1);
    NAOS_CHECK_UINT_EQ(rec_last_count, 3);
    NAOS_CHECK_UINT_EQ(rec_last_size, 4);

    a = clopts_realloc(a, 20);
    NAOS_REQUIRE_NOT_NULL(a);
    NAOS_CHECK_UINT_EQ(rec_realloc_calls, 1);
    NAOS_CHECK_UINT_EQ(rec_last_size, 20);

    clopts_free(a);
    clopts_free(b);
    NAOS_CHECK_UINT_EQ(rec_free_calls, 2);

    /* NULL is ignored without calling free_fn. */
    clopts_free(NULL);
    NAOS_CHECK_UINT_EQ(rec_free_calls, 2);

    clopts_set_allocator(NULL);
}

NAOS_TEST(set_allocator_copies_the_struct)
{
    clopts_allocator_t allocator = { rec_malloc, rec_calloc, rec_realloc, rec_free };
    void *a;

    rec_malloc_calls = 0;
    rec_free_calls = 0;
    clopts_set_allocator(&allocator);

    /* Changing the caller's struct afterwards has no effect. */
    allocator.malloc_fn = NULL;
    allocator.free_fn = NULL;

    a = clopts_malloc(8);
    NAOS_REQUIRE_NOT_NULL(a);
    NAOS_CHECK_UINT_EQ(rec_malloc_calls, 1);
    clopts_free(a);
    NAOS_CHECK_UINT_EQ(rec_free_calls, 1);

    clopts_set_allocator(NULL);
}

NAOS_TEST(null_restores_the_standard_functions)
{
    void *a;

    rec_install();
    clopts_set_allocator(NULL);

    a = clopts_malloc(8);
    NAOS_REQUIRE_NOT_NULL(a);
    a = clopts_realloc(a, 16);
    NAOS_REQUIRE_NOT_NULL(a);
    clopts_free(a);

    NAOS_CHECK_UINT_EQ(rec_malloc_calls, 0);
    NAOS_CHECK_UINT_EQ(rec_realloc_calls, 0);
    NAOS_CHECK_UINT_EQ(rec_free_calls, 0);
}

/* ---- sizes and overflow ------------------------------------------------ */

NAOS_TEST(size_zero_is_allocated_as_one_byte)
{
    void *a;
    void *b;
    void *c;

    rec_install();

    a = clopts_malloc(0);
    NAOS_REQUIRE_NOT_NULL(a);
    NAOS_CHECK_UINT_EQ(rec_last_size, 1);

    b = clopts_calloc(0, 4);
    NAOS_REQUIRE_NOT_NULL(b);
    NAOS_CHECK_UINT_EQ(rec_last_count, 1);
    NAOS_CHECK_UINT_EQ(rec_last_size, 1);

    c = clopts_malloc(4);
    NAOS_REQUIRE_NOT_NULL(c);
    c = clopts_realloc(c, 0);
    NAOS_REQUIRE_NOT_NULL(c);
    NAOS_CHECK_UINT_EQ(rec_last_size, 1);

    clopts_free(a);
    clopts_free(b);
    clopts_free(c);
    clopts_set_allocator(NULL);
}

NAOS_TEST(realloc_of_null_allocates_with_malloc_fn)
{
    void *a;

    rec_install();

    a = clopts_realloc(NULL, 24);
    NAOS_REQUIRE_NOT_NULL(a);
    NAOS_CHECK_UINT_EQ(rec_malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(rec_realloc_calls, 0);
    NAOS_CHECK_UINT_EQ(rec_last_size, 24);

    clopts_free(a);
    clopts_set_allocator(NULL);
}

NAOS_TEST(overflow_allocates_nothing)
{
    rec_install();

    NAOS_CHECK_NULL(clopts_calloc(SIZE_MAX / 2, 3));

    NAOS_CHECK_UINT_EQ(rec_malloc_calls, 0);
    NAOS_CHECK_UINT_EQ(rec_calloc_calls, 0);
    clopts_set_allocator(NULL);
}

/* ---- failures ---------------------------------------------------------- */

NAOS_TEST(failures_return_null)
{
    void *a;

    test_alloc_begin(1);

    NAOS_CHECK_NULL(clopts_malloc(8));
    NAOS_CHECK_NULL(clopts_calloc(2, 8));
    NAOS_CHECK_NULL(clopts_realloc(NULL, 8));
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 3);

    /* A failed realloc leaves the block valid and unchanged. */
    test_alloc_begin(2);
    a = clopts_malloc(4);
    NAOS_REQUIRE_NOT_NULL(a);
    memcpy(a, "xyz", 4);
    NAOS_CHECK_NULL(clopts_realloc(a, 64));
    NAOS_CHECK_STR_EQ((const char *)a, "xyz");
    clopts_free(a);

    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- vec --------------------------------------------------------------- */

NAOS_TEST(vec_allocates_only_when_it_grows)
{
    clopts_vec_t vec;
    int i;

    test_alloc_begin(0);
    clopts_vec_init(&vec, sizeof(int));

    /* Capacity 8, then 16, then 32: three fault points for 20 elements. */
    for (i = 0; i < 20; i++)
    {
        NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &i), 0);
    }
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 3);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    clopts_vec_release(&vec);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(vec_push_failure_keeps_the_elements)
{
    clopts_vec_t vec;
    int i;

    test_alloc_begin(2);
    clopts_vec_init(&vec, sizeof(int));

    for (i = 0; i < 8; i++)
    {
        NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &i), 0);
    }
    /* The 9th push needs the second allocation (realloc to 16), which fails. */
    NAOS_CHECK_INT_EQ(clopts_vec_push(&vec, &i), -1);
    NAOS_CHECK_UINT_EQ(vec.len, 8);
    NAOS_CHECK_UINT_EQ(vec.cap, 8);
    for (i = 0; i < 8; i++)
    {
        NAOS_CHECK_INT_EQ(*(int *)clopts_vec_at(&vec, (size_t)i), i);
    }

    clopts_vec_release(&vec);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_SUITE(alloc,
    NAOS_ADD_TEST(standard_functions_allocate_and_free),
    NAOS_ADD_TEST(set_allocator_routes_every_function),
    NAOS_ADD_TEST(set_allocator_copies_the_struct),
    NAOS_ADD_TEST(null_restores_the_standard_functions),
    NAOS_ADD_TEST(size_zero_is_allocated_as_one_byte),
    NAOS_ADD_TEST(realloc_of_null_allocates_with_malloc_fn),
    NAOS_ADD_TEST(overflow_allocates_nothing),
    NAOS_ADD_TEST(failures_return_null),
    NAOS_ADD_TEST(vec_allocates_only_when_it_grows),
    NAOS_ADD_TEST(vec_push_failure_keeps_the_elements)
)
