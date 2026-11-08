/*
 * Suite split: command line splitting with the rules of the current MSVC
 * CRT (clopts_platform_split), on every OS.
 *
 * It also checks the comparison helper of test_util, which the table of
 * this suite relies on.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>

NAOS_TEST(split_mismatch_accepts_equal_lists)
{
    platform_test_u16_t a;
    platform_test_u16_t b;
    platform_test_u16_t empty;
    clopts_platform_u16_t *args[3];
    static const char *const expected[] = {"prog", "b c", "", NULL};
    static const char *const none[] = {NULL};

    args[0] = platform_test_u16_from_ascii(&a, "prog");
    args[1] = platform_test_u16_from_ascii(&b, "b c");
    args[2] = platform_test_u16_from_ascii(&empty, "");

    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 3, expected), -1);
    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 0, none), -1);
}

NAOS_TEST(split_mismatch_reports_the_first_difference)
{
    platform_test_u16_t a;
    platform_test_u16_t b;
    clopts_platform_u16_t *args[2];
    static const char *const second_differs[] = {"prog", "bx", NULL};
    static const char *const both_differ[] = {"pro", "c", NULL};
    static const char *const longer[] = {"prog", "b", "c", NULL};
    static const char *const shorter[] = {"prog", NULL};

    args[0] = platform_test_u16_from_ascii(&a, "prog");
    args[1] = platform_test_u16_from_ascii(&b, "b");

    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 2, second_differs), 1);
    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 2, both_differ), 0);
    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 2, longer), 2);
    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 2, shorter), 1);
}

NAOS_TEST(split_mismatch_compares_non_ascii_units_as_different)
{
    static const uint16_t units[] = {0x30C6, 0x0061};
    platform_test_u16_t a;
    clopts_platform_u16_t *args[1];
    static const char *const expected[] = {"\x30" "a", NULL};

    args[0] = platform_test_u16_from_units(&a, units, 2);
    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 1, expected), 0);
}

NAOS_SUITE(split,
    NAOS_ADD_TEST(split_mismatch_accepts_equal_lists),
    NAOS_ADD_TEST(split_mismatch_reports_the_first_difference),
    NAOS_ADD_TEST(split_mismatch_compares_non_ascii_units_as_different)
)
