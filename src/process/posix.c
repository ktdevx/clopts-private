/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * argv and environment lookup elsewhere than Windows: argv is passed through
 * and variables are read with getenv(), whose pointer is lent as is. The
 * library calls getenv only here.
 */

#include "process/process_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

clopts_argv_t *clopts_argv_create(int argc, char *const *argv)
{
    assert(argc >= 0);
    assert(argc == 0 || argv != NULL);
    /* Neither the array nor the strings are copied or converted. */
    return clopts_process_argv_passthrough(argc, argv);
}

clopts_env_status_t clopts_process_env_lookup(void *ctx, const char *name, const char **value)
{
    const char *found;

    assert(ctx != NULL);
    assert(name != NULL);
    assert(value != NULL);
    (void)ctx;   /* nothing is converted, so no buffer is needed */

    found = getenv(name);
    if (found == NULL)
    {
        return CLOPTS_ENV_UNSET;
    }
    /* Borrowed from the process environment; an empty string is FOUND too. */
    *value = found;
    return CLOPTS_ENV_FOUND;
}
