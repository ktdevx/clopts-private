/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSER_POSITIONAL_H
#define CLOPTS_PARSER_POSITIONAL_H

#include "clopts/positional.h"

#include <stdbool.h>

/*
 * Internal representation of a positional (parser_positional.c). Like an
 * option, it does not point to the command it is attached to.
 */

struct clopts_positional
{
    char *name;                 /* not empty; owned copy */
    char *help;                 /* may be NULL; owned copy */
    bool required;
    bool variadic;              /* true for the last positional of a command only */
};

#endif /* CLOPTS_PARSER_POSITIONAL_H */
