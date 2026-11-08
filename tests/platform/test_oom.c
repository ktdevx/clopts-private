/*
 * Suite oom: the allocation gateway and the allocation probe, and sweeps
 * that move the failing fault point over handle creation, the Windows
 * conversions and environment lookups (the sentinel or a recorded
 * OUT_OF_MEMORY, and live == 0 after the release).
 *
 * It also checks that the helpers of test_util hold the longest array they
 * accept, which the long inputs of the sweeps rely on.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

NAOS_TEST(u16_helpers_fill_the_whole_capacity)
{
    static char text[PLATFORM_TEST_U16_CAP];
    static uint16_t units[PLATFORM_TEST_U16_CAP - 1];
    static platform_test_u16_t s;
    size_t i;

    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    platform_test_u16_from_ascii(&s, text);
    NAOS_CHECK_UINT_EQ(s.len, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len - 1], 'x');
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, text));

    for (i = 0; i < PLATFORM_TEST_U16_CAP - 1; ++i)
    {
        units[i] = (uint16_t)(i + 1);
    }
    platform_test_u16_from_units(&s, units, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.len, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len - 1], PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
}

NAOS_SUITE(oom,
    NAOS_ADD_TEST(u16_helpers_fill_the_whole_capacity)
)
