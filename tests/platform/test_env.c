/*
 * Suite env: environment lookup contexts (creation, release, the failure
 * record and the buffers) and clopts_env_lookup on every OS (getenv outside
 * Windows, the UTF-16 environment and UTF-8 conversion on Windows).
 *
 * It also checks the code unit array helper of test_util.
 */

/*
 * Declares setenv / unsetenv outside Windows even in C99 without language
 * extensions (-std=c99 -Werror). Must come before every include.
 */
#define _POSIX_C_SOURCE 200112L

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

NAOS_TEST(u16_from_units_copies_any_unit_and_terminates)
{
    /* a lone high surrogate, a pair, an embedded 0 and the largest unit */
    static const uint16_t units[] = {0xD800, 0xD83D, 0xDE00, 0x0000, 0xFFFF, 0x0041};
    platform_test_u16_t s;
    clopts_platform_u16_t *out;
    size_t i;

    out = platform_test_u16_from_units(&s, units, sizeof(units) / sizeof(units[0]));
    NAOS_CHECK_PTR_EQ(out, s.units);
    NAOS_CHECK_UINT_EQ(s.len, 6);
    for (i = 0; i < s.len; ++i)
    {
        NAOS_CHECK_UINT_EQ(s.units[i], units[i]);
    }
    NAOS_CHECK_UINT_EQ(s.units[6], 0);
}

NAOS_TEST(u16_from_units_accepts_an_empty_sequence)
{
    platform_test_u16_t s;

    platform_test_u16_from_units(&s, NULL, 0);
    NAOS_CHECK_UINT_EQ(s.len, 0);
    NAOS_CHECK_UINT_EQ(s.units[0], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, ""));
}

NAOS_TEST(u16_eq_ascii_does_not_match_non_ascii_units)
{
    /* U+3041 has the low byte 0x41 ('A'); it must not compare equal */
    static const uint16_t units[] = {0x3041};
    platform_test_u16_t s;

    platform_test_u16_from_units(&s, units, 1);
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "A"));
}

NAOS_SUITE(env,
    NAOS_ADD_TEST(u16_from_units_copies_any_unit_and_terminates),
    NAOS_ADD_TEST(u16_from_units_accepts_an_empty_sequence),
    NAOS_ADD_TEST(u16_eq_ascii_does_not_match_non_ascii_units)
)
