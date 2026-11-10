/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Environment lookup elsewhere than Windows: variables are read with
 * getenv(), whose pointer is lent as is. The library calls getenv only here.
 */

#include "process/env_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value)
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
