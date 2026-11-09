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

/*
 * A command, the root included. A command does not point to the program it
 * is attached to. Attached, it is owned by that program and read-only.
 * Everything outside this file reads it through the public read functions.
 */
struct clopts_command
{
    char *name;                         /* owned copy */
    char *summary;                      /* may be NULL; owned copy */
    char *description;                  /* may be NULL; owned copy */
    clopts_option_t **options;          /* option_count in order of attachment; NULL when option_cap == 0 */
    size_t option_count;
    size_t option_cap;                  /* elements allocated */
    clopts_positional_t **positionals;  /* positional_count in order of attachment; NULL when positional_cap == 0 */
    size_t positional_count;
    size_t positional_cap;              /* elements allocated */
};

clopts_error_t clopts_command_create(const char *name, clopts_command_t **out)
{
    clopts_command_t *command;

    assert(out != NULL);

    *out = NULL;

    if (!clopts_parser_valid_command_name(name))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }

    command = (clopts_command_t *)clopts_core_calloc(1, sizeof(*command));
    if (command == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    command->name = clopts_core_strdup(name);
    if (command->name == NULL)
    {
        clopts_core_free(command);
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    command->summary = NULL;
    command->description = NULL;
    command->options = NULL;
    command->option_count = 0;
    command->option_cap = 0;
    command->positionals = NULL;
    command->positional_count = 0;
    command->positional_cap = 0;

    *out = command;

    return CLOPTS_ERROR_NONE;
}

void clopts_command_destroy(clopts_command_t *command)
{
    size_t i;

    if (command == NULL)
    {
        return;
    }

    for (i = 0; i < command->option_count; i++)
    {
        clopts_option_destroy(command->options[i]);
    }
    clopts_core_free(command->options);

    for (i = 0; i < command->positional_count; i++)
    {
        clopts_positional_destroy(command->positionals[i]);
    }
    clopts_core_free(command->positionals);

    clopts_core_free(command->name);
    clopts_core_free(command->summary);
    clopts_core_free(command->description);
    clopts_core_free(command);
}

clopts_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    assert(command != NULL);
    return (clopts_core_replace_string(&command->summary, summary) == 0) ? CLOPTS_ERROR_NONE : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_command_set_description(clopts_command_t *command, const char *description)
{
    assert(command != NULL);
    return (clopts_core_replace_string(&command->description, description) == 0) ? CLOPTS_ERROR_NONE : CLOPTS_ERROR_OUT_OF_MEMORY;
}

/* Capacity of the first allocation of an attach array, in elements. */
#define COMMAND_INITIAL_CAP ((size_t)8)

/*
 * Makes room for one more element in the attach array items holding count
 * of cap elements of elem_size bytes: grows the capacity to 8, then doubles
 * it, when full. Returns the array to use (items itself when not full) and
 * updates *cap, or NULL on overflow or allocation failure, leaving items
 * and *cap unchanged.
 */
static void *command_reserve(void *items, size_t count, size_t *cap, size_t elem_size)
{
    size_t new_cap;
    size_t new_bytes;
    void *grown;

    if (count < *cap)
    {
        return items;
    }

    if (*cap == 0)
    {
        new_cap = COMMAND_INITIAL_CAP;
    }
    else if (clopts_core_size_mul(*cap, 2, &new_cap) != 0)
    {
        return NULL;
    }

    if (clopts_core_size_mul(new_cap, elem_size, &new_bytes) != 0)
    {
        return NULL;
    }

    grown = clopts_core_realloc(items, new_bytes);
    if (grown != NULL)
    {
        *cap = new_cap;
    }

    return grown;
}

/*
 * True when command has a negatable FLAG whose long name is the name_len
 * bytes at name.
 */
static bool command_has_negatable(const clopts_command_t *command, const char *name, size_t name_len)
{
    const clopts_option_t *found = clopts_parser_find_named(command, name, name_len);

    return ((found != NULL) && (clopts_option_kind_of(found) == CLOPTS_OPTION_FLAG) && clopts_option_negatable(found));
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
static clopts_error_t command_check_attach_option(const clopts_command_t *command, const clopts_option_t *option)
{
    const char *l = clopts_option_long_name(option);
    int conflict;

    if (clopts_parser_find_conflict(command, l, clopts_option_short_name(option)) != NULL)
    {
        return CLOPTS_ERROR_DUPLICATE_OPTION;
    }

    if ((l != NULL) && (strncmp(l, "no-", 3) == 0) && command_has_negatable(command, l + 3, strlen(l) - 3))
    {
        return CLOPTS_ERROR_NEGATION_CONFLICT;
    }

    if (clopts_option_negatable(option) && (l != NULL))
    {
        conflict = command_has_negation_name(command, l);
        if (conflict < 0)
        {
            return CLOPTS_ERROR_OUT_OF_MEMORY;
        }
        else if (conflict > 0)
        {
            return CLOPTS_ERROR_NEGATION_CONFLICT;
        }
    }

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option)
{
    clopts_option_t **options;
    clopts_error_t error;

    assert(command != NULL);
    assert(option != NULL);

    error = command_check_attach_option(command, option);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }

    options = (clopts_option_t **)command_reserve(command->options, command->option_count, &command->option_cap,
                                                  sizeof(*options));
    if (options == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    command->options = options;
    command->options[command->option_count++] = option;

    return CLOPTS_ERROR_NONE;
}

/* Checks of clopts_command_add_positional(command, positional) against command, in order. */
static clopts_error_t command_check_attach_positional(const clopts_command_t *command,
                                                         const clopts_positional_t *positional)
{
    const clopts_positional_t *last;

    if (command->positional_count == 0)
    {
        return CLOPTS_ERROR_NONE;
    }

    last = command->positionals[command->positional_count - 1];
    if (clopts_positional_variadic(last))
    {
        return CLOPTS_ERROR_VARIADIC_POSITION;
    }

    /* The attached ones are in order, so checking the last one is enough. */
    if (clopts_positional_required(positional) && !clopts_positional_required(last))
    {
        return CLOPTS_ERROR_POSITIONAL_ORDER;
    }

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_command_add_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    clopts_positional_t **positionals;
    clopts_error_t error;

    assert(command != NULL);
    assert(positional != NULL);

    error = command_check_attach_positional(command, positional);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }

    positionals = (clopts_positional_t **)command_reserve(command->positionals, command->positional_count,
                                                          &command->positional_cap, sizeof(*positionals));
    if (positionals == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    command->positionals = positionals;
    command->positionals[command->positional_count++] = positional;

    return CLOPTS_ERROR_NONE;
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
    return (command != NULL) ? command->option_count : 0u;
}

const clopts_option_t *clopts_command_option_at(const clopts_command_t *command, size_t index)
{
    size_t count = clopts_command_option_count(command);

    assert((index < count) && "index must be less than clopts_command_option_count()");
    if (index >= count)
    {
        return NULL;
    }

    return command->options[index];
}

size_t clopts_command_positional_count(const clopts_command_t *command)
{
    return (command != NULL) ? command->positional_count : 0u;
}

const clopts_positional_t *clopts_command_positional_at(const clopts_command_t *command, size_t index)
{
    size_t count = clopts_command_positional_count(command);

    assert((index < count) && "index must be less than clopts_command_positional_count()");
    if (index >= count)
    {
        return NULL;
    }

    return command->positionals[index];
}
