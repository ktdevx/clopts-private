/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_USAGE_ERROR_H
#define CLOPTS_SRC_PARSE_USAGE_ERROR_H

#include "clopts/parse/usage_error.h"

#include <stddef.h>

/*
 * Usage error (usage_error.c). The scan, the resolution and the required
 * checks fill one in; the result keeps a copy (parse/result.h).
 */
struct clopts_usage_error
{
    clopts_usage_error_kind_t kind;
    const clopts_command_t *command;        /* the selected command, or NULL */
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const char *token;                      /* an argv element, or NULL */
    int argv_index;                         /* -1 when there is none */
    size_t name_len;
    char short_name;
};

/*
 * Makes error a usage error of kind on command (NULL: none selected), with
 * every other field empty (argv_index -1). Kind NONE makes "no error".
 */
void clopts_usage_error_init(clopts_usage_error_t *error, clopts_usage_error_kind_t kind,
                             const clopts_command_t *command);

#endif /* CLOPTS_SRC_PARSE_USAGE_ERROR_H */
