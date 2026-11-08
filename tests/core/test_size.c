/*
 * Unit tests for the overflow-checked size arithmetic
 * (clopts_core_size_mul / clopts_core_size_add).
 */

#include "core/core_internal.h"

#include <naos/naos.h>

#include <stdint.h>

/* Stored in *out before each call so that "left unchanged" can be checked. */
#define SENTINEL ((size_t)0x5A5A)

NAOS_TEST(mul_zero_times_any_is_zero)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_mul(0, 12345, &out), 0);
    NAOS_CHECK_UINT_EQ(out, 0);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_mul(0, SIZE_MAX, &out), 0);
    NAOS_CHECK_UINT_EQ(out, 0);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_mul(SIZE_MAX, 0, &out), 0);
    NAOS_CHECK_UINT_EQ(out, 0);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_mul(0, 0, &out), 0);
    NAOS_CHECK_UINT_EQ(out, 0);
}

NAOS_TEST(mul_size_max_times_one_fits)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_mul(SIZE_MAX, 1, &out), 0);
    NAOS_CHECK_UINT_EQ(out, SIZE_MAX);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_mul(1, SIZE_MAX, &out), 0);
    NAOS_CHECK_UINT_EQ(out, SIZE_MAX);
}

NAOS_TEST(mul_half_times_two_fits)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_mul(SIZE_MAX / 2, 2, &out), 0);
    NAOS_CHECK_UINT_EQ(out, SIZE_MAX - 1);
}

NAOS_TEST(mul_half_plus_one_times_two_overflows)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_mul((SIZE_MAX / 2) + 1, 2, &out), -1);
    NAOS_CHECK_UINT_EQ(out, SENTINEL);

    NAOS_CHECK_INT_EQ(clopts_core_size_mul(2, (SIZE_MAX / 2) + 1, &out), -1);
    NAOS_CHECK_UINT_EQ(out, SENTINEL);
}

NAOS_TEST(mul_size_max_times_size_max_overflows)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_mul(SIZE_MAX, SIZE_MAX, &out), -1);
    NAOS_CHECK_UINT_EQ(out, SENTINEL);
}

NAOS_TEST(mul_normal_values)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_mul(3, 7, &out), 0);
    NAOS_CHECK_UINT_EQ(out, 21);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_mul(8, sizeof(int), &out), 0);
    NAOS_CHECK_UINT_EQ(out, 8 * sizeof(int));
}

NAOS_TEST(add_zero_is_identity)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_add(0, 0, &out), 0);
    NAOS_CHECK_UINT_EQ(out, 0);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_add(SIZE_MAX, 0, &out), 0);
    NAOS_CHECK_UINT_EQ(out, SIZE_MAX);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_add(0, SIZE_MAX, &out), 0);
    NAOS_CHECK_UINT_EQ(out, SIZE_MAX);
}

NAOS_TEST(add_up_to_size_max_fits)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_add(SIZE_MAX - 1, 1, &out), 0);
    NAOS_CHECK_UINT_EQ(out, SIZE_MAX);

    out = SENTINEL;
    NAOS_CHECK_INT_EQ(clopts_core_size_add(SIZE_MAX / 2, (SIZE_MAX / 2) + 1, &out), 0);
    NAOS_CHECK_UINT_EQ(out, SIZE_MAX);
}

NAOS_TEST(add_past_size_max_overflows)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_add(SIZE_MAX, 1, &out), -1);
    NAOS_CHECK_UINT_EQ(out, SENTINEL);

    NAOS_CHECK_INT_EQ(clopts_core_size_add(1, SIZE_MAX, &out), -1);
    NAOS_CHECK_UINT_EQ(out, SENTINEL);

    NAOS_CHECK_INT_EQ(clopts_core_size_add((SIZE_MAX / 2) + 1, (SIZE_MAX / 2) + 1, &out), -1);
    NAOS_CHECK_UINT_EQ(out, SENTINEL);

    NAOS_CHECK_INT_EQ(clopts_core_size_add(SIZE_MAX, SIZE_MAX, &out), -1);
    NAOS_CHECK_UINT_EQ(out, SENTINEL);
}

NAOS_TEST(add_normal_values)
{
    size_t out = SENTINEL;

    NAOS_CHECK_INT_EQ(clopts_core_size_add(40, 2, &out), 0);
    NAOS_CHECK_UINT_EQ(out, 42);
}

NAOS_SUITE(size,
    NAOS_ADD_TEST(mul_zero_times_any_is_zero),
    NAOS_ADD_TEST(mul_size_max_times_one_fits),
    NAOS_ADD_TEST(mul_half_times_two_fits),
    NAOS_ADD_TEST(mul_half_plus_one_times_two_overflows),
    NAOS_ADD_TEST(mul_size_max_times_size_max_overflows),
    NAOS_ADD_TEST(mul_normal_values),
    NAOS_ADD_TEST(add_zero_is_identity),
    NAOS_ADD_TEST(add_up_to_size_max_fits),
    NAOS_ADD_TEST(add_past_size_max_overflows),
    NAOS_ADD_TEST(add_normal_values)
)
