/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

/* Declaration helpers for the unit tests (see test_decl.h). */

#include "test_decl.h"

#include <stddef.h>

clopts_program_t *test_decl_finish(clopts_program_t *program)
{
    if (clopts_program_get_error(program) != CLOPTS_ERROR_NONE)
    {
        clopts_program_destroy(program);
        return NULL;
    }
    return program;
}

clopts_program_t *test_decl_program(const char *name)
{
    return test_decl_finish(clopts_program_create(name));
}

clopts_command_t *test_decl_help_command(clopts_program_t *program)
{
    clopts_command_t *command = clopts_program_add_command(program, "help");
    clopts_positional_t *positional = clopts_command_add_positional(command, "COMMAND");

    clopts_command_set_summary(command, "Show help for a command");
    clopts_positional_set_help(positional, "Command to show help for");
    return command;
}
