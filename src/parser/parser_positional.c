/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Positional (clopts/positional.h): creation, release and attributes.
 * Nothing here reads or writes a command or a program; the order of the
 * positionals is checked when one is attached (command.c).
 */

#include "clopts/positional.h"
#include "parser/parser_internal.h"

clopts_decl_error_t clopts_positional_create(const char *name, clopts_positional_t **out)
{
    clopts_positional_t *positional;

    if (out == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    *out = NULL;
    if ((name == NULL) || (name[0] == '\0'))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }

    positional = (clopts_positional_t *)clopts_core_calloc(1, sizeof(*positional));
    if (positional == NULL)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    positional->attached = false;
    positional->name = clopts_core_strdup(name);
    if (positional->name == NULL)
    {
        clopts_core_free(positional);
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    positional->help = NULL;
    positional->required = false;
    positional->variadic = false;
    *out = positional;
    return CLOPTS_DECL_NONE;
}

void clopts_positional_destroy(clopts_positional_t *positional)
{
    if (positional == NULL)
    {
        return;
    }
    clopts_core_free(positional->name);
    clopts_core_free(positional->help);
    clopts_core_free(positional);
}

/* Common checks of a positional setter. */
static clopts_decl_error_t positional_check_call(const clopts_positional_t *positional)
{
    if (positional == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    return positional->attached ? CLOPTS_DECL_ALREADY_ATTACHED : CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_positional_set_help(clopts_positional_t *positional, const char *help)
{
    clopts_decl_error_t error = positional_check_call(positional);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return (clopts_core_replace_string(&positional->help, help) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_positional_set_required(clopts_positional_t *positional, bool required)
{
    clopts_decl_error_t error = positional_check_call(positional);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    positional->required = required;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic)
{
    clopts_decl_error_t error = positional_check_call(positional);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    positional->variadic = variadic;
    return CLOPTS_DECL_NONE;
}

/* ---- read functions ---- */

const char *clopts_positional_name(const clopts_positional_t *positional)
{
    return (positional != NULL) ? positional->name : NULL;
}

const char *clopts_positional_help(const clopts_positional_t *positional)
{
    return (positional != NULL) ? positional->help : NULL;
}

bool clopts_positional_required(const clopts_positional_t *positional)
{
    return (positional != NULL) && positional->required;
}

bool clopts_positional_variadic(const clopts_positional_t *positional)
{
    return (positional != NULL) && positional->variadic;
}
