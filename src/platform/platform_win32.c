/*
 * argv and environment lookup on Windows: the command line is split with the
 * rules of the current MSVC CRT and converted to UTF-8, and variables are
 * read with GetEnvironmentVariableW() and converted to UTF-8.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "platform/platform_internal.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <wchar.h>

#include <windows.h>

/*
 * Outcome of measuring one element: its UTF-8 size in bytes (without the
 * NUL) or why it cannot be converted.
 */
typedef enum platform_win32_measure
{
    PLATFORM_WIN32_MEASURE_OK,
    PLATFORM_WIN32_MEASURE_INVALID,   /* not valid UTF-16 */
    PLATFORM_WIN32_MEASURE_FAILED     /* any other failure; treated as out of memory */
} platform_win32_measure_t;

/* The length of s in code units as an int; -1 when it does not fit. */
static int platform_win32_length(const wchar_t *s)
{
    size_t len = wcslen(s);

    return (len > (size_t)INT_MAX) ? -1 : (int)len;
}

/* Measures the UTF-8 size of s (len code units, len > 0) without replacing invalid UTF-16. */
static platform_win32_measure_t platform_win32_measure(const wchar_t *s, int len, size_t *bytes)
{
    int n;

    n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, len, NULL, 0, NULL, NULL);
    if (n <= 0)
    {
        return (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) ? PLATFORM_WIN32_MEASURE_INVALID
                                                                : PLATFORM_WIN32_MEASURE_FAILED;
    }
    *bytes = (size_t)n;
    return PLATFORM_WIN32_MEASURE_OK;
}

clopts_argv_t *clopts_platform_argv_convert(clopts_platform_probe_t *probe, int argc, const wchar_t *const *wargs)
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
     * reported. An element of length 0 is not passed to
     * WideCharToMultiByte(), which fails on empty input.
     */
    total = 0;
    for (i = 0; i < argc; ++i)
    {
        assert(wargs[i] != NULL);
        len = platform_win32_length(wargs[i]);
        if (len < 0)
        {
            return clopts_platform_argv_sentinel();
        }
        bytes = 0;
        if (len > 0)
        {
            switch (platform_win32_measure(wargs[i], len, &bytes))
            {
            case PLATFORM_WIN32_MEASURE_OK:
                break;
            case PLATFORM_WIN32_MEASURE_INVALID:
                return clopts_platform_argv_failure(probe, CLOPTS_ARGV_INVALID_UTF16, i);
            default:
                return clopts_platform_argv_sentinel();
            }
        }
        /* The element and its NUL. */
        if ((clopts_core_size_add(bytes, 1, &bytes) != 0) || (clopts_core_size_add(total, bytes, &total) != 0))
        {
            return clopts_platform_argv_sentinel();
        }
    }

    /* One block: the handle, argc + 1 pointers, then the strings. */
    if ((clopts_core_size_mul((size_t)argc + 1, sizeof(char *), &pointers) != 0) ||
        (clopts_core_size_add(pointers, sizeof(*args), &pointers) != 0) ||
        (clopts_core_size_add(pointers, total, &total) != 0))
    {
        return clopts_platform_argv_sentinel();
    }
    args = (clopts_argv_t *)clopts_platform_malloc(probe, total);
    if (args == NULL)
    {
        return clopts_platform_argv_sentinel();
    }
    values = (char **)(void *)(args + 1);
    out = (char *)(values + argc + 1);
    left = total - pointers;

    /* Second pass: convert into the block. */
    for (i = 0; i < argc; ++i)
    {
        len = platform_win32_length(wargs[i]);
        n = 0;
        if (len > 0)
        {
            n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargs[i], len, out,
                                    (left > (size_t)INT_MAX) ? INT_MAX : (int)left, NULL, NULL);
            if (n <= 0)
            {
                clopts_platform_free(probe, args);
                return clopts_platform_argv_sentinel();
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

clopts_argv_t *clopts_platform_argv_from_cmdline(clopts_platform_probe_t *probe, const wchar_t *cmdline)
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
        return clopts_platform_argv_failure(probe, CLOPTS_ARGV_CMDLINE_UNAVAILABLE, -1);
    }

    /* Count, then split into one work block: the pointer array followed by the code units. */
    clopts_platform_split(cmdline, NULL, NULL, &argc, &units);
    if (argc > (size_t)INT_MAX)
    {
        return clopts_platform_argv_sentinel();
    }
    if ((clopts_core_size_mul(argc, sizeof(wchar_t *), &pointers) != 0) ||
        (clopts_core_size_mul(units, sizeof(wchar_t), &bytes) != 0) ||
        (clopts_core_size_add(pointers, bytes, &bytes) != 0))
    {
        return clopts_platform_argv_sentinel();
    }
    work = clopts_platform_malloc(probe, bytes);
    if (work == NULL)
    {
        return clopts_platform_argv_sentinel();
    }
    wargs = (wchar_t **)work;
    buf = (wchar_t *)(void *)(wargs + argc);
    clopts_platform_split(cmdline, buf, wargs, &argc, &units);

    args = clopts_platform_argv_convert(probe, (int)argc, (const wchar_t *const *)wargs);
    clopts_platform_free(probe, work);
    return args;
}

clopts_argv_t *clopts_argv_get(int argc, char *const *argv)
{
    /* argc and argv are not read: the process command line is the source on Windows. */
    (void)argc;
    (void)argv;
    return clopts_platform_argv_from_cmdline(NULL, GetCommandLineW());
}

clopts_argv_t *clopts_argv_from_wide(int argc, wchar_t *const *wargv)
{
    assert(argc >= 0);
    assert(wargv != NULL);
    return clopts_platform_argv_convert(NULL, argc, (const wchar_t *const *)wargv);
}

clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_FAILED;
}
