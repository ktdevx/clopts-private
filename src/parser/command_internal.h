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
    bool attached;                  /* owned by a program and read-only */
    char *name;                     /* owned copy */
    char *summary;                  /* may be NULL; owned copy */
    char *description;              /* may be NULL; owned copy */
    clopts_core_vec_t options;      /* clopts_option_t * elements, in order of attachment */
    clopts_core_vec_t positionals;  /* clopts_positional_t * elements, in order of attachment */
};

/* Releases command with its options, positionals and texts, attached or not. */
void clopts_parser_command_release(clopts_command_t *command);

#endif /* CLOPTS_PARSER_COMMAND_INTERNAL_H */
