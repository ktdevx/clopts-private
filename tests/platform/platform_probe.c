/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * platform_probe (Windows only): compares clopts_argv_create() with the CRT's
 * __argc / __argv of this process. Suite crt starts it with raw command
 * lines.
 *
 * Exit code: 0 when the element counts and every string match, 1 on a
 * mismatch, 2 when clopts_argv_create() fails. The first difference is
 * written to stderr.
 */

#include <clopts/argv.h>

#include <stdio.h>
#include <stdlib.h>   /* __argc / __argv */
#include <string.h>

/*
 * Same workaround as the Windows part of test_argv.c: MinGW-w64 declares
 * __argc / __argv as macros in <stdlib.h> even with the language extensions
 * off, but MSVC's UCRT may hide the macros when the non-standard names are
 * disabled (/std:c11 without extensions), so when the macros are missing
 * they are defined here over the accessors, which every CRT that lacks the
 * macros exports.
 */
#ifndef __argc
int *__cdecl __p___argc(void);
#define __argc (*__p___argc())
#endif
#ifndef __argv
char ***__cdecl __p___argv(void);
#define __argv (*__p___argv())
#endif

static int platform_probe_compare(const clopts_argv_t *args)
{
    char *const *values;
    int count;
    int i;

    if (clopts_argv_get_status(args) != CLOPTS_ARGV_OK)
    {
        fprintf(stderr, "platform_probe: clopts_argv_create failed (status %d)\n",
                (int)clopts_argv_get_status(args));
        return 2;
    }
    count = clopts_argv_get_count(args);
    values = clopts_argv_get_values(args);
    if (count != __argc)
    {
        fprintf(stderr, "platform_probe: count %d, CRT __argc %d\n", count, __argc);
        return 1;
    }
    for (i = 0; i < count; ++i)
    {
        if ((values[i] == NULL) || (__argv[i] == NULL) || (strcmp(values[i], __argv[i]) != 0))
        {
            fprintf(stderr, "platform_probe: argv[%d] is \"%s\", CRT __argv[%d] is \"%s\"\n", i,
                    (values[i] != NULL) ? values[i] : "(null)", i,
                    (__argv[i] != NULL) ? __argv[i] : "(null)");
            return 1;
        }
    }
    if (values[count] != NULL)
    {
        fprintf(stderr, "platform_probe: argv[%d] is not NULL\n", count);
        return 1;
    }
    return 0;
}

int main(void)
{
    clopts_argv_t *args;
    int result;

    args = clopts_argv_create(0, NULL);
    result = platform_probe_compare(args);
    clopts_argv_destroy(args);
    return result;
}
