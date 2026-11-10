/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * TextCommand: the help command that goes with the help texts.
 *
 * clopts_help_command_create declares an ordinary subcommand "help" with one
 * optional positional COMMAND through the public builder functions, and
 * clopts_help_find_target reads a result that selected it through the public
 * result accessors. Nothing here writes text.
 */

#include "text/text_internal.h"

#include <assert.h>

clopts_decl_error_t clopts_help_command_create(clopts_command_t **out)
{
    clopts_command_t *command = NULL;
    clopts_positional_t *positional = NULL;
    clopts_decl_error_t error;

    assert(out != NULL);
    *out = NULL;
    error = clopts_command_create("help", &command);
    if (error == CLOPTS_DECL_ERROR_NONE)
    {
        error = clopts_command_set_summary(command, "Show help for a command");
    }
    if (error == CLOPTS_DECL_ERROR_NONE)
    {
        error = clopts_positional_create("COMMAND", &positional);
    }
    if (error == CLOPTS_DECL_ERROR_NONE)
    {
        error = clopts_positional_set_help(positional, "Command to show help for");
    }
    if (error == CLOPTS_DECL_ERROR_NONE)
    {
        error = clopts_command_add_positional(command, positional);
        if (error == CLOPTS_DECL_ERROR_NONE)
        {
            positional = NULL;
        }
    }
    if (error != CLOPTS_DECL_ERROR_NONE)
    {
        clopts_positional_destroy(positional);
        clopts_command_destroy(command);
        return error;
    }
    *out = command;
    return CLOPTS_DECL_ERROR_NONE;
}

bool clopts_help_find_target(const clopts_program_t *program, const clopts_result_t *result,
                        const clopts_command_t **target)
{
    const char *name;

    assert(program != NULL);
    assert(target != NULL);

    *target = NULL;
    if (clopts_result_get_status(result) != CLOPTS_PARSE_OK)
    {
        return false;
    }
    if (clopts_result_get_positional_count(result) == 0)
    {
        return true;
    }
    name = clopts_result_get_positional_at(result, 0);
    *target = clopts_program_find_command(program, name);
    return *target != NULL;
}
