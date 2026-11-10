/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * The descriptions of the declaration errors, generated from CLOPTS_DECL_ERROR_LIST.
 */

#include "clopts/decl/decl_error.h"

#include <stddef.h>

const char *clopts_decl_error_get_message(clopts_decl_error_t error)
{
    switch (error)
    {
#define CLOPTS_DECL_ERROR_CASE(name, message) \
    case CLOPTS_DECL_ERROR_##name:            \
        return message;
    CLOPTS_DECL_ERROR_LIST(CLOPTS_DECL_ERROR_CASE)
#undef CLOPTS_DECL_ERROR_CASE
    }
    return NULL;
}
