/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Positional (clopts/decl/positional.h): creation, release and attributes.
 * Nothing here reads or writes a command or a program; the order of the
 * positionals is checked when one is attached (command.c).
 */

#include "clopts/decl/positional.h"

#include "base/alloc.h"
#include "base/str.h"

#include <assert.h>
#include <stdbool.h>

/*
 * Internal representation of a positional. Only this file knows its layout;
 * everything else reads it through the public read functions. Like an option,
 * it does not point to the command it is attached to.
 */
struct clopts_positional
{
    char *name;                 /* not empty; owned copy */
    char *help;                 /* may be NULL; owned copy */
    bool required;
    bool variadic;              /* true for the last positional of a command only */
};

clopts_error_t clopts_positional_create(const char *name, clopts_positional_t **out)
{
    clopts_positional_t *positional;

    assert(out != NULL);

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

clopts_error_t clopts_positional_set_help(clopts_positional_t *positional, const char *help)
{
    assert(positional != NULL);
    return (clopts_strrep(&positional->help, help) == 0) ? CLOPTS_ERROR_NONE
                                                                      : CLOPTS_ERROR_OUT_OF_MEMORY;
}

void clopts_positional_set_required(clopts_positional_t *positional, bool required)
{
    assert(positional != NULL);
    positional->required = required;
}

void clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic)
{
    assert(positional != NULL);
    positional->variadic = variadic;
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
