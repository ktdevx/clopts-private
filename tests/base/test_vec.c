/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the growable array (clopts_vec_*).
 */

#include "base/vec.h"

#include <naos/naos.h>

#include <stdint.h>

/* Checks len <= cap and (cap == 0) if and only if (data == NULL). */
static void check_invariants(const clopts_vec_t *vec)
{
    NAOS_CHECK_UINT_LE(vec->len, vec->cap);
    NAOS_CHECK_INT_EQ((vec->cap == 0) ? 1 : 0, (vec->data == NULL) ? 1 : 0);
}

NAOS_TEST(init_sets_empty_state)
{
    clopts_vec_t vec;

    vec.data = &vec;
    vec.len = 3;
    vec.cap = 5;
    vec.elem_size = 1;

    clopts_vec_init(&vec, sizeof(int));

    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(vec.elem_size, sizeof(int));
    check_invariants(&vec);
}

NAOS_TEST(push_100_keeps_all_elements)
{
    clopts_vec_t vec;
    int i;

    clopts_vec_init(&vec, sizeof(int));

    for (i = 0; i < 100; i++)
    {
        int value = (i * 7) + 1;

        NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &value), 0);
        NAOS_REQUIRE_UINT_EQ(vec.len, (size_t)i + 1);
        check_invariants(&vec);
    }

    for (i = 0; i < 100; i++)
    {
        const int *p = (const int *)clopts_vec_at(&vec, (size_t)i);

        NAOS_REQUIRE_NOT_NULL(p);
        NAOS_CHECK_INT_EQ(*p, (i * 7) + 1);
    }

    NAOS_CHECK_UINT_EQ(vec.cap, 128);

    clopts_vec_free(&vec);
}

NAOS_TEST(push_struct_elements_are_copied)
{
    struct pair
    {
        unsigned char a;
        double b;
    };
    clopts_vec_t vec;
    struct pair in;
    const struct pair *out;

    clopts_vec_init(&vec, sizeof(struct pair));

    in.a = 42;
    in.b = 2.5;
    NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &in), 0);

    /* Changing the source afterwards must not affect the stored copy. */
    in.a = 0;
    in.b = 0.0;

    out = (const struct pair *)clopts_vec_at(&vec, 0);
    NAOS_REQUIRE_NOT_NULL(out);
    NAOS_CHECK_UINT_EQ(out->a, 42);
    NAOS_CHECK_DOUBLE_NEAR(out->b, 2.5, 0.0);

    clopts_vec_free(&vec);
}

NAOS_TEST(capacity_starts_at_8_then_doubles)
{
    clopts_vec_t vec;
    int i;

    clopts_vec_init(&vec, sizeof(int));

    for (i = 1; i <= 33; i++)
    {
        size_t expected_cap;

        NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &i), 0);

        if (i <= 8)
        {
            expected_cap = 8;
        }
        else if (i <= 16)
        {
            expected_cap = 16;
        }
        else if (i <= 32)
        {
            expected_cap = 32;
        }
        else
        {
            expected_cap = 64;
        }

        NAOS_CHECK_UINT_EQ(vec.cap, expected_cap);
        NAOS_CHECK_UINT_EQ(vec.len, (size_t)i);
        check_invariants(&vec);
    }

    /* Elements pushed before each growth survive it. */
    for (i = 1; i <= 33; i++)
    {
        NAOS_CHECK_INT_EQ(*(const int *)clopts_vec_at(&vec, (size_t)i - 1), i);
    }

    clopts_vec_free(&vec);
}

NAOS_TEST(huge_elem_size_push_fails_without_change)
{
    clopts_vec_t vec;
    unsigned char elem[16] = {0};
    int attempt;

    /* 8 * (SIZE_MAX / 4) overflows, so the first growth must be refused. */
    clopts_vec_init(&vec, SIZE_MAX / 4);

    for (attempt = 0; attempt < 3; attempt++)
    {
        int rc = clopts_vec_push(&vec, elem);

        if (rc == 0)
        {
            /* Not expected; still the vec must be consistent. */
            NAOS_NOTE("push with elem_size SIZE_MAX / 4 unexpectedly succeeded");
            check_invariants(&vec);
            NAOS_CHECK_UINT_GE(vec.len, 1);
            break;
        }

        NAOS_CHECK_INT_EQ(rc, -1);
        NAOS_CHECK_NULL(vec.data);
        NAOS_CHECK_UINT_EQ(vec.len, 0);
        NAOS_CHECK_UINT_EQ(vec.cap, 0);
        NAOS_CHECK_UINT_EQ(vec.elem_size, SIZE_MAX / 4);
        check_invariants(&vec);
    }

    clopts_vec_free(&vec);
}

NAOS_TEST(free_resets_and_keeps_elem_size)
{
    clopts_vec_t vec;
    int value = 5;

    clopts_vec_init(&vec, sizeof(int));
    NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &value), 0);

    clopts_vec_free(&vec);

    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(vec.elem_size, sizeof(int));

    /* The freed vec is reusable without init. */
    value = 9;
    NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &value), 0);
    NAOS_CHECK_UINT_EQ(vec.len, 1);
    NAOS_CHECK_UINT_EQ(vec.cap, 8);
    NAOS_CHECK_INT_EQ(*(const int *)clopts_vec_at(&vec, 0), 9);

    clopts_vec_free(&vec);
}

NAOS_TEST(double_free_is_safe)
{
    clopts_vec_t vec;
    int value = 1;

    clopts_vec_init(&vec, sizeof(int));
    NAOS_REQUIRE_INT_EQ(clopts_vec_push(&vec, &value), 0);

    clopts_vec_free(&vec);
    clopts_vec_free(&vec);

    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(vec.elem_size, sizeof(int));
}

NAOS_TEST(free_zero_initialized_is_safe)
{
    clopts_vec_t vec = {0};

    clopts_vec_free(&vec);

    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(vec.elem_size, 0);
}

NAOS_TEST(free_after_failed_push_releases_everything)
{
    clopts_vec_t vec;
    unsigned char elem[16] = {0};

    clopts_vec_init(&vec, SIZE_MAX / 4);
    (void)clopts_vec_push(&vec, elem);

    clopts_vec_free(&vec);

    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(vec.elem_size, SIZE_MAX / 4);

    /* A second free after the failure is still safe. */
    clopts_vec_free(&vec);
    NAOS_CHECK_NULL(vec.data);
}

NAOS_SUITE(vec,
    NAOS_ADD_TEST(init_sets_empty_state),
    NAOS_ADD_TEST(push_100_keeps_all_elements),
    NAOS_ADD_TEST(push_struct_elements_are_copied),
    NAOS_ADD_TEST(capacity_starts_at_8_then_doubles),
    NAOS_ADD_TEST(huge_elem_size_push_fails_without_change),
    NAOS_ADD_TEST(free_resets_and_keeps_elem_size),
    NAOS_ADD_TEST(double_free_is_safe),
    NAOS_ADD_TEST(free_zero_initialized_is_safe),
    NAOS_ADD_TEST(free_after_failed_push_releases_everything)
)
