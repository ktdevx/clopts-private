/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Required checks: step 6 of parsing (parse.c), after the resolution from
 * the environment and the defaults (resolve.c). The first missing required
 * option or positional is recorded as the error of the result; the status
 * is left to parse.c.
 */

#include "parser/parser_internal.h"

#include <assert.h>

/*
 * Records a missing required option (positional == NULL) or positional
 * (option == NULL) as the error of result. The status is left to parse.c.
 */
static void set_missing(clopts_result_t *result, const clopts_command_t *command,
                        const clopts_option_t *option, const clopts_positional_t *positional)
{
    clopts_parser_error_t error;

    error.usage = (option != NULL) ? CLOPTS_USAGE_MISSING_OPTION : CLOPTS_USAGE_MISSING_ARGUMENT;
    error.command = command;
    error.option = option;
    error.positional = positional;
    error.token = NULL;
    error.argv_index = -1;
    error.name_len = 0;
    error.short_name = '\0';
    clopts_parser_result_set_error(result, &error);
}

/* Returns true when slot got a value that satisfies required (from argv or the environment). */
static bool slot_given(const clopts_parser_slot_t *slot)
{
    return ((slot->source == CLOPTS_SOURCE_EXPLICIT) || (slot->source == CLOPTS_SOURCE_ENV));
}

/* Returns the first required option of scope whose slot was not given, or NULL. */
static const clopts_option_t *first_missing_option(const clopts_parser_slot_t *slots, size_t slot_count,
                                                   const clopts_scope_t *scope)
{
    size_t i;

    assert(slot_count == clopts_scope_option_count(scope));
    for (i = 0; i < slot_count; i++)
    {
        const clopts_option_t *option = clopts_scope_option_at(scope, i);

        if (clopts_option_is_required(option) && !slot_given(&slots[i]))
        {
            return option;
        }
    }
    return NULL;
}

int clopts_parser_check_required(clopts_result_t *result, const clopts_program_t *program, bool include_command)
{
    const clopts_command_t *command;
    const clopts_scope_t *positional_scope;
    const clopts_option_t *missing;
    const clopts_parser_slot_t *slots;
    size_t slot_count;
    size_t recorded;
    size_t i;

    assert(result != NULL);
    assert(result != clopts_parser_result_oom());
    assert(program != NULL);

    command = include_command ? clopts_result_get_command(result) : NULL;

    slots = clopts_parser_result_get_slots(result, true, &slot_count);
    missing = first_missing_option(slots, slot_count, clopts_program_scope(program));
    if (missing != NULL)
    {
        set_missing(result, command, missing, NULL);
        return -1;
    }
    if (!include_command)
    {
        return 0;
    }

    if (command != NULL)
    {
        slots = clopts_parser_result_get_slots(result, false, &slot_count);
        missing = first_missing_option(slots, slot_count, clopts_command_scope(command));
        if (missing != NULL)
        {
            set_missing(result, command, missing, NULL);
            return -1;
        }
        positional_scope = clopts_command_scope(command);
    }
    else if (clopts_program_get_command_count(program) == 0)
    {
        positional_scope = clopts_program_scope(program);
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
    recorded = clopts_parser_result_get_positional_count(result);
    for (i = 0; i < clopts_scope_positional_count(positional_scope); i++)
    {
        const clopts_positional_t *positional = clopts_scope_positional_at(positional_scope, i);

        if (clopts_positional_is_required(positional) && (i >= recorded))
        {
            set_missing(result, command, NULL, positional);
            return -1;
        }
    }
    return 0;
}
