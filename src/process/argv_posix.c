/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * argv elsewhere than Windows: passed through as is.
 */

#include "process/argv_internal.h"

#include <assert.h>
#include <stddef.h>

clopts_argv_t *clopts_argv_create(int argc, char *const *argv)
{
    assert(argc >= 0);
    assert(argc == 0 || argv != NULL);
    /* Neither the array nor the strings are copied or converted. */
    return clopts_argv_passthrough(argc, argv);
}
