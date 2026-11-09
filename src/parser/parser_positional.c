/*
 * Positional (clopts/positional.h): creation, release and attributes.
 * Nothing here reads or writes a command or a program; the order of the
 * positionals is checked when one is attached (parser_command.c).
 */

#include "clopts/positional.h"
#include "parser/parser_internal.h"

#include <assert.h>

void clopts_parser_positional_release(clopts_positional_t *positional)
{
    clopts_core_free(positional->name);
    clopts_core_free(positional->help);
    clopts_core_free(positional);
}

clopts_decl_error_t clopts_positional_new(const char *name, clopts_positional_t **out)
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

void clopts_positional_free(clopts_positional_t *positional)
{
    if (positional == NULL)
    {
        return;
    }
    assert(!positional->attached && "an attached positional is owned by its command");
    if (positional->attached)
    {
        return;
    }
    clopts_parser_positional_release(positional);
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
