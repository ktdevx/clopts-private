/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include "base/size.h"

#include <assert.h>
#include <stdint.h>

int clopts_size_mul(size_t a, size_t b, size_t *out)
{
    assert(out != NULL);

    if ((a != 0) && (b > (SIZE_MAX / a)))
    {
        return -1;
    }

    *out = a * b;
    return 0;
}

int clopts_size_add(size_t a, size_t b, size_t *out)
{
    assert(out != NULL);

    if (b > (SIZE_MAX - a))
    {
        return -1;
    }

    *out = a + b;
    return 0;
}
