/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Environment lookup on Windows: variables are read with
 * GetEnvironmentVariableW() and converted to UTF-8 (utf16_win32.c).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "process/env_impl.h"

#include "base/size.h"
#include "process/utf16.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <wchar.h>

#include <windows.h>

/* Code units for the value when the context has no larger work buffer yet. */
#define ENV_INITIAL_UNITS 256u

/*
 * Converts name (UTF-8, NUL-terminated) into the start of env->wide, which
 * must hold name_units code units (its NUL included). 0 on success, -1 when
 * the conversion fails.
 */
static int put_name(clopts_env_t *env, const char *name, int name_units)
{
    int n;

    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, (wchar_t *)env->wide, name_units);
    return (n == name_units) ? 0 : -1;
}

clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value)
{
    clopts_env_t *env;
    int name_units;
    size_t units;
    size_t room;
    DWORD got;
    DWORD error;
    wchar_t *wvalue;
    size_t bytes;
    int n;

    assert(ctx != NULL);
    assert(name != NULL);
    assert(value != NULL);
    env = (clopts_env_t *)ctx;

    /* The name in UTF-16, its NUL included; invalid UTF-8 is not replaced. */
    name_units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    if (name_units <= 0)
    {
        /* No variable can have a name that is not valid UTF-8. */
        return (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) ? CLOPTS_ENV_UNSET : CLOPTS_ENV_FAILED;
    }

    /*
     * env->wide holds the name followed by the value. Growing it does not
     * keep its contents, so the name is converted again after every reserve.
     */
    if (clopts_size_add((size_t)name_units, ENV_INITIAL_UNITS, &units) != 0)
    {
        return CLOPTS_ENV_FAILED;
    }
    for (;;)
    {
        if ((clopts_env_reserve_wide(env, units) != 0) || (put_name(env, name, name_units) != 0))
        {
            return CLOPTS_ENV_FAILED;
        }
        wvalue = (wchar_t *)env->wide + name_units;
        room = env->wide_cap - (size_t)name_units;
        if (room > (size_t)INT_MAX)
        {
            room = (size_t)INT_MAX;   /* keeps every length below an int for the conversion */
        }

        SetLastError(0);
        got = GetEnvironmentVariableW((const wchar_t *)env->wide, wvalue, (DWORD)room);
        if (got == 0)
        {
            error = GetLastError();
            if (error == ERROR_ENVVAR_NOT_FOUND)
            {
                return CLOPTS_ENV_UNSET;
            }
            if (error != 0)
            {
                return CLOPTS_ENV_FAILED;
            }
            break;   /* set to the empty string */
        }
        if ((size_t)got < room)
        {
            break;   /* fits: got code units before the NUL */
        }
        /*
         * Too small: got is the size needed with the NUL. The value can change
         * between calls, so read again until it fits.
         */
        if ((size_t)got >= (size_t)INT_MAX)
        {
            return CLOPTS_ENV_FAILED;
        }
        if (clopts_size_add((size_t)name_units, ((size_t)got > room) ? (size_t)got : room + 1, &units) != 0)
        {
            return CLOPTS_ENV_FAILED;
        }
    }

    /* The value in UTF-8 with a NUL; invalid UTF-16 is not replaced. */
    bytes = 0;
    if (got > 0)
    {
        switch (clopts_utf16_measure(wvalue, (int)got, &bytes))
        {
        case CLOPTS_UTF16_OK:
            break;
        case CLOPTS_UTF16_INVALID:
            return CLOPTS_ENV_INVALID;
        default:
            return CLOPTS_ENV_FAILED;
        }
    }
    if ((bytes > (size_t)INT_MAX) || (clopts_size_add(bytes, 1, &units) != 0) ||
        (clopts_env_reserve_value(env, units) != 0))
    {
        return CLOPTS_ENV_FAILED;
    }
    n = 0;
    if (got > 0)
    {
        n = clopts_utf16_to_utf8(wvalue, (int)got, env->value, bytes);
        if (n != (int)bytes)
        {
            return CLOPTS_ENV_FAILED;
        }
    }
    env->value[n] = '\0';
    *value = env->value;
    return CLOPTS_ENV_FOUND;
}
