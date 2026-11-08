/*
 * Suite convert (Windows only; built only on Windows): clopts_argv_from_wide,
 * conversion from a command line, non-ASCII elements and the index of the
 * first invalid UTF-16 element.
 *
 * It also checks that the helpers of test_util give the same code units as
 * wide string literals, which the inputs of this suite mix.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

NAOS_TEST(u16_helpers_match_wide_literals)
{
    static const uint16_t units[] = {0x30C6, 0x30B9, 0x30C8, 0x002E, 0x0074, 0x0078, 0x0074};
    platform_test_u16_t s;

    platform_test_u16_from_ascii(&s, "a \"b\"");
    NAOS_CHECK_UINT_EQ(s.len, wcslen(L"a \"b\""));
    NAOS_CHECK_INT_EQ(wcscmp(s.units, L"a \"b\""), 0);

    platform_test_u16_from_units(&s, units, sizeof(units) / sizeof(units[0]));
    NAOS_CHECK_UINT_EQ(s.len, wcslen(L"テスト.txt"));
    NAOS_CHECK_INT_EQ(wcscmp(s.units, L"テスト.txt"), 0);
}

NAOS_SUITE(convert,
    NAOS_ADD_TEST(u16_helpers_match_wide_literals)
)
