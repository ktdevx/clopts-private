/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * The one reader of UTF-8 sequences in the library: declaration names are
 * checked with it, and the help text counts code points with it.
 */

#include "base/utf8.h"

#include <assert.h>
#include <string.h>

size_t clopts_utf8_sequence(const char *data, size_t avail)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned char lead;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;
    size_t n;
    size_t i;

    assert(data != NULL);
    assert(avail > 0);

    /*
     * The ranges of the second byte rule out overlong forms (E0, F0),
     * surrogates (ED) and code points above U+10FFFF (F4); C0, C1 and
     * F5..FF never start a sequence.
     */
    lead = p[0];
    if (lead < 0x80)
    {
        return 1;
    }

    if ((lead >= 0xC2) && (lead <= 0xDF))
    {
        n = 2;
    }
    else if ((lead >= 0xE0) && (lead <= 0xEF))
    {
        n = 3;
        if (lead == 0xE0)
        {
            lo = 0xA0;
        }
        else if (lead == 0xED)
        {
            hi = 0x9F;
        }
    }
    else if ((lead >= 0xF0) && (lead <= 0xF4))
    {
        n = 4;
        if (lead == 0xF0)
        {
            lo = 0x90;
        }
        else if (lead == 0xF4)
        {
            hi = 0x8F;
        }
    }
    else
    {
        return 0;
    }

    if (avail < n)
    {
        return 0;
    }

    if ((p[1] < lo) || (p[1] > hi))
    {
        return 0;
    }

    for (i = 2; i < n; i++)
    {
        if ((p[i] < 0x80) || (p[i] > 0xBF))
        {
            return 0;
        }
    }

    return n;
}

bool clopts_utf8_valid(const char *s)
{
    size_t len;
    size_t i = 0;

    assert(s != NULL);

    len = strlen(s);
    while (i < len)
    {
        size_t n = clopts_utf8_sequence(s + i, len - i);

        if (n == 0)
        {
            return false;
        }
    
        i += n;
    }

    return true;
}

size_t clopts_utf8_count(const char *data, size_t len)
{
    size_t count = 0;
    size_t i = 0;

    assert((len == 0) || (data != NULL));

    while (i < len)
    {
        size_t n = clopts_utf8_sequence(data + i, len - i);

        /* an invalid byte counts on its own; resume at the next byte */
        i += (n != 0) ? n : 1;
        count++;
    }

    return count;
}
