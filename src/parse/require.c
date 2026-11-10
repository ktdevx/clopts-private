/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Required checks: step 6 of parsing (parse.c).
 */

#include "parse/require.h"

#include <assert.h>

/* True when slot got a value that satisfies required (from argv or the environment). */
static bool given(const clopts_slot_t *slot)
{
    return (slot != NULL) && ((slot->source == CLOPTS_SOURCE_EXPLICIT) || (slot->source == CLOPTS_SOURCE_ENV));
}

/* The first required option of scope that was not given, or NULL. */
static const clopts_option_t *first_missing_option(const clopts_matches_t *matches, const clopts_scope_t *scope)
{
    size_t i;

    for (i = 0; i < clopts_scope_get_option_count(scope); i++)
    {
        const clopts_option_t *option = clopts_scope_get_option_at(scope, i);

        if (clopts_option_is_required(option) && !given(clopts_matches_find(matches, option)))
        {
            return option;
        }
    }
    return NULL;
}

/* Writes the missing option (positional NULL) or positional (option NULL) to *error. */
static int missing(clopts_usage_error_t *error, const clopts_command_t *command, const clopts_option_t *option,
                   const clopts_positional_t *positional)
{
    clopts_usage_error_init(error,
                            (option != NULL) ? CLOPTS_USAGE_ERROR_MISSING_OPTION : CLOPTS_USAGE_ERROR_MISSING_ARGUMENT,
                            command);
    error->option = option;
    error->positional = positional;
    return -1;
}

int clopts_require(const clopts_matches_t *matches, const clopts_program_t *program, bool include_command,
                   clopts_usage_error_t *error)
{
    const clopts_command_t *command;
    const clopts_scope_t *positional_scope;
    const clopts_option_t *option;
    size_t recorded;
    size_t i;

    assert(matches != NULL);
    assert(program != NULL);
    assert(error != NULL);

    command = include_command ? clopts_matches_get_command(matches) : NULL;

    option = first_missing_option(matches, clopts_program_get_scope(program));
    if (option != NULL)
    {
        return missing(error, command, option, NULL);
    }
    if (!include_command)
    {
        return 0;
    }

    if (command != NULL)
    {
        option = first_missing_option(matches, clopts_command_get_scope(command));
        if (option != NULL)
        {
            return missing(error, command, option, NULL);
        }
        positional_scope = clopts_command_get_scope(command);
    }
    else if (clopts_program_get_command_count(program) == 0)
    {
        positional_scope = clopts_program_get_scope(program);
    }
    else
    {
        /* NO_COMMAND: only the global options are checked. */
        return 0;
    }

    /*
     * Positionals are assigned in order and required ones precede optional
     * ones, so the i-th positional got a token exactly when i < recorded. A
     * required variadic positional therefore needs at least one token.
     */
    recorded = clopts_matches_get_positional_count(matches);
    for (i = 0; i < clopts_scope_get_positional_count(positional_scope); i++)
    {
        const clopts_positional_t *positional = clopts_scope_get_positional_at(positional_scope, i);

        if (clopts_positional_is_required(positional) && (i >= recorded))
        {
            return missing(error, command, NULL, positional);
        }
    }
    return 0;
}
