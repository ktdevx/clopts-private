/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Builder of the program (clopts/program.h): creation from the root and the
 * subcommands, release, and the read functions.
 *
 * clopts_program_create() is the only place that checks rules between
 * commands: the names of the subcommands and the layout of the root. It
 * checks its arguments and these rules in a fixed order and returns the
 * first rule that fails. Nothing is changed before every check has passed
 * and every allocation has succeeded, so a failing call leaves every handle
 * as it was. After it the program never changes.
 */

#include "clopts/program.h"
#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/* Checks of clopts_program_create(root, commands, count), in order. */
static clopts_decl_error_t program_check_new(const clopts_command_t *root, clopts_command_t *const *commands,
                                             size_t count)
{
    size_t i;
    size_t j;

    if ((root == NULL) || ((count > 0) && (commands == NULL)))
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    for (i = 0; i < count; i++)
    {
        if (commands[i] == NULL)
        {
            return CLOPTS_DECL_INVALID_ARGUMENT;
        }
    }
    if (root->attached)
    {
        return CLOPTS_DECL_ALREADY_ATTACHED;
    }
    for (i = 0; i < count; i++)
    {
        if (commands[i]->attached || (commands[i] == root))
        {
            return CLOPTS_DECL_ALREADY_ATTACHED;
        }
        for (j = 0; j < i; j++)
        {
            if (commands[j] == commands[i])
            {
                return CLOPTS_DECL_ALREADY_ATTACHED;
            }
        }
    }
    for (i = 0; i < count; i++)
    {
        for (j = 0; j < i; j++)
        {
            if (strcmp(commands[j]->name, commands[i]->name) == 0)
            {
                return CLOPTS_DECL_DUPLICATE_COMMAND;
            }
        }
    }
    if ((count > 0) && (root->positionals.len > 0))
    {
        return CLOPTS_DECL_MIXED_LAYOUT;
    }
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_program_create(clopts_command_t *root, clopts_command_t *const *commands,
                                          size_t command_count, clopts_program_t **out)
{
    clopts_program_t *program;
    clopts_command_t **copy = NULL;
    clopts_decl_error_t error;
    size_t i;

    if (out == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    *out = NULL;
    error = program_check_new(root, commands, command_count);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    program = (clopts_program_t *)clopts_core_calloc(1, sizeof(*program));
    if (program == NULL)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    if (command_count > 0)
    {
        copy = (clopts_command_t **)clopts_core_malloc_array(command_count, sizeof(*copy));
        if (copy == NULL)
        {
            clopts_core_free(program);
            return CLOPTS_DECL_OUT_OF_MEMORY;
        }
        memcpy(copy, commands, command_count * sizeof(*copy));
    }

    /* Nothing can fail from here on: the program takes ownership. */
    root->attached = true;
    for (i = 0; i < command_count; i++)
    {
        copy[i]->attached = true;
    }
    program->root = root;
    program->commands = copy;
    program->command_count = command_count;
    *out = program;
    return CLOPTS_DECL_NONE;
}

void clopts_program_destroy(clopts_program_t *program)
{
    size_t i;

    if (program == NULL)
    {
        return;
    }
    for (i = 0; i < program->command_count; i++)
    {
        clopts_parser_command_release(program->commands[i]);
    }
    clopts_core_free(program->commands);
    clopts_parser_command_release(program->root);
    clopts_core_free(program);
}

/* ---- read functions ---- */

const clopts_command_t *clopts_program_root(const clopts_program_t *program)
{
    return (program != NULL) ? program->root : NULL;
}

size_t clopts_program_command_count(const clopts_program_t *program)
{
    return (program != NULL) ? program->command_count : 0u;
}

const clopts_command_t *clopts_program_command_at(const clopts_program_t *program, size_t index)
{
    size_t count = clopts_program_command_count(program);

    assert((index < count) && "index must be less than clopts_program_command_count()");
    if (index >= count)
    {
        return NULL;
    }
    return program->commands[index];
}

const clopts_command_t *clopts_program_find_command(const clopts_program_t *program, const char *name)
{
    if ((program == NULL) || (name == NULL))
    {
        return NULL;
    }
    return clopts_parser_find_command(program, name);
}
