/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * The descriptions of the declaration errors, generated from CLOPTS_ERROR_LIST.
 */

#include "clopts/error.h"

#include <stddef.h>

const char *clopts_error_get_message(clopts_error_t error)
{
    switch (error)
    {
#define CLOPTS_ERROR_CASE(name, message) \
    case CLOPTS_ERROR_##name:            \
        return message;
    CLOPTS_ERROR_LIST(CLOPTS_ERROR_CASE)
#undef CLOPTS_ERROR_CASE
    }
    return NULL;
}
