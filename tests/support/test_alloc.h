/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_TEST_ALLOC_H
#define CLOPTS_TEST_ALLOC_H

/*
 * Fault-injecting allocator for the unit tests (tests/support/).
 *
 * test_alloc_begin() installs the allocator with clopts_set_allocator() and
 * resets the counters. It is never uninstalled by test_alloc_end(), so a
 * block allocated through it is always released through it, even when a test
 * stops early. From then on each call of malloc_fn, calloc_fn or realloc_fn is
 * one fault point: it increments test_alloc.calls and, when fail_at != 0 and
 * calls >= fail_at, fails (returns NULL; a failed realloc leaves the old
 * block as it was). test_alloc.live counts the blocks currently allocated.
 *
 * Every block carries a header, so a release of a pointer the allocator did
 * not return (or a second release of the same block) is detected: it is
 * counted in test_alloc.bad_frees and the pointer is not passed to free().
 *
 * Call test_alloc_begin() before the code under test allocates anything,
 * and test_alloc_end() after it has released everything.
 */

#include <stddef.h>

typedef struct test_alloc_state
{
    size_t fail_at;    /* 0: never fail. n: the n-th and every later fault point fail */
    size_t calls;      /* fault points passed (malloc_fn, calloc_fn and realloc_fn calls) */
    long live;         /* blocks currently allocated */
    size_t bad_frees;  /* releases of unknown or already released pointers */
    size_t zero_sizes; /* calls with a size (or a count or size) of 0 */
} test_alloc_state_t;

extern test_alloc_state_t test_alloc;

/* Installs the allocator and sets fail_at, clearing the counters. */
void test_alloc_begin(size_t fail_at);

/* Sets fail_at without clearing the counters. */
void test_alloc_set_fail_at(size_t fail_at);

/*
 * Stops failing (fail_at = 0) and returns true when no block is alive, no bad
 * release happened and no size of 0 was requested. The allocator stays
 * installed.
 */
int test_alloc_end(void);

#endif /* CLOPTS_TEST_ALLOC_H */
