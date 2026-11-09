/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include "core/core_internal.h"

#include <assert.h>
#include <string.h>

int clopts_core_write(clopts_write_fn fn, void *ctx, const char *data, size_t len)
{
    assert(fn != NULL);
    assert((len == 0) || (data != NULL));

    return fn(ctx, data, len);
}

int clopts_core_write_str(clopts_write_fn fn, void *ctx, const char *s)
{
    assert(s != NULL);

    return clopts_core_write(fn, ctx, s, strlen(s));
}
