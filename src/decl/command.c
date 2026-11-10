/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Builder of subcommands (clopts/decl/command.h): creation and
 * release, texts, the attach of options and positionals, and the read
 * functions. The options and positionals live in the scope of the command,
 * which makes the checks against the other options and positionals of the
 * same command (scope.c).
 *
 * A command does not know its program: every check made here reads the
 * command alone. The checks between commands are made when a command is
 * added to the program (program.c).
 *
 * Every builder function checks its arguments and the declaration rules that
 * belong to it, in a fixed order, and returns the first rule that fails.
 * Nothing is changed before every check has passed and every allocation has
 * succeeded, so a failing call leaves every handle as it was. Name rules
 * live in name.c, the comparisons of names in scope.c.
 */

#include "clopts/decl/command.h"

#include "base/alloc.h"
#include "base/str.h"
#include "decl/name.h"
#include "decl/scope_impl.h"

#include <assert.h>

/*
 * A subcommand. A command does not point to the program it
 * is attached to. Attached, it is owned by that program and read-only.
 * Everything outside this file reads it through the public read functions,
 * or its scope through clopts_command_get_scope().
 */
struct clopts_command
{
    char *name;           /* owned copy */
    char *summary;        /* may be NULL; owned copy */
    char *description;    /* may be NULL; owned copy */
    clopts_scope_t scope; /* the options and positionals of the command */
};

clopts_error_t clopts_command_create(const char *name, clopts_command_t **out)
{
    clopts_command_t *command;

    assert(out != NULL);

    *out = NULL;

    if (!clopts_name_valid_command(name))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }

    command = (clopts_command_t *)clopts_calloc(1, sizeof(*command));
    if (command == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    command->name = clopts_strdup(name);
    if (command->name == NULL)
    {
        clopts_free(command);
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    command->summary = NULL;
    command->description = NULL;
    clopts_scope_init(&command->scope);

    *out = command;

    return CLOPTS_ERROR_NONE;
}

void clopts_command_destroy(clopts_command_t *command)
{
    if (command == NULL)
    {
        return;
    }

    clopts_scope_release(&command->scope);
    clopts_free(command->name);
    clopts_free(command->summary);
    clopts_free(command->description);
    clopts_free(command);
}

clopts_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    assert(command != NULL);
    return (clopts_strrep(&command->summary, summary) == 0) ? CLOPTS_ERROR_NONE
                                                                         : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_command_set_description(clopts_command_t *command, const char *description)
{
    assert(command != NULL);
    return (clopts_strrep(&command->description, description) == 0) ? CLOPTS_ERROR_NONE
                                                                                 : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option)
{
    assert(command != NULL);
    assert(option != NULL);
    return clopts_scope_add_option(&command->scope, option);
}

clopts_error_t clopts_command_add_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    assert(command != NULL);
    assert(positional != NULL);
    return clopts_scope_add_positional(&command->scope, positional);
}

/* ---- read functions: a NULL command is a contract violation ---- */

const clopts_scope_t *clopts_command_get_scope(const clopts_command_t *command)
{
    assert(command != NULL);
    return &command->scope;
}

const char *clopts_command_get_name(const clopts_command_t *command)
{
    assert(command != NULL);
    return command->name;
}

const char *clopts_command_get_summary(const clopts_command_t *command)
{
    assert(command != NULL);
    return command->summary;
}

const char *clopts_command_get_description(const clopts_command_t *command)
{
    assert(command != NULL);
    return command->description;
}

