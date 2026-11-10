/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Builder of the program (clopts/decl/program.h): creation and release, the
 * declaration error, the description, the global options, positionals and
 * subcommands added to it, the help and version options, and the read
 * functions.
 *
 * The program is the scope of the program itself (scope.c holds its options
 * and positionals and their checks) plus the subcommands, and it holds the
 * declaration error that its scope and the scopes of its commands report to.
 * This file makes the only checks between commands: the names of the
 * subcommands and the layout (positionals of the program or subcommands,
 * not both). Every builder function checks its rules in a fixed order and
 * stops at the first rule that fails. Nothing is changed before every check
 * has passed and every allocation has succeeded, so a failing call leaves
 * every handle as it was.
 */

#include "clopts/decl/program.h"

#include "base/alloc_internal.h"
#include "base/str.h"
#include "base/vec.h"
#include "decl/command_internal.h"
#include "decl/name.h"
#include "decl/scope_internal.h"

#include <assert.h>
#include <string.h>

/*
 * A program: its first declaration error, its name and description, its own
 * scope (the global options and the positionals of a program without
 * subcommands) and the subcommands, in the order they were added. Everything
 * outside this file reads it through the public read functions.
 */
struct clopts_program
{
    clopts_error_t error;    /* the first declaration error; every scope of the program reports to it */
    char *name;              /* owned copy; NULL when the name is invalid (error is INVALID_NAME) */
    char *description;       /* may be NULL; owned copy */
    clopts_scope_t scope;    /* the global options and the positionals of the program */
    clopts_vec_t commands;  /* clopts_command_t * elements, in the order they were added */
};

/* Subcommand index of program; requires index < the command count. */
static clopts_command_t *program_command(const clopts_program_t *program, size_t index)
{
    return *(clopts_command_t **)clopts_vec_at(&program->commands, index);
}

clopts_program_t *clopts_program_create(const char *name)
{
    clopts_program_t *program;

    program = (clopts_program_t *)clopts_calloc(1, sizeof(*program));
    if (program == NULL)
    {
        return NULL;
    }

    program->error = CLOPTS_ERROR_NONE;
    program->name = NULL;
    program->description = NULL;
    clopts_scope_init(&program->scope, &program->error);
    clopts_vec_init(&program->commands, sizeof(clopts_command_t *));

    if (!clopts_name_valid_command(name))
    {
        program->error = CLOPTS_ERROR_INVALID_NAME;
        return program;
    }
    program->name = clopts_strdup(name);
    if (program->name == NULL)
    {
        clopts_free(program);
        return NULL;
    }
    return program;
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
    clopts_vec_release(&program->commands);
    clopts_scope_release(&program->scope);
    clopts_free(program->name);
    clopts_free(program->description);
    clopts_free(program);
}

clopts_error_t clopts_program_get_error(const clopts_program_t *program)
{
    return (program != NULL) ? program->error : CLOPTS_ERROR_OUT_OF_MEMORY;
}

/* True when a builder function given program has nothing to do: program is NULL, or it has an error. */
static bool program_skip(const clopts_program_t *program)
{
    return (program == NULL) || (program->error != CLOPTS_ERROR_NONE);
}

void clopts_program_set_description(clopts_program_t *program, const char *description)
{
    if (program_skip(program))
    {
        return;
    }
    if (clopts_strrep(&program->description, description) != 0)
    {
        clopts_scope_fail(&program->scope, CLOPTS_ERROR_OUT_OF_MEMORY);
    }
}

clopts_option_t *clopts_program_add_option(clopts_program_t *program, clopts_option_kind_t kind,
                                           const char *long_name, char short_name)
{
    return (program != NULL) ? clopts_scope_add_option(&program->scope, kind, long_name, short_name) : NULL;
}

clopts_positional_t *clopts_program_add_positional(clopts_program_t *program, const char *name)
{
    if (program_skip(program))
    {
        return NULL;
    }
    /* INVALID_NAME comes before CONFLICT, as the add checks the positional alone first. */
    if ((name != NULL) && (name[0] != '\0') && (program->commands.len > 0))
    {
        clopts_scope_fail(&program->scope, CLOPTS_ERROR_CONFLICT);
        return NULL;
    }
    return clopts_scope_add_positional(&program->scope, name);
}

/* Checks of clopts_program_add_command(program, name), in order, before the command is created. */
static clopts_error_t program_check_command(const clopts_program_t *program, const char *name)
{
    if (!clopts_name_valid_command(name))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }
    if (clopts_program_find_command(program, name) != NULL)
    {
        return CLOPTS_ERROR_DUPLICATE;
    }
    if (clopts_scope_get_positional_count(&program->scope) > 0)
    {
        return CLOPTS_ERROR_CONFLICT;
    }
    return CLOPTS_ERROR_NONE;
}

clopts_command_t *clopts_program_add_command(clopts_program_t *program, const char *name)
{
    clopts_command_t *command = NULL;
    clopts_error_t error;

    if (program_skip(program))
    {
        return NULL;
    }
    error = program_check_command(program, name);
    if (error == CLOPTS_ERROR_NONE)
    {
        error = clopts_command_create(name, &program->error, &command);
    }
    if ((error == CLOPTS_ERROR_NONE) && (clopts_vec_push(&program->commands, &command) != 0))
    {
        error = CLOPTS_ERROR_OUT_OF_MEMORY;
    }
    if (error != CLOPTS_ERROR_NONE)
    {
        clopts_command_destroy(command);
        clopts_scope_fail(&program->scope, error);
        return NULL;
    }
    return command;
}

/* Adds the terminal FLAG l / s with help to program. */
static clopts_option_t *program_add_terminal(clopts_program_t *program, const char *l, char s, const char *help)
{
    clopts_option_t *option = clopts_program_add_option(program, CLOPTS_OPTION_FLAG, l, s);

    clopts_option_set_terminal(option, true);
    clopts_option_set_help(option, help);
    /* A failed setter leaves the option in place, with the error in the program. */
    return (clopts_program_get_error(program) == CLOPTS_ERROR_NONE) ? option : NULL;
}

clopts_option_t *clopts_program_add_help_option(clopts_program_t *program)
{
    return program_add_terminal(program, "help", 'h', "Show this help");
}

clopts_option_t *clopts_program_add_version_option(clopts_program_t *program)
{
    return program_add_terminal(program, "version", '\0', "Show version information");
}

/* ---- read functions: a NULL program is a contract violation ---- */

const clopts_scope_t *clopts_program_get_scope(const clopts_program_t *program)
{
    assert(program != NULL);
    return &program->scope;
}

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
