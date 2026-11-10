/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * argv on Windows: the command line is split with the rules of the current
 * MSVC CRT (cmdline.c) and converted to UTF-8 (utf16_win32.c).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "process/argv_impl.h"

#include "base/alloc.h"
#include "base/size.h"
#include "process/cmdline.h"
#include "process/utf16.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <wchar.h>

#include <windows.h>

clopts_argv_t *clopts_argv_convert(int argc, const wchar_t *const *wargs)
{
    size_t total;
    size_t bytes;
    size_t pointers;
    clopts_argv_t *args;
    char **values;
    char *out;
    size_t left;
    int len;
    int n;
    int i;

    assert(argc >= 0);
    assert((wargs != NULL) || (argc == 0));

    /*
     * First pass: check every element in ascending index order and add up
     * the UTF-8 sizes; nothing is allocated before an invalid element is
     * reported. An element of length 0 is not converted: the conversion
     * fails on empty input.
     */
    total = 0;
    for (i = 0; i < argc; ++i)
    {
        assert(wargs[i] != NULL);
        len = clopts_utf16_length(wargs[i]);
        if (len < 0)
        {
            return clopts_argv_sentinel();
        }
        bytes = 0;
        if (len > 0)
        {
            switch (clopts_utf16_measure(wargs[i], len, &bytes))
            {
            case CLOPTS_UTF16_OK:
                break;
            case CLOPTS_UTF16_INVALID:
                return clopts_argv_failure(CLOPTS_ARGV_INVALID_UTF16, i);
            default:
                return clopts_argv_sentinel();
            }
        }
        /* The element and its NUL. */
        if ((clopts_size_add(bytes, 1, &bytes) != 0) || (clopts_size_add(total, bytes, &total) != 0))
        {
            return clopts_argv_sentinel();
        }
    }

    /* One block: the handle, argc + 1 pointers, then the strings. */
    if ((clopts_size_mul((size_t)argc + 1, sizeof(char *), &pointers) != 0) ||
        (clopts_size_add(pointers, sizeof(*args), &pointers) != 0) ||
        (clopts_size_add(pointers, total, &total) != 0))
    {
        return clopts_argv_sentinel();
    }
    args = (clopts_argv_t *)clopts_malloc(total);
    if (args == NULL)
    {
        return clopts_argv_sentinel();
    }
    values = (char **)(void *)(args + 1);
    out = (char *)(values + argc + 1);
    left = total - pointers;

    /* Second pass: convert into the block. */
    for (i = 0; i < argc; ++i)
    {
        len = clopts_utf16_length(wargs[i]);
        n = 0;
        if (len > 0)
        {
            n = clopts_utf16_to_utf8(wargs[i], len, out, left);
            if (n == 0)
            {
                clopts_free(args);
                return clopts_argv_sentinel();
            }
        }
        assert((size_t)n < left);
        out[n] = '\0';
        values[i] = out;
        out += (size_t)n + 1;
        left -= (size_t)n + 1;
    }
    values[argc] = NULL;

    args->status = CLOPTS_ARGV_OK;
    args->argc = argc;
    args->error_index = -1;
    args->argv = values;
    return args;
}

clopts_argv_t *clopts_argv_from_cmdline(const wchar_t *cmdline)
{
    size_t argc;
    size_t units;
    size_t bytes;
    size_t pointers;
    void *work;
    wchar_t **wargs;
    wchar_t *buf;
    clopts_argv_t *args;

    if (cmdline == NULL)
    {
        return clopts_argv_failure(CLOPTS_ARGV_CMDLINE_UNAVAILABLE, -1);
    }

    /* Count, then split into one work block: the pointer array followed by the code units. */
    clopts_cmdline_split(cmdline, NULL, NULL, &argc, &units);
    if (argc > (size_t)INT_MAX)
    {
        return clopts_argv_sentinel();
    }
    if ((clopts_size_mul(argc, sizeof(wchar_t *), &pointers) != 0) ||
        (clopts_size_mul(units, sizeof(wchar_t), &bytes) != 0) ||
        (clopts_size_add(pointers, bytes, &bytes) != 0))
    {
        return clopts_argv_sentinel();
    }
    work = clopts_malloc(bytes);
    if (work == NULL)
    {
        return clopts_argv_sentinel();
    }
    wargs = (wchar_t **)work;
    buf = (wchar_t *)(void *)(wargs + argc);
    clopts_cmdline_split(cmdline, buf, wargs, &argc, &units);

    args = clopts_argv_convert((int)argc, (const wchar_t *const *)wargs);
    clopts_free(work);
    return args;
}

clopts_argv_t *clopts_argv_create(int argc, char *const *argv)
{
    /* argc and argv are not read: the process command line is the source on Windows. */
    (void)argc;
    (void)argv;
    return clopts_argv_from_cmdline(GetCommandLineW());
}

clopts_argv_t *clopts_argv_create_from_wide(int argc, wchar_t *const *wargv)
{
    assert(argc >= 0);
    assert(wargv != NULL);
    return clopts_argv_convert(argc, (const wchar_t *const *)wargv);
}
