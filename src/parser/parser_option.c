/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Option (clopts/option.h): creation, release and attributes. Everything
 * here depends on the option alone: nothing reads or writes a command or a
 * program. The checks against the other options of the same command are
 * made when the option is attached (command.c).
 *
 * Every function checks its arguments and the rules that belong to it, in a
 * fixed order, and returns the first rule that fails, changing nothing.
 * Resetting an attribute (NULL, false, '\0' or 0) is not checked.
 */

#include "clopts/option.h"
#include "parser/parser_internal.h"

#include <assert.h>

/* True for the kinds that take no value (FLAG / COUNT). */
static bool option_takes_no_value(clopts_option_kind_t kind)
{
    return ((kind == CLOPTS_OPTION_FLAG) || (kind == CLOPTS_OPTION_COUNT));
}

void clopts_parser_option_release(clopts_option_t *option)
{
    clopts_core_free(option->long_name);
    clopts_core_free(option->help);
    clopts_core_free(option->value_name);
    clopts_core_free(option->default_value);
    clopts_core_free(option->env);
    clopts_core_free(option);
}

/* True for the values of clopts_option_kind_t. */
static bool option_valid_kind(clopts_option_kind_t kind)
{
    return ((kind == CLOPTS_OPTION_FLAG) || (kind == CLOPTS_OPTION_COUNT) || (kind == CLOPTS_OPTION_VALUE)
            || (kind == CLOPTS_OPTION_LIST));
}

/* Checks of clopts_option_create(kind, l, s), in order. */
static clopts_decl_error_t option_check_new(clopts_option_kind_t kind, const char *l, char s)
{
    if (!option_valid_kind(kind))
    {
        return CLOPTS_DECL_INVALID_KIND;
    }
    if ((l == NULL) && (s == '\0'))
    {
        return CLOPTS_DECL_NO_NAME;
    }
    if (((l != NULL) && (!clopts_parser_valid_long_name(l))) || ((s != '\0') && (!clopts_parser_valid_short_name(s))))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_create(clopts_option_kind_t kind, const char *long_name, char short_name,
                                         clopts_option_t **out)
{
    clopts_option_t *option;
    clopts_decl_error_t error;

    if (out == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    *out = NULL;
    error = option_check_new(kind, long_name, short_name);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    option = (clopts_option_t *)clopts_core_calloc(1, sizeof(*option));
    if (option == NULL)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    option->attached = false;
    option->kind = kind;
    option->long_name = clopts_core_strdup(long_name);
    if ((long_name != NULL) && (option->long_name == NULL))
    {
        clopts_core_free(option);
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    option->short_name = short_name;
    option->help = NULL;
    option->value_name = NULL;
    option->default_value = NULL;
    option->env = NULL;
    option->delimiter = '\0';
    option->count_max = 0;
    option->required = false;
    option->negatable = false;
    option->terminal = false;
    option->index = 0;
    *out = option;
    return CLOPTS_DECL_NONE;
}

void clopts_option_destroy(clopts_option_t *option)
{
    if (option == NULL)
    {
        return;
    }
    assert(!option->attached && "an attached option is owned by its command");
    if (option->attached)
    {
        return;
    }
    clopts_parser_option_release(option);
}

/* Common checks of an option setter. */
static clopts_decl_error_t option_check_call(const clopts_option_t *option)
{
    if (option == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    return option->attached ? CLOPTS_DECL_ALREADY_ATTACHED : CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_help(clopts_option_t *option, const char *help)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return (clopts_core_replace_string(&option->help, help) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_value_name(clopts_option_t *option, const char *value_name)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return (clopts_core_replace_string(&option->value_name, value_name) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_env(clopts_option_t *option, const char *env)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((env != NULL) && option_takes_no_value(option->kind))
    {
        return CLOPTS_DECL_ENV_NOT_ALLOWED;
    }
    if ((env != NULL) && (env[0] == '\0'))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
    return (clopts_core_replace_string(&option->env, env) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((default_value != NULL) && option_takes_no_value(option->kind))
    {
        return CLOPTS_DECL_DEFAULT_NOT_ALLOWED;
    }
    if ((default_value != NULL) && option->required)
    {
        return CLOPTS_DECL_REQUIRED_WITH_DEFAULT;
    }
    return (clopts_core_replace_string(&option->default_value, default_value) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_required(clopts_option_t *option, bool required)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (required && option_takes_no_value(option->kind))
    {
        return CLOPTS_DECL_REQUIRED_NOT_ALLOWED;
    }
    if (required && (option->default_value != NULL))
    {
        return CLOPTS_DECL_REQUIRED_WITH_DEFAULT;
    }
    option->required = required;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (negatable && (option->kind != CLOPTS_OPTION_FLAG))
    {
        return CLOPTS_DECL_NOT_NEGATABLE;
    }
    option->negatable = negatable;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((delimiter != '\0') && (option->kind != CLOPTS_OPTION_LIST))
    {
        return CLOPTS_DECL_ATTRIBUTE_KIND;
    }
    option->delimiter = delimiter;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_count_max(clopts_option_t *option, unsigned count_max)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((count_max != 0) && (option->kind != CLOPTS_OPTION_COUNT))
    {
        return CLOPTS_DECL_ATTRIBUTE_KIND;
    }
    option->count_max = count_max;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_terminal(clopts_option_t *option, bool terminal)
{
    clopts_decl_error_t error = option_check_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (terminal && (option->kind != CLOPTS_OPTION_FLAG))
    {
        return CLOPTS_DECL_ATTRIBUTE_KIND;
    }
    option->terminal = terminal;
    return CLOPTS_DECL_NONE;
}

/* ---- read functions ---- */

clopts_option_kind_t clopts_option_kind_of(const clopts_option_t *option)
{
    return (option != NULL) ? option->kind : CLOPTS_OPTION_FLAG;
}

const char *clopts_option_long_name(const clopts_option_t *option)
{
    return (option != NULL) ? option->long_name : NULL;
}

char clopts_option_short_name(const clopts_option_t *option)
{
    return (option != NULL) ? option->short_name : '\0';
}

const char *clopts_option_help(const clopts_option_t *option)
{
    return (option != NULL) ? option->help : NULL;
}

const char *clopts_option_value_name(const clopts_option_t *option)
{
    return (option != NULL) ? option->value_name : NULL;
}

const char *clopts_option_env(const clopts_option_t *option)
{
    return (option != NULL) ? option->env : NULL;
}

const char *clopts_option_default(const clopts_option_t *option)
{
    return (option != NULL) ? option->default_value : NULL;
}

char clopts_option_delimiter(const clopts_option_t *option)
{
    return (option != NULL) ? option->delimiter : '\0';
}

unsigned clopts_option_count_max(const clopts_option_t *option)
{
    return (option != NULL) ? option->count_max : 0u;
}

bool clopts_option_required(const clopts_option_t *option)
{
    return (option != NULL) && option->required;
}

bool clopts_option_negatable(const clopts_option_t *option)
{
    return (option != NULL) && option->negatable;
}

bool clopts_option_terminal(const clopts_option_t *option)
{
    return (option != NULL) && option->terminal;
}
