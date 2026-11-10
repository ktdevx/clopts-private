/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Subcommands (clopts/decl/command.h): creation and release (for the
 * program, program.c), texts, the options and positionals added to them,
 * and the read functions. The options and positionals live in the scope of
 * the command, which makes the checks against the other options and
 * positionals of the same command (scope.c) and reports failures to the
 * error of the program.
 *
 * The checks between commands are made when a command is added to the
 * program (program.c). A builder function given a NULL command, or called
 * once the program has an error, does nothing. Name rules live in name.c,
 * the comparisons of names in scope.c.
 */

#include "decl/command_internal.h"

#include "base/alloc_internal.h"
#include "base/str.h"
#include "decl/name.h"
#include "decl/scope_internal.h"

#include <assert.h>

/*
 * A subcommand, owned by its program. Everything outside this file reads it
 * through the public read functions, or its scope through
 * clopts_command_get_scope().
 */
struct clopts_command
{
    char *name;           /* owned copy */
    char *summary;        /* may be NULL; owned copy */
    char *description;    /* may be NULL; owned copy */
    clopts_scope_t scope; /* the options and positionals of the command */
};

clopts_error_t clopts_command_create(const char *name, clopts_error_t *error, clopts_command_t **out)
{
    clopts_command_t *command;

    assert((error != NULL) && (out != NULL));
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
    clopts_scope_init(&command->scope, error);

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

/* Replaces the text *slot of command with a copy of text, unless there is nothing to do. */
static void command_set_text(clopts_command_t *command, char **slot, const char *text)
{
    if (clopts_scope_failed(&command->scope))
    {
        return;
    }
    if (clopts_strrep(slot, text) != 0)
    {
        clopts_scope_fail(&command->scope, CLOPTS_ERROR_OUT_OF_MEMORY);
    }
}

void clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    if (command != NULL)
    {
        command_set_text(command, &command->summary, summary);
    }
}

void clopts_command_set_description(clopts_command_t *command, const char *description)
{
    if (command != NULL)
    {
        command_set_text(command, &command->description, description);
    }
}

clopts_option_t *clopts_command_add_option(clopts_command_t *command, clopts_option_kind_t kind,
                                           const char *long_name, char short_name)
{
    return (command != NULL) ? clopts_scope_add_option(&command->scope, kind, long_name, short_name) : NULL;
}

clopts_positional_t *clopts_command_add_positional(clopts_command_t *command, const char *name)
{
    return (command != NULL) ? clopts_scope_add_positional(&command->scope, name) : NULL;
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
