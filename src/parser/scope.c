/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Scope: the options and positionals of the program or of a command, in
 * order of attachment, with the checks of an attach against the other
 * options and positionals of the same scope. The program (parser_program.c)
 * and every command (command.c) embed one; nothing else reads a scope but
 * through the functions here.
 *
 * Every attach checks its rules in a fixed order and returns the first rule
 * that fails. Nothing is changed before every check has passed and every
 * allocation has succeeded, so a failing call leaves the scope and the
 * handle as they were.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

void clopts_scope_init(clopts_scope_t *scope)
{
    assert(scope != NULL);
    scope->options = NULL;
    scope->option_count = 0;
    scope->option_cap = 0;
    scope->positionals = NULL;
    scope->positional_count = 0;
    scope->positional_cap = 0;
}

void clopts_scope_release(clopts_scope_t *scope)
{
    size_t i;

    assert(scope != NULL);
    for (i = 0; i < scope->option_count; i++)
    {
        clopts_option_destroy(scope->options[i]);
    }
    clopts_core_free(scope->options);

    for (i = 0; i < scope->positional_count; i++)
    {
        clopts_positional_destroy(scope->positionals[i]);
    }
    clopts_core_free(scope->positionals);
    clopts_scope_init(scope);
}

/* Capacity of the first allocation of an attach array, in elements. */
#define SCOPE_INITIAL_CAP ((size_t)8)

/*
 * Makes room for one more element in the attach array items holding count
 * of cap elements of elem_size bytes: grows the capacity to 8, then doubles
 * it, when full. Returns the array to use (items itself when not full) and
 * updates *cap, or NULL on overflow or allocation failure, leaving items
 * and *cap unchanged.
 */
static void *scope_reserve(void *items, size_t count, size_t *cap, size_t elem_size)
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
        new_cap = SCOPE_INITIAL_CAP;
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
 * True when scope has a negatable FLAG whose long name is the name_len
 * bytes at name.
 */
static bool scope_has_negatable(const clopts_scope_t *scope, const char *name, size_t name_len)
{
    const clopts_option_t *found = clopts_parser_find_named(scope, name, name_len);

    return ((found != NULL) && (clopts_option_kind_of(found) == CLOPTS_OPTION_FLAG) && clopts_option_negatable(found));
}

/* True when scope has an option whose long name is "no-" followed by long_name. */
static bool scope_has_negation_name(const clopts_scope_t *scope, const char *long_name)
{
    size_t i;

    for (i = 0; i < scope->option_count; i++)
    {
        const char *name = clopts_option_long_name(scope->options[i]);

        if ((name != NULL) && (strncmp(name, "no-", 3) == 0) && (strcmp(name + 3, long_name) == 0))
        {
            return true;
        }
    }

    return false;
}

/* Checks of clopts_scope_add_option(scope, option) against scope, in order. */
static clopts_error_t scope_check_option(const clopts_scope_t *scope, const clopts_option_t *option)
{
    const char *l = clopts_option_long_name(option);

    if (clopts_parser_find_conflict(scope, l, clopts_option_short_name(option)) != NULL)
    {
        return CLOPTS_ERROR_DUPLICATE_OPTION;
    }

    if ((l != NULL) && (strncmp(l, "no-", 3) == 0) && scope_has_negatable(scope, l + 3, strlen(l) - 3))
    {
        return CLOPTS_ERROR_NEGATION_CONFLICT;
    }

    if (clopts_option_negatable(option) && (l != NULL) && scope_has_negation_name(scope, l))
    {
        return CLOPTS_ERROR_NEGATION_CONFLICT;
    }

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_scope_add_option(clopts_scope_t *scope, clopts_option_t *option)
{
    clopts_option_t **options;
    clopts_error_t error;

    assert(scope != NULL);
    assert(option != NULL);

    error = scope_check_option(scope, option);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }

    options = (clopts_option_t **)scope_reserve(scope->options, scope->option_count, &scope->option_cap,
                                                sizeof(*options));
    if (options == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    scope->options = options;
    scope->options[scope->option_count++] = option;

    return CLOPTS_ERROR_NONE;
}

/* Checks of clopts_scope_add_positional(scope, positional) against scope, in order. */
static clopts_error_t scope_check_positional(const clopts_scope_t *scope, const clopts_positional_t *positional)
{
    const clopts_positional_t *last;

    if (scope->positional_count == 0)
    {
        return CLOPTS_ERROR_NONE;
    }

    last = scope->positionals[scope->positional_count - 1];
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

clopts_error_t clopts_scope_add_positional(clopts_scope_t *scope, clopts_positional_t *positional)
{
    clopts_positional_t **positionals;
    clopts_error_t error;

    assert(scope != NULL);
    assert(positional != NULL);

    error = scope_check_positional(scope, positional);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }

    positionals = (clopts_positional_t **)scope_reserve(scope->positionals, scope->positional_count,
                                                        &scope->positional_cap, sizeof(*positionals));
    if (positionals == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    scope->positionals = positionals;
    scope->positionals[scope->positional_count++] = positional;

    return CLOPTS_ERROR_NONE;
}

/* ---- read functions ---- */

size_t clopts_scope_option_count(const clopts_scope_t *scope)
{
    assert(scope != NULL);
    return scope->option_count;
}

const clopts_option_t *clopts_scope_option_at(const clopts_scope_t *scope, size_t index)
{
    assert(scope != NULL);
    assert((index < scope->option_count) && "index must be less than the option count");
    if (index >= scope->option_count)
    {
        return NULL;
    }
    return scope->options[index];
}

size_t clopts_scope_positional_count(const clopts_scope_t *scope)
{
    assert(scope != NULL);
    return scope->positional_count;
}

const clopts_positional_t *clopts_scope_positional_at(const clopts_scope_t *scope, size_t index)
{
    assert(scope != NULL);
    assert((index < scope->positional_count) && "index must be less than the positional count");
    if (index >= scope->positional_count)
    {
        return NULL;
    }
    return scope->positionals[index];
}
