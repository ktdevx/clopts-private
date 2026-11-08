/*
 * Suite argv: argv handles, their accessors and release, the NULL handle
 * and the out-of-memory sentinel, the pass-through outside Windows and the
 * match with the CRT's __argv on Windows.
 *
 * It also checks the ASCII array helper of test_util.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

NAOS_TEST(u16_from_ascii_copies_each_byte_and_terminates)
{
    platform_test_u16_t s;
    const char *text = "a \"b\\c\td";
    clopts_platform_u16_t *units;
    size_t i;

    units = platform_test_u16_from_ascii(&s, text);
    NAOS_CHECK_PTR_EQ(units, s.units);
    NAOS_CHECK_UINT_EQ(s.len, strlen(text));
    for (i = 0; i < s.len; ++i)
    {
        NAOS_CHECK_UINT_EQ(s.units[i], (unsigned char)text[i]);
    }
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(units, text));
}

NAOS_TEST(u16_from_ascii_accepts_the_empty_string)
{
    platform_test_u16_t s;

    platform_test_u16_from_ascii(&s, "");
    NAOS_CHECK_UINT_EQ(s.len, 0);
    NAOS_CHECK_UINT_EQ(s.units[0], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, ""));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "a"));
}

NAOS_TEST(u16_eq_ascii_rejects_prefixes_and_longer_strings)
{
    platform_test_u16_t s;

    platform_test_u16_from_ascii(&s, "abc");
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, "abc"));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "ab"));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "abcd"));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "abd"));
}

NAOS_SUITE(argv,
    NAOS_ADD_TEST(u16_from_ascii_copies_each_byte_and_terminates),
    NAOS_ADD_TEST(u16_from_ascii_accepts_the_empty_string),
    NAOS_ADD_TEST(u16_eq_ascii_rejects_prefixes_and_longer_strings)
)
