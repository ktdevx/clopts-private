/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Lookup: the order of the scopes a written name is looked up in. The names
 * themselves are compared by scope.c.
 */

#include "decl/lookup.h"

#include "decl/scope.h"

#include <assert.h>

static clopts_lookup_t long_in(const clopts_scope_t *scope, const char *name, size_t name_len)
{
    clopts_lookup_t found;

    found.negated = false;
    found.option = clopts_scope_find_long(scope, name, name_len, &found.negated);
    return found;
}

static clopts_lookup_t short_in(const clopts_scope_t *scope, char c)
{
    clopts_lookup_t found;

    found.negated = false;
    found.option = clopts_scope_find_short(scope, c);
    return found;
}

clopts_lookup_t clopts_lookup_long(const clopts_program_t *program, const clopts_command_t *command,
                                   const char *name, size_t name_len)
{
    clopts_lookup_t found;

    assert((program != NULL) && (name != NULL));
    if (command != NULL)
    {
        found = long_in(clopts_command_get_scope(command), name, name_len);
        if (found.option != NULL)
        {
            return found;
        }
    }
    return long_in(clopts_program_get_scope(program), name, name_len);
}

clopts_lookup_t clopts_lookup_short(const clopts_program_t *program, const clopts_command_t *command, char c)
{
    clopts_lookup_t found;

    assert(program != NULL);
    if (command != NULL)
    {
        found = short_in(clopts_command_get_scope(command), c);
        if (found.option != NULL)
        {
            return found;
        }
    }
    return short_in(clopts_program_get_scope(program), c);
}
