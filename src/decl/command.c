/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Builder of subcommands (clopts/command.h): creation and
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
 * succeeded, so a failing call leaves every handle as it was. Name rules and
 * comparisons live in names.c.
 */

#include "clopts/command.h"
#include "decl/decl_internal.h"

#include <assert.h>

/*
 * A subcommand. A command does not point to the program it
 * is attached to. Attached, it is owned by that program and read-only.
 * Everything outside this file reads it through the public read functions,
 * or its scope through clopts_command_scope().
 */
struct clopts_command
{
    char *name;           /* owned copy */
    char *summary;        /* may be NULL; owned copy */
    char *description;    /* may be NULL; owned copy */
    clopts_scope_t scope; /* the options and positionals of the command */
};

clopts_decl_error_t clopts_command_create(const char *name, clopts_command_t **out)
{
    clopts_command_t *command;

    assert(out != NULL);

    *out = NULL;

    if (!clopts_decl_valid_command_name(name))
    {
        return CLOPTS_DECL_ERROR_INVALID_NAME;
    }

    command = (clopts_command_t *)clopts_core_calloc(1, sizeof(*command));
    if (command == NULL)
    {
        return CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
    }

    command->name = clopts_core_strdup(name);
    if (command->name == NULL)
    {
        clopts_core_free(command);
        return CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
    }

    command->summary = NULL;
    command->description = NULL;
    clopts_scope_init(&command->scope);

    *out = command;

    return CLOPTS_DECL_ERROR_NONE;
}

void clopts_command_destroy(clopts_command_t *command)
{
    if (command == NULL)
    {
        return;
    }

    clopts_scope_release(&command->scope);
    clopts_core_free(command->name);
    clopts_core_free(command->summary);
    clopts_core_free(command->description);
    clopts_core_free(command);
}

clopts_decl_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    assert(command != NULL);
    return (clopts_core_replace_string(&command->summary, summary) == 0) ? CLOPTS_DECL_ERROR_NONE
                                                                         : CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_command_set_description(clopts_command_t *command, const char *description)
{
    assert(command != NULL);
    return (clopts_core_replace_string(&command->description, description) == 0) ? CLOPTS_DECL_ERROR_NONE
                                                                                 : CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option)
{
    assert(command != NULL);
    assert(option != NULL);
    return clopts_scope_add_option(&command->scope, option);
}

clopts_decl_error_t clopts_command_add_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    assert(command != NULL);
    assert(positional != NULL);
    return clopts_scope_add_positional(&command->scope, positional);
}

const clopts_scope_t *clopts_command_scope(const clopts_command_t *command)
{
    assert(command != NULL);
    return &command->scope;
}

/* ---- read functions: a NULL command is a contract violation ---- */

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

size_t clopts_command_get_option_count(const clopts_command_t *command)
{
    assert(command != NULL);
    return clopts_scope_option_count(&command->scope);
}

const clopts_option_t *clopts_command_get_option_at(const clopts_command_t *command, size_t index)
{
    assert(command != NULL);
    return clopts_scope_option_at(&command->scope, index);
}

size_t clopts_command_get_positional_count(const clopts_command_t *command)
{
    assert(command != NULL);
    return clopts_scope_positional_count(&command->scope);
}

const clopts_positional_t *clopts_command_get_positional_at(const clopts_command_t *command, size_t index)
{
    assert(command != NULL);
    return clopts_scope_positional_at(&command->scope, index);
}
