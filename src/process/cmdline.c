/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Command line splitting with the rules of the current MSVC CRT.
 *
 * argv[0] and the later elements follow different rules (rule IDs R0-R9 of
 * the design). One pass both counts and, when buf is not NULL, fills, so the
 * rules are written only once. Only U+0020 and U+0009 separate; every other
 * code unit, surrogates included, is copied as is.
 */

#include "process/cmdline.h"

#include <assert.h>
#include <stdbool.h>

#define SPLIT_QUOTE ((clopts_utf16_t)0x0022)
#define SPLIT_BACKSLASH ((clopts_utf16_t)0x005C)

static bool split_is_space(clopts_utf16_t c)
{
    return c == (clopts_utf16_t)0x0020 || c == (clopts_utf16_t)0x0009;
}

/* Output position: counts every unit and writes it only when buf is not NULL. */
typedef struct split_out
{
    clopts_utf16_t *buf;
    clopts_utf16_t **args;
    size_t argc;
    size_t units;
} split_out_t;

static void split_put(split_out_t *out, clopts_utf16_t c)
{
    if (out->buf != NULL)
    {
        out->buf[out->units] = c;
    }
    out->units++;
}

static void split_begin(split_out_t *out)
{
    if (out->buf != NULL)
    {
        out->args[out->argc] = out->buf + out->units;
    }
    out->argc++;
}

static void split_put_backslashes(split_out_t *out, size_t n)
{
    while (n > 0)
    {
        split_put(out, SPLIT_BACKSLASH);
        n--;
    }
}

/* argv[0] (R0, R1). Returns the position after it. */
static const clopts_utf16_t *split_arg0(const clopts_utf16_t *p,
                                                        split_out_t *out)
{
    bool quoted = false;

    split_begin(out);
    while (*p != 0 && (quoted || !split_is_space(*p)))
    {
        if (*p == SPLIT_QUOTE)
        {
            quoted = !quoted;
        }
        else
        {
            split_put(out, *p);
        }
        p++;
    }
    split_put(out, 0);
    return p;
}

/* One element after argv[0], starting at a non-space unit (R3-R9). Returns the position after it. */
static const clopts_utf16_t *split_arg(const clopts_utf16_t *p,
                                                       split_out_t *out)
{
    bool quoted = false;

    split_begin(out);
    while (*p != 0 && (quoted || !split_is_space(*p)))
    {
        if (*p == SPLIT_BACKSLASH)
        {
            size_t k = 0;

            while (*p == SPLIT_BACKSLASH)
            {
                k++;
                p++;
            }
            if (*p != SPLIT_QUOTE)
            {
                split_put_backslashes(out, k);            /* R6 */
                continue;
            }
            split_put_backslashes(out, k / 2);
            if (k % 2 == 1)
            {
                split_put(out, SPLIT_QUOTE);     /* R4 */
                p++;
                continue;
            }
            /* k = 2n: the quote is handled below (R3) */
        }
        if (*p == SPLIT_QUOTE)
        {
            if (quoted && p[1] == SPLIT_QUOTE)
            {
                split_put(out, SPLIT_QUOTE);     /* R5: "" inside quotes */
                p += 2;
            }
            else
            {
                quoted = !quoted;                                  /* R5 */
                p++;
            }
            continue;
        }
        split_put(out, *p);                               /* R8 */
        p++;
    }
    split_put(out, 0);                                    /* R7, R9 */
    return p;
}

void clopts_cmdline_split(const clopts_utf16_t *cmdline, clopts_utf16_t *buf,
                           clopts_utf16_t **args, size_t *argc, size_t *units)
{
    split_out_t out;
    const clopts_utf16_t *p = cmdline;

    assert(cmdline != NULL);
    assert(argc != NULL);
    assert(units != NULL);
    assert(buf == NULL || args != NULL);

    out.buf = buf;
    out.args = args;
    out.argc = 0;
    out.units = 0;

    p = split_arg0(p, &out);
    for (;;)
    {
        while (split_is_space(*p))                        /* R2 */
        {
            p++;
        }
        if (*p == 0)
        {
            break;
        }
        p = split_arg(p, &out);
    }

    *argc = out.argc;
    *units = out.units;
}
