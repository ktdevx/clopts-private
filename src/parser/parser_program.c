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

/*
 * A program: the root and the subcommands, in the order given. It is created
 * in one call and never changes afterwards. Everything outside this file reads
 * it through the public read functions.
 */
struct clopts_program
{
    clopts_command_t *root;         /* the root scope; its name is the program name */
    clopts_command_t **commands;    /* command_count elements, in the order given; NULL when there are none */
    size_t command_count;
};

/* Checks of clopts_program_create(root, commands, count), in order. */
static clopts_error_t program_check_new(const clopts_command_t *root, clopts_command_t *const *commands,
                                             size_t count)
{
    size_t i;
    size_t j;

    assert(root != NULL);
    assert((count == 0) || (commands != NULL));
    for (i = 0; i < count; i++)
    {
        assert(commands[i] != NULL);
        for (j = 0; j < i; j++)
        {
            if (strcmp(clopts_command_name(commands[j]), clopts_command_name(commands[i])) == 0)
            {
                return CLOPTS_ERROR_DUPLICATE_COMMAND;
            }
        }
    }
    if ((count > 0) && (clopts_command_positional_count(root) > 0))
    {
        return CLOPTS_ERROR_MIXED_LAYOUT;
    }
    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_program_create(clopts_command_t *root, clopts_command_t *const *commands,
                                          size_t command_count, clopts_program_t **out)
{
    clopts_program_t *program;
    clopts_command_t **copy = NULL;
    clopts_error_t error;

    assert(out != NULL);
    *out = NULL;
    error = program_check_new(root, commands, command_count);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }

    program = (clopts_program_t *)clopts_core_calloc(1, sizeof(*program));
    if (program == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }
    if (command_count > 0)
    {
        copy = (clopts_command_t **)clopts_core_malloc_array(command_count, sizeof(*copy));
        if (copy == NULL)
        {
            clopts_core_free(program);
            return CLOPTS_ERROR_OUT_OF_MEMORY;
        }
        memcpy(copy, commands, command_count * sizeof(*copy));
    }

    /* Nothing can fail from here on: the program takes ownership. */
    program->root = root;
    program->commands = copy;
    program->command_count = command_count;
    *out = program;
    return CLOPTS_ERROR_NONE;
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
        clopts_command_destroy(program->commands[i]);
    }
    clopts_core_free(program->commands);
    clopts_command_destroy(program->root);
    clopts_core_free(program);
}

const clopts_scope_t *clopts_program_scope(const clopts_program_t *program)
{
    assert(program != NULL);
    return clopts_command_scope(program->root);
}

/* ---- read functions ---- */

const clopts_command_t *clopts_program_root(const clopts_program_t *program)
{
    assert(program != NULL);
    return program->root;
}

size_t clopts_program_command_count(const clopts_program_t *program)
{
    assert(program != NULL);
    return program->command_count;
}

const clopts_command_t *clopts_program_command_at(const clopts_program_t *program, size_t index)
{
    assert(program != NULL);
    assert((index < program->command_count) && "index must be less than clopts_program_command_count()");
    if (index >= program->command_count)
    {
        return NULL;
    }
    return program->commands[index];
}

const clopts_command_t *clopts_program_find_command(const clopts_program_t *program, const char *name)
{
    size_t i;

    assert((program != NULL) && (name != NULL));
    for (i = 0; i < program->command_count; i++)
    {
        if (strcmp(clopts_command_name(program->commands[i]), name) == 0)
        {
            return program->commands[i];
        }
    }
    return NULL;
}
