/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSER_COMMAND_INTERNAL_H
#define CLOPTS_PARSER_COMMAND_INTERNAL_H

#include "clopts/command.h"
#include "core/core_internal.h"

#include <stdbool.h>

/*
 * Internal representation of a command, the root included
 * (command.c). A command does not point to the program it is
 * attached to. Attached, it is owned by that program and read-only.
 */

struct clopts_command
{
    char *name;                     /* owned copy */
    char *summary;                  /* may be NULL; owned copy */
    char *description;              /* may be NULL; owned copy */
    clopts_option_t **options;          /* option_count elements, in order of attachment; NULL when option_cap == 0 */
    size_t option_count;
    size_t option_cap;                  /* elements allocated */
    clopts_positional_t **positionals;  /* positional_count elements, in order of attachment; NULL when positional_cap == 0 */
    size_t positional_count;
    size_t positional_cap;              /* elements allocated */
};

#endif /* CLOPTS_PARSER_COMMAND_INTERNAL_H */
