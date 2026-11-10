/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite str: the owned strings (clopts_strdup / strndup / strrep).
 *
 * Checked here: the copies and their termination, that each copy is one
 * allocation of the gateway, that NULL and the overflow of len + 1 allocate
 * nothing, that a failed allocation returns NULL, and that clopts_strrep()
 * replaces, resets and keeps the old string on failure.
 */

#include "base/alloc.h"
#include "base/str.h"
#include "test_alloc.h"

#include <naos/naos.h>

#include <stdint.h>

NAOS_TEST(strdup_copies_with_one_allocation)
{
    char *copy;

    test_alloc_begin(0);

    copy = clopts_strdup("build");
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_CHECK_STR_EQ(copy, "build");
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
    clopts_free(copy);

    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(strdup_of_null_allocates_nothing)
{
    test_alloc_begin(0);

    NAOS_CHECK_NULL(clopts_strdup(NULL));
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 0);

    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(strndup_copies_len_bytes_and_terminates)
{
    char *copy;

    test_alloc_begin(0);

    copy = clopts_strndup("abcdef", 3);
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_CHECK_STR_EQ(copy, "abc");
    clopts_free(copy);

    copy = clopts_strndup("abc", 0);
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_CHECK_STR_EQ(copy, "");
    clopts_free(copy);

    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(strndup_overflow_allocates_nothing)
{
    test_alloc_begin(0);

    /* len + 1 overflows: strndup returns NULL before reading s. */
    NAOS_CHECK_NULL(clopts_strndup("x", SIZE_MAX));
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 0);

    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(string_failures_return_null)
{
    test_alloc_begin(1);

    NAOS_CHECK_NULL(clopts_strdup("abc"));
    NAOS_CHECK_NULL(clopts_strndup("abc", 2));
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 2);

    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(strrep_replaces_resets_and_keeps_on_failure)
{
    char *field = NULL;

    test_alloc_begin(0);

    NAOS_CHECK_INT_EQ(clopts_strrep(&field, "first"), 0);
    NAOS_REQUIRE_NOT_NULL(field);
    NAOS_CHECK_STR_EQ(field, "first");
    NAOS_CHECK_INT_EQ(clopts_strrep(&field, "second"), 0);
    NAOS_CHECK_STR_EQ(field, "second");
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);

    /* a failed copy keeps the old string */
    test_alloc_set_fail_at(test_alloc.calls + 1);
    NAOS_CHECK_INT_EQ(clopts_strrep(&field, "third"), -1);
    NAOS_CHECK_STR_EQ(field, "second");
    test_alloc_set_fail_at(0);

    /* replacing with its own current copy works (the copy is made first) */
    NAOS_CHECK_INT_EQ(clopts_strrep(&field, field), 0);
    NAOS_CHECK_STR_EQ(field, "second");

    /* NULL resets without allocating */
    NAOS_CHECK_INT_EQ(clopts_strrep(&field, NULL), 0);
    NAOS_CHECK_NULL(field);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);

    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_SUITE(str,
    NAOS_ADD_TEST(strdup_copies_with_one_allocation),
    NAOS_ADD_TEST(strdup_of_null_allocates_nothing),
    NAOS_ADD_TEST(strndup_copies_len_bytes_and_terminates),
    NAOS_ADD_TEST(strndup_overflow_allocates_nothing),
    NAOS_ADD_TEST(string_failures_return_null),
    NAOS_ADD_TEST(strrep_replaces_resets_and_keeps_on_failure)
)
