/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Scope: the options and positionals of the program or of a command, in
 * order of attachment, with the checks of an attach against the other
 * options and positionals of the same scope. The program (program.c)
 * and every command (command.c) embed one; nothing else reads a scope but
 * through the functions here.
 *
 * Every attach checks its rules in a fixed order and returns the first rule
 * that fails. Nothing is changed before every check has passed and every
 * allocation has succeeded, so a failing call leaves the scope and the
 * handle as they were.
 */

#include "decl/decl_internal.h"

#include <assert.h>
#include <string.h>

/* Option index of scope; requires index < the option count. */
static clopts_option_t *scope_option(const clopts_scope_t *scope, size_t index)
{
    return *(clopts_option_t **)clopts_core_vec_at(&scope->options, index);
}

/* Positional index of scope; requires index < the positional count. */
static clopts_positional_t *scope_positional(const clopts_scope_t *scope, size_t index)
{
    return *(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, index);
}

void clopts_scope_init(clopts_scope_t *scope)
{
    assert(scope != NULL);
    clopts_core_vec_init(&scope->options, sizeof(clopts_option_t *));
    clopts_core_vec_init(&scope->positionals, sizeof(clopts_positional_t *));
}

void clopts_scope_release(clopts_scope_t *scope)
{
    size_t i;

    assert(scope != NULL);
    for (i = 0; i < scope->options.len; i++)
    {
        clopts_option_destroy(scope_option(scope, i));
    }
    clopts_core_vec_free(&scope->options);

    for (i = 0; i < scope->positionals.len; i++)
    {
        clopts_positional_destroy(scope_positional(scope, i));
    }
    clopts_core_vec_free(&scope->positionals);
}

/*
 * True when scope has a negatable FLAG whose long name is the name_len
 * bytes at name.
 */
static bool scope_has_negatable(const clopts_scope_t *scope, const char *name, size_t name_len)
{
    const clopts_option_t *found = clopts_decl_find_named(scope, name, name_len);

    return ((found != NULL) && (clopts_option_get_kind(found) == CLOPTS_OPTION_FLAG) &&
            clopts_option_is_negatable(found));
}

/* True when scope has an option whose long name is "no-" followed by long_name. */
static bool scope_has_negation_name(const clopts_scope_t *scope, const char *long_name)
{
    size_t i;

    for (i = 0; i < scope->options.len; i++)
    {
        const char *name = clopts_option_get_long_name(scope_option(scope, i));

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
    const char *l = clopts_option_get_long_name(option);

    if (clopts_decl_find_conflict(scope, l, clopts_option_get_short_name(option)) != NULL)
    {
        return CLOPTS_ERROR_DUPLICATE_OPTION;
    }

    if ((l != NULL) && (strncmp(l, "no-", 3) == 0) && scope_has_negatable(scope, l + 3, strlen(l) - 3))
    {
        return CLOPTS_ERROR_NEGATION_CONFLICT;
    }

    if (clopts_option_is_negatable(option) && (l != NULL) && scope_has_negation_name(scope, l))
    {
        return CLOPTS_ERROR_NEGATION_CONFLICT;
    }

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_scope_add_option(clopts_scope_t *scope, clopts_option_t *option)
{
    clopts_error_t error;

    assert(scope != NULL);
    assert(option != NULL);

    error = scope_check_option(scope, option);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }
    return (clopts_core_vec_push(&scope->options, &option) == 0) ? CLOPTS_ERROR_NONE : CLOPTS_ERROR_OUT_OF_MEMORY;
}

/* Checks of clopts_scope_add_positional(scope, positional) against scope, in order. */
static clopts_error_t scope_check_positional(const clopts_scope_t *scope, const clopts_positional_t *positional)
{
    const clopts_positional_t *last;

    if (scope->positionals.len == 0)
    {
        return CLOPTS_ERROR_NONE;
    }

    last = scope_positional(scope, scope->positionals.len - 1);
    if (clopts_positional_is_variadic(last))
    {
        return CLOPTS_ERROR_VARIADIC_POSITION;
    }

    /* The attached ones are in order, so checking the last one is enough. */
    if (clopts_positional_is_required(positional) && !clopts_positional_is_required(last))
    {
        return CLOPTS_ERROR_POSITIONAL_ORDER;
    }

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_scope_add_positional(clopts_scope_t *scope, clopts_positional_t *positional)
{
    clopts_error_t error;

    assert(scope != NULL);
    assert(positional != NULL);

    error = scope_check_positional(scope, positional);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }
    return (clopts_core_vec_push(&scope->positionals, &positional) == 0) ? CLOPTS_ERROR_NONE
                                                                          : CLOPTS_ERROR_OUT_OF_MEMORY;
}

/* ---- read functions ---- */

size_t clopts_scope_option_count(const clopts_scope_t *scope)
{
    assert(scope != NULL);
    return scope->options.len;
}

const clopts_option_t *clopts_scope_option_at(const clopts_scope_t *scope, size_t index)
{
    assert(scope != NULL);
    assert((index < scope->options.len) && "index must be less than the option count");
    if (index >= scope->options.len)
    {
        return NULL;
    }
    return scope_option(scope, index);
}

size_t clopts_scope_positional_count(const clopts_scope_t *scope)
{
    assert(scope != NULL);
    return scope->positionals.len;
}

const clopts_positional_t *clopts_scope_positional_at(const clopts_scope_t *scope, size_t index)
{
    assert(scope != NULL);
    assert((index < scope->positionals.len) && "index must be less than the positional count");
    if (index >= scope->positionals.len)
    {
        return NULL;
    }
    return scope_positional(scope, index);
}
