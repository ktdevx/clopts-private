/*
 * Suite crt (Windows only; built only on Windows): starts the probe
 * executable with raw command lines and compares clopts_argv_get with the
 * CRT's __argc / __argv there.
 *
 * It also checks that a command line built with test_util can be compared
 * with what the CRT split gives, element by element.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <wchar.h>

NAOS_TEST(u16_from_ascii_builds_a_wide_command_line)
{
    platform_test_u16_t cmdline;
    platform_test_u16_t a;
    platform_test_u16_t b;
    clopts_platform_u16_t *args[2];
    static const char *const expected[] = {"p", "foo\"bar", NULL};

    platform_test_u16_from_ascii(&cmdline, "p \"foo\"\"bar\"");
    NAOS_CHECK_INT_EQ(wcscmp(cmdline.units, L"p \"foo\"\"bar\""), 0);

    args[0] = platform_test_u16_from_ascii(&a, "p");
    args[1] = platform_test_u16_from_ascii(&b, "foo\"bar");
    NAOS_CHECK_INT_EQ(platform_test_split_mismatch(args, 2, expected), -1);
}

NAOS_SUITE(crt,
    NAOS_ADD_TEST(u16_from_ascii_builds_a_wide_command_line)
)
