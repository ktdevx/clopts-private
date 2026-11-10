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

#include "process/process_internal.h"

#include <assert.h>
#include <stdbool.h>

#define PROCESS_SPLIT_QUOTE ((clopts_process_u16_t)0x0022)
#define PROCESS_SPLIT_BACKSLASH ((clopts_process_u16_t)0x005C)

static bool process_split_is_space(clopts_process_u16_t c)
{
    return c == (clopts_process_u16_t)0x0020 || c == (clopts_process_u16_t)0x0009;
}

/* Output position: counts every unit and writes it only when buf is not NULL. */
typedef struct process_split_out
{
    clopts_process_u16_t *buf;
    clopts_process_u16_t **args;
    size_t argc;
    size_t units;
} process_split_out_t;

static void process_split_put(process_split_out_t *out, clopts_process_u16_t c)
{
    if (out->buf != NULL)
    {
        out->buf[out->units] = c;
    }
    out->units++;
}

static void process_split_begin(process_split_out_t *out)
{
    if (out->buf != NULL)
    {
        out->args[out->argc] = out->buf + out->units;
    }
    out->argc++;
}

static void process_split_put_backslashes(process_split_out_t *out, size_t n)
{
    while (n > 0)
    {
        process_split_put(out, PROCESS_SPLIT_BACKSLASH);
        n--;
    }
}

/* argv[0] (R0, R1). Returns the position after it. */
static const clopts_process_u16_t *process_split_arg0(const clopts_process_u16_t *p,
                                                        process_split_out_t *out)
{
    bool quoted = false;

    process_split_begin(out);
    while (*p != 0 && (quoted || !process_split_is_space(*p)))
    {
        if (*p == PROCESS_SPLIT_QUOTE)
        {
            quoted = !quoted;
        }
        else
        {
            process_split_put(out, *p);
        }
        p++;
    }
    process_split_put(out, 0);
    return p;
}

/* One element after argv[0], starting at a non-space unit (R3-R9). Returns the position after it. */
static const clopts_process_u16_t *process_split_arg(const clopts_process_u16_t *p,
                                                       process_split_out_t *out)
{
    bool quoted = false;

    process_split_begin(out);
    while (*p != 0 && (quoted || !process_split_is_space(*p)))
    {
        if (*p == PROCESS_SPLIT_BACKSLASH)
        {
            size_t k = 0;

            while (*p == PROCESS_SPLIT_BACKSLASH)
            {
                k++;
                p++;
            }
            if (*p != PROCESS_SPLIT_QUOTE)
            {
                process_split_put_backslashes(out, k);            /* R6 */
                continue;
            }
            process_split_put_backslashes(out, k / 2);
            if (k % 2 == 1)
            {
                process_split_put(out, PROCESS_SPLIT_QUOTE);     /* R4 */
                p++;
                continue;
            }
            /* k = 2n: the quote is handled below (R3) */
        }
        if (*p == PROCESS_SPLIT_QUOTE)
        {
            if (quoted && p[1] == PROCESS_SPLIT_QUOTE)
            {
                process_split_put(out, PROCESS_SPLIT_QUOTE);     /* R5: "" inside quotes */
                p += 2;
            }
            else
            {
                quoted = !quoted;                                  /* R5 */
                p++;
            }
            continue;
        }
        process_split_put(out, *p);                               /* R8 */
        p++;
    }
    process_split_put(out, 0);                                    /* R7, R9 */
    return p;
}

void clopts_process_split(const clopts_process_u16_t *cmdline, clopts_process_u16_t *buf,
                           clopts_process_u16_t **args, size_t *argc, size_t *units)
{
    process_split_out_t out;
    const clopts_process_u16_t *p = cmdline;

    assert(cmdline != NULL);
    assert(argc != NULL);
    assert(units != NULL);
    assert(buf == NULL || args != NULL);

    out.buf = buf;
    out.args = args;
    out.argc = 0;
    out.units = 0;

    p = process_split_arg0(p, &out);
    for (;;)
    {
        while (process_split_is_space(*p))                        /* R2 */
        {
            p++;
        }
        if (*p == 0)
        {
            break;
        }
        p = process_split_arg(p, &out);
    }

    *argc = out.argc;
    *units = out.units;
}
