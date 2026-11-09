/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Builder of the program (clopts/program.h): creation and release, the
 * description, the attach of global options, positionals and subcommands,
 * and the read functions.
 *
 * The program is the scope of the program itself (scope.c holds its options
 * and positionals and their checks) plus the subcommands. This file makes the
 * only checks between commands: the names of the subcommands and the layout
 * (positionals of the program or subcommands, not both). Every builder
 * function checks its rules in a fixed order and returns the first rule that
 * fails. Nothing is changed before every check has passed and every
 * allocation has succeeded, so a failing call leaves every handle as it was.
 */

#include "clopts/program.h"
#include "decl/decl_internal.h"

#include <assert.h>
#include <string.h>

/*
 * A program: its name and description, its own scope (the global options and
 * the positionals of a program without subcommands) and the subcommands, in
 * the order they were added. Everything outside this file reads it through
 * the public read functions, or its scope through clopts_program_scope().
 */
struct clopts_program
{
    char *name;              /* owned copy */
    char *description;       /* may be NULL; owned copy */
    clopts_scope_t scope;    /* the global options and the positionals of the program */
    clopts_core_vec_t commands;  /* clopts_command_t * elements, in the order they were added */
};

/* Subcommand index of program; requires index < the command count. */
static clopts_command_t *program_command(const clopts_program_t *program, size_t index)
{
    return *(clopts_command_t **)clopts_core_vec_at(&program->commands, index);
}

clopts_error_t clopts_program_create(const char *name, clopts_program_t **out)
{
    clopts_program_t *program;

    assert(out != NULL);
    *out = NULL;

    if (!clopts_decl_valid_command_name(name))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }

    program = (clopts_program_t *)clopts_core_calloc(1, sizeof(*program));
    if (program == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    program->name = clopts_core_strdup(name);
    if (program->name == NULL)
    {
        clopts_core_free(program);
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    program->description = NULL;
    clopts_scope_init(&program->scope);
    clopts_core_vec_init(&program->commands, sizeof(clopts_command_t *));

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
    for (i = 0; i < program->commands.len; i++)
    {
        clopts_command_destroy(program_command(program, i));
    }
    clopts_core_vec_free(&program->commands);
    clopts_scope_release(&program->scope);
    clopts_core_free(program->name);
    clopts_core_free(program->description);
    clopts_core_free(program);
}

clopts_error_t clopts_program_set_description(clopts_program_t *program, const char *description)
{
    assert(program != NULL);
    return (clopts_core_replace_string(&program->description, description) == 0) ? CLOPTS_ERROR_NONE
                                                                                 : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_program_add_option(clopts_program_t *program, clopts_option_t *option)
{
    assert(program != NULL);
    assert(option != NULL);
    return clopts_scope_add_option(&program->scope, option);
}

clopts_error_t clopts_program_add_positional(clopts_program_t *program, clopts_positional_t *positional)
{
    assert(program != NULL);
    assert(positional != NULL);

    if (program->commands.len > 0)
    {
        return CLOPTS_ERROR_MIXED_LAYOUT;
    }
    return clopts_scope_add_positional(&program->scope, positional);
}

clopts_error_t clopts_program_add_command(clopts_program_t *program, clopts_command_t *command)
{
    assert(program != NULL);
    assert(command != NULL);

    if (clopts_program_find_command(program, clopts_command_get_name(command)) != NULL)
    {
        return CLOPTS_ERROR_DUPLICATE_COMMAND;
    }
    if (clopts_scope_positional_count(&program->scope) > 0)
    {
        return CLOPTS_ERROR_MIXED_LAYOUT;
    }
    return (clopts_core_vec_push(&program->commands, &command) == 0) ? CLOPTS_ERROR_NONE
                                                                      : CLOPTS_ERROR_OUT_OF_MEMORY;
}

const clopts_scope_t *clopts_program_scope(const clopts_program_t *program)
{
    assert(program != NULL);
    return &program->scope;
}

/* ---- read functions: a NULL program is a contract violation ---- */

const char *clopts_program_get_name(const clopts_program_t *program)
{
    assert(program != NULL);
    return program->name;
}

const char *clopts_program_get_description(const clopts_program_t *program)
{
    assert(program != NULL);
    return program->description;
}

size_t clopts_program_get_option_count(const clopts_program_t *program)
{
    assert(program != NULL);
    return clopts_scope_option_count(&program->scope);
}

const clopts_option_t *clopts_program_get_option_at(const clopts_program_t *program, size_t index)
{
    assert(program != NULL);
    return clopts_scope_option_at(&program->scope, index);
}

size_t clopts_program_get_positional_count(const clopts_program_t *program)
{
    assert(program != NULL);
    return clopts_scope_positional_count(&program->scope);
}

const clopts_positional_t *clopts_program_get_positional_at(const clopts_program_t *program, size_t index)
{
    assert(program != NULL);
    return clopts_scope_positional_at(&program->scope, index);
}

size_t clopts_program_get_command_count(const clopts_program_t *program)
{
    assert(program != NULL);
    return program->commands.len;
}

const clopts_command_t *clopts_program_get_command_at(const clopts_program_t *program, size_t index)
{
    assert(program != NULL);
    assert((index < program->commands.len) && "index must be less than clopts_program_get_command_count()");
    if (index >= program->commands.len)
    {
        return NULL;
    }
    return program_command(program, index);
}

const clopts_command_t *clopts_program_find_command(const clopts_program_t *program, const char *name)
{
    size_t i;

    assert((program != NULL) && (name != NULL));
    for (i = 0; i < program->commands.len; i++)
    {
        if (strcmp(clopts_command_get_name(program_command(program, i)), name) == 0)
        {
            return program_command(program, i);
        }
    }
    return NULL;
}
