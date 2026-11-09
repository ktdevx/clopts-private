/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Builder of commands, the root included (clopts/command.h): creation and
 * release, texts, the attach of options and positionals with the checks
 * against the other options and positionals of the same command, and the
 * read functions.
 *
 * A command does not know its program: every check made here reads the
 * command alone. The checks between commands are made by
 * clopts_program_create() (parser_program.c).
 *
 * Every builder function checks its arguments and the declaration rules that
 * belong to it, in a fixed order, and returns the first rule that fails.
 * Nothing is changed before every check has passed and every allocation has
 * succeeded, so a failing call leaves every handle as it was. Name rules and
 * lookups live in parser_decl.c.
 */

#include "clopts/command.h"
#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

clopts_decl_error_t clopts_command_create(const char *name, clopts_command_t **out)
{
    clopts_command_t *command;

    if (out == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }

    *out = NULL;

    if (!clopts_parser_valid_command_name(name))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }

    command = (clopts_command_t *)clopts_core_calloc(1, sizeof(*command));
    if (command == NULL)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }

    command->name = clopts_core_strdup(name);
    if (command->name == NULL)
    {
        clopts_core_free(command);
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }

    command->summary = NULL;
    command->description = NULL;
    clopts_core_vec_init(&command->options, sizeof(clopts_option_t *));
    clopts_core_vec_init(&command->positionals, sizeof(clopts_positional_t *));

    *out = command;

    return CLOPTS_DECL_NONE;
}

void clopts_command_destroy(clopts_command_t *command)
{
    size_t i;

    if (command == NULL)
    {
        return;
    }

    for (i = 0; i < command->options.len; i++)
    {
        clopts_option_destroy(*(clopts_option_t **)clopts_core_vec_at(&command->options, i));
    }
    clopts_core_vec_free(&command->options);

    for (i = 0; i < command->positionals.len; i++)
    {
        clopts_positional_destroy(*(clopts_positional_t **)clopts_core_vec_at(&command->positionals, i));
    }
    clopts_core_vec_free(&command->positionals);

    clopts_core_free(command->name);
    clopts_core_free(command->summary);
    clopts_core_free(command->description);
    clopts_core_free(command);
}

/* Common checks of a call that changes command. */
static clopts_decl_error_t command_check_call(const clopts_command_t *command)
{
    return (command == NULL) ? CLOPTS_DECL_INVALID_ARGUMENT : CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    clopts_decl_error_t error = command_check_call(command);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    return (clopts_core_replace_string(&command->summary, summary) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_command_set_description(clopts_command_t *command, const char *description)
{
    clopts_decl_error_t error = command_check_call(command);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    return (clopts_core_replace_string(&command->description, description) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

/*
 * True when command has a negatable FLAG whose long name is the name_len
 * bytes at name.
 */
static bool command_has_negatable(const clopts_command_t *command, const char *name, size_t name_len)
{
    const clopts_option_t *found = clopts_parser_find_named(command, name, name_len);

    return ((found != NULL) && (found->kind == CLOPTS_OPTION_FLAG) && found->negatable);
}

/*
 * True when command has an option whose long name is "no-" followed by
 * long_name. Returns -1 when the name cannot be built.
 */
static int command_has_negation_name(const clopts_command_t *command, const char *long_name)
{
    size_t len = strlen(long_name);
    size_t size;
    char *name;
    int found;

    if (clopts_core_size_add(len, 4, &size) != 0)
    {
        return -1;
    }

    name = (char *)clopts_core_malloc(size);
    if (name == NULL)
    {
        return -1;
    }

    memcpy(name, "no-", 3);
    memcpy(name + 3, long_name, len + 1);
    found = (clopts_parser_find_named(command, name, len + 3) != NULL) ? 1 : 0;
    clopts_core_free(name);

    return found;
}

/* Checks of clopts_command_add_option(command, option) against command, in order. */
static clopts_decl_error_t command_check_attach_option(const clopts_command_t *command, const clopts_option_t *option)
{
    const char *l = option->long_name;
    int conflict;

    if (clopts_parser_find_conflict(command, l, option->short_name) != NULL)
    {
        return CLOPTS_DECL_DUPLICATE_OPTION;
    }

    if ((l != NULL) && (strncmp(l, "no-", 3) == 0) && command_has_negatable(command, l + 3, strlen(l) - 3))
    {
        return CLOPTS_DECL_NEGATION_CONFLICT;
    }

    if (option->negatable && (l != NULL))
    {
        conflict = command_has_negation_name(command, l);
        if (conflict < 0)
        {
            return CLOPTS_DECL_OUT_OF_MEMORY;
        }
        else if (conflict > 0)
        {
            return CLOPTS_DECL_NEGATION_CONFLICT;
        }
    }

    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option)
{
    clopts_decl_error_t error;
    size_t index;

    if ((command == NULL) || (option == NULL))
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }

    error = command_check_attach_option(command, option);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    index = command->options.len;

    if (clopts_core_vec_push(&command->options, &option) != 0)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }

    option->index = index;

    return CLOPTS_DECL_NONE;
}

/* Checks of clopts_command_add_positional(command, positional) against command, in order. */
static clopts_decl_error_t command_check_attach_positional(const clopts_command_t *command,
                                                         const clopts_positional_t *positional)
{
    const clopts_positional_t *last;

    if (command->positionals.len == 0)
    {
        return CLOPTS_DECL_NONE;
    }

    last = *(clopts_positional_t *const *)clopts_core_vec_at(&command->positionals, command->positionals.len - 1);
    if (last->variadic)
    {
        return CLOPTS_DECL_VARIADIC_POSITION;
    }

    /* The attached ones are in order, so checking the last one is enough. */
    if (positional->required && !last->required)
    {
        return CLOPTS_DECL_POSITIONAL_ORDER;
    }

    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_command_add_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    clopts_decl_error_t error;

    if ((command == NULL) || (positional == NULL))
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }

    error = command_check_attach_positional(command, positional);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    if (clopts_core_vec_push(&command->positionals, &positional) != 0)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }

    return CLOPTS_DECL_NONE;
}

/* ---- read functions ---- */

const char *clopts_command_name(const clopts_command_t *command)
{
    return (command != NULL) ? command->name : NULL;
}

const char *clopts_command_summary(const clopts_command_t *command)
{
    return (command != NULL) ? command->summary : NULL;
}

const char *clopts_command_description(const clopts_command_t *command)
{
    return (command != NULL) ? command->description : NULL;
}

size_t clopts_command_option_count(const clopts_command_t *command)
{
    return (command != NULL) ? command->options.len : 0u;
}

const clopts_option_t *clopts_command_option_at(const clopts_command_t *command, size_t index)
{
    size_t count = clopts_command_option_count(command);

    assert((index < count) && "index must be less than clopts_command_option_count()");
    if (index >= count)
    {
        return NULL;
    }

    return *(const clopts_option_t *const *)clopts_core_vec_at(&command->options, index);
}

size_t clopts_command_positional_count(const clopts_command_t *command)
{
    return (command != NULL) ? command->positionals.len : 0u;
}

const clopts_positional_t *clopts_command_positional_at(const clopts_command_t *command, size_t index)
{
    size_t count = clopts_command_positional_count(command);

    assert((index < count) && "index must be less than clopts_command_positional_count()");
    if (index >= count)
    {
        return NULL;
    }

    return *(const clopts_positional_t *const *)clopts_core_vec_at(&command->positionals, index);
}
