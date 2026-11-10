/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * UTF-16 to UTF-8 on Windows, for argv (argv_win32.c) and for environment
 * values (env_win32.c).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "process/utf16.h"

#include <assert.h>
#include <limits.h>
#include <wchar.h>

#include <windows.h>

int clopts_utf16_length(const wchar_t *s)
{
    size_t len;

    assert(s != NULL);
    len = wcslen(s);
    return (len > (size_t)INT_MAX) ? -1 : (int)len;
}

clopts_utf16_status_t clopts_utf16_measure(const wchar_t *s, int len, size_t *bytes)
{
    int n;

    assert(s != NULL);
    assert(len > 0);
    assert(bytes != NULL);

    n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, len, NULL, 0, NULL, NULL);
    if (n <= 0)
    {
        return (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) ? CLOPTS_UTF16_INVALID : CLOPTS_UTF16_FAILED;
    }
    *bytes = (size_t)n;
    return CLOPTS_UTF16_OK;
}

int clopts_utf16_to_utf8(const wchar_t *s, int len, char *out, size_t room)
{
    int n;

    assert(s != NULL);
    assert(len > 0);
    assert(out != NULL);

    n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, len, out,
                            (room > (size_t)INT_MAX) ? INT_MAX : (int)room, NULL, NULL);
    return (n > 0) ? n : 0;
}
