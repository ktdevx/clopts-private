/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Positional (clopts/decl/positional.h): creation, release and attributes.
 * A positional knows the scope it is added to (scope.c): its setters report
 * their failure there, do nothing once the program has an error, and ask the
 * scope whether the positionals stay in order. A setter given a NULL
 * positional does nothing; a read function given one is a contract violation
 * (assert).
 */

#include "decl/positional_internal.h"

#include "base/alloc_internal.h"
#include "base/str.h"
#include "decl/scope_internal.h"

#include <assert.h>
#include <stdbool.h>

/*
 * Internal representation of a positional. Only this file knows its layout;
 * everything else reads it through the public read functions. It is owned by
 * the scope it was added to.
 */
struct clopts_positional
{
    clopts_scope_t *scope;      /* the scope that holds it */
    char *name;                 /* not empty; owned copy */
    char *help;                 /* may be NULL; owned copy */
    bool required;
    bool variadic;              /* true for the last positional of a command only */
};

clopts_error_t clopts_positional_create(const char *name, clopts_scope_t *scope, clopts_positional_t **out)
{
    clopts_positional_t *positional;

    assert((scope != NULL) && (out != NULL));

    *out = NULL;

    if ((name == NULL) || (name[0] == '\0'))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }

    positional = (clopts_positional_t *)clopts_calloc(1, sizeof(*positional));
    if (positional == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    positional->name = clopts_strdup(name);
    if (positional->name == NULL)
    {
        clopts_free(positional);
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    positional->scope = scope;
    positional->help = NULL;
    positional->required = false;
    positional->variadic = false;

    *out = positional;

    return CLOPTS_ERROR_NONE;
}

void clopts_positional_destroy(clopts_positional_t *positional)
{
    if (positional == NULL)
    {
        return;
    }

    clopts_free(positional->name);
    clopts_free(positional->help);
    clopts_free(positional);
}

/* True when a setter given positional has nothing to do: positional is NULL, or its program has an error. */
static bool positional_skip(const clopts_positional_t *positional)
{
    return (positional == NULL) || clopts_scope_failed(positional->scope);
}

void clopts_positional_set_help(clopts_positional_t *positional, const char *help)
{
    if (positional_skip(positional))
    {
        return;
    }
    if (clopts_strrep(&positional->help, help) != 0)
    {
        clopts_scope_fail(positional->scope, CLOPTS_ERROR_OUT_OF_MEMORY);
    }
}

/* Gives positional the attributes required and variadic when the positionals stay in order. */
static void positional_set_order(clopts_positional_t *positional, bool required, bool variadic)
{
    clopts_error_t error;

    if (positional_skip(positional))
    {
        return;
    }
    error = clopts_scope_check_positional(positional->scope, positional, required, variadic);
    if (error != CLOPTS_ERROR_NONE)
    {
        clopts_scope_fail(positional->scope, error);
        return;
    }
    positional->required = required;
    positional->variadic = variadic;
}

void clopts_positional_set_required(clopts_positional_t *positional, bool required)
{
    if (positional != NULL)
    {
        positional_set_order(positional, required, positional->variadic);
    }
}

void clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic)
{
    if (positional != NULL)
    {
        positional_set_order(positional, positional->required, variadic);
    }
}

/* ---- read functions ---- */

const char *clopts_positional_get_name(const clopts_positional_t *positional)
{
    assert(positional != NULL);
    return positional->name;
}

const char *clopts_positional_get_help(const clopts_positional_t *positional)
{
    assert(positional != NULL);
    return positional->help;
}

bool clopts_positional_is_required(const clopts_positional_t *positional)
{
    assert(positional != NULL);
    return positional->required;
}

bool clopts_positional_is_variadic(const clopts_positional_t *positional)
{
    assert(positional != NULL);
    return positional->variadic;
}
