/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Shared helpers for the platform tests (see test_util.h).
 */

#include "test_util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

clopts_platform_u16_t *platform_test_u16_from_ascii(platform_test_u16_t *out, const char *ascii)
{
    size_t len;
    size_t i;

    assert(out != NULL);
    assert(ascii != NULL);
    len = strlen(ascii);
    if (len >= PLATFORM_TEST_U16_CAP)
    {
        abort(); /* kept under NDEBUG: overflowing the fixed buffer must not go unnoticed */
    }

    for (i = 0; i < len; ++i)
    {
        unsigned char byte = (unsigned char)ascii[i];

        assert(byte < 0x80);
        out->units[i] = (clopts_platform_u16_t)byte;
    }
    out->units[len] = 0;
    out->len = len;
    return out->units;
}

clopts_platform_u16_t *platform_test_u16_from_units(platform_test_u16_t *out, const uint16_t *units,
                                                    size_t count)
{
    size_t i;

    assert(out != NULL);
    assert(units != NULL || count == 0);
    if (count >= PLATFORM_TEST_U16_CAP)
    {
        abort(); /* kept under NDEBUG */
    }

    for (i = 0; i < count; ++i)
    {
        out->units[i] = (clopts_platform_u16_t)units[i];
    }
    out->units[count] = 0;
    out->len = count;
    return out->units;
}

bool platform_test_u16_eq_ascii(const clopts_platform_u16_t *s, const char *ascii)
{
    size_t i;

    assert(s != NULL);
    assert(ascii != NULL);

    for (i = 0; ascii[i] != '\0'; ++i)
    {
        if ((unsigned long)s[i] != (unsigned long)(unsigned char)ascii[i])
        {
            return false;   /* includes s ending first (s[i] == 0) */
        }
    }
    return s[i] == 0;
}

long platform_test_split_mismatch(clopts_platform_u16_t *const *args, size_t argc,
                                  const char *const *expected)
{
    size_t i;

    assert(args != NULL || argc == 0);
    assert(expected != NULL);

    for (i = 0; i < argc && expected[i] != NULL; ++i)
    {
        if (!platform_test_u16_eq_ascii(args[i], expected[i]))
        {
            return (long)i;
        }
    }
    if (i < argc || expected[i] != NULL)
    {
        return (long)i;   /* one list is a prefix of the other */
    }
    return -1;
}
