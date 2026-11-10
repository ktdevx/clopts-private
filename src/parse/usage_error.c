/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Usage error: its fields and the public accessors. A NULL error reads as
 * "no error".
 */

#include "parse/usage_error.h"

#include <assert.h>

void clopts_usage_error_init(clopts_usage_error_t *error, clopts_usage_error_kind_t kind)
{
    assert(error != NULL);
    error->kind = kind;
    error->option = NULL;
    error->positional = NULL;
    error->token = NULL;
    error->argv_index = -1;
    error->name_len = 0;
    error->short_name = '\0';
}

clopts_usage_error_kind_t clopts_usage_error_get_kind(const clopts_usage_error_t *error)
{
    return (error != NULL) ? error->kind : CLOPTS_USAGE_ERROR_NONE;
}

const clopts_option_t *clopts_usage_error_get_option(const clopts_usage_error_t *error)
{
    return (error != NULL) ? error->option : NULL;
}

const clopts_positional_t *clopts_usage_error_get_positional(const clopts_usage_error_t *error)
{
    return (error != NULL) ? error->positional : NULL;
}

const char *clopts_usage_error_get_token(const clopts_usage_error_t *error)
{
    return (error != NULL) ? error->token : NULL;
}

int clopts_usage_error_get_argv_index(const clopts_usage_error_t *error)
{
    return (error != NULL) ? error->argv_index : -1;
}

size_t clopts_usage_error_get_name_len(const clopts_usage_error_t *error)
{
    return (error != NULL) ? error->name_len : 0;
}

char clopts_usage_error_get_short_name(const clopts_usage_error_t *error)
{
    return (error != NULL) ? error->short_name : '\0';
}
