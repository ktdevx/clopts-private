/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Option (clopts/option.h): creation, release and attributes. Everything
 * here depends on the option alone: nothing reads or writes a command or a
 * program. The checks against the other options of the same command are
 * made when the option is attached (command.c).
 *
 * Every function checks the rules that belong to it, in a fixed order, and
 * returns the first rule that fails, changing nothing. Resetting an attribute
 * (NULL, false, '\0' or 0) is not checked. A NULL option passed to a setter or
 * a read function, or a NULL output pointer passed to clopts_option_create(),
 * is a contract violation (assert).
 */

#include "clopts/option.h"
#include "decl/decl_internal.h"

#include <assert.h>

/*
 * Internal representation of an option. It is private to this file: the rest
 * of the library reads it through the public read functions. An option holds
 * its own attributes only: it does not point to the command it is attached
 * to, and does not know its position there. Attached, it is owned by that
 * command and read-only.
 */
struct clopts_option
{
    clopts_option_kind_t kind;
    char *long_name;            /* may be NULL */
    char short_name;            /* '\0' for none */
    char *help;                 /* may be NULL; owned copy */
    char *value_name;           /* may be NULL; owned copy */
    char *default_value;        /* may be NULL; owned copy */
    char *env;                  /* may be NULL; owned copy */
    char delimiter;             /* non-'\0' for LIST only */
    unsigned count_max;         /* non-0 for COUNT only; 0 means the default (UINT_MAX) */
    bool required;              /* true for VALUE / LIST only */
    bool negatable;             /* true for FLAG only */
    bool terminal;              /* true for FLAG only */
};

/* True for the kinds that take no value (FLAG / COUNT). */
static bool option_takes_no_value(clopts_option_kind_t kind)
{
    return ((kind == CLOPTS_OPTION_FLAG) || (kind == CLOPTS_OPTION_COUNT));
}

/* True for the values of clopts_option_kind_t. */
static bool option_valid_kind(clopts_option_kind_t kind)
{
    return ((kind == CLOPTS_OPTION_FLAG) || (kind == CLOPTS_OPTION_COUNT) || (kind == CLOPTS_OPTION_VALUE)
            || (kind == CLOPTS_OPTION_LIST));
}

/* Checks of clopts_option_create(kind, l, s), in order. */
static clopts_decl_error_t option_check_create(clopts_option_kind_t kind, const char *l, char s)
{
    if (!option_valid_kind(kind))
    {
        return CLOPTS_DECL_ERROR_INVALID_KIND;
    }

    if ((l == NULL) && (s == '\0'))
    {
        return CLOPTS_DECL_ERROR_NO_NAME;
    }

    if (((l != NULL) && (!clopts_decl_valid_long_name(l))) || ((s != '\0') && (!clopts_decl_valid_short_name(s))))
    {
        return CLOPTS_DECL_ERROR_INVALID_NAME;
    }

    return CLOPTS_DECL_ERROR_NONE;
}

clopts_decl_error_t clopts_option_create(clopts_option_kind_t kind, const char *long_name, char short_name,
                                         clopts_option_t **out)
{
    clopts_option_t *option;
    clopts_decl_error_t error;

    assert(out != NULL);

    *out = NULL;

    error = option_check_create(kind, long_name, short_name);
    if (error != CLOPTS_DECL_ERROR_NONE)
    {
        return error;
    }

    option = (clopts_option_t *)clopts_core_malloc(sizeof(*option));
    if (option == NULL)
    {
        return CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
    }

    option->kind = kind;
    option->long_name = NULL;
    if (long_name != NULL)
    {
        option->long_name = clopts_core_strdup(long_name);
        if (option->long_name == NULL)
        {
            clopts_core_free(option);
            return CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
        }
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

    *out = option;

    return CLOPTS_DECL_ERROR_NONE;
}

void clopts_option_destroy(clopts_option_t *option)
{
    if (option == NULL)
    {
        return;
    }

    clopts_core_free(option->long_name);
    clopts_core_free(option->help);
    clopts_core_free(option->value_name);
    clopts_core_free(option->default_value);
    clopts_core_free(option->env);
    clopts_core_free(option);
}

clopts_decl_error_t clopts_option_set_help(clopts_option_t *option, const char *help)
{
    assert(option != NULL);
    return (clopts_core_replace_string(&option->help, help) == 0) ? CLOPTS_DECL_ERROR_NONE
                                                                  : CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_value_name(clopts_option_t *option, const char *value_name)
{
    assert(option != NULL);
    return (clopts_core_replace_string(&option->value_name, value_name) == 0) ? CLOPTS_DECL_ERROR_NONE
                                                                              : CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_env(clopts_option_t *option, const char *env)
{
    assert(option != NULL);

    if (env != NULL)
    {
        if (option_takes_no_value(option->kind))
        {
            return CLOPTS_DECL_ERROR_ENV_NOT_ALLOWED;
        }

        if (!clopts_decl_valid_env_name(env))
        {
            return CLOPTS_DECL_ERROR_INVALID_NAME;
        }
    }

    return (clopts_core_replace_string(&option->env, env) == 0) ? CLOPTS_DECL_ERROR_NONE
                                                                : CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    assert(option != NULL);

    if (default_value != NULL)
    {
        if (option_takes_no_value(option->kind))
        {
            return CLOPTS_DECL_ERROR_DEFAULT_NOT_ALLOWED;
        }

        if (option->required)
        {
            return CLOPTS_DECL_ERROR_REQUIRED_WITH_DEFAULT;
        }
    }

    return (clopts_core_replace_string(&option->default_value, default_value) == 0) ? CLOPTS_DECL_ERROR_NONE
                                                                                    : CLOPTS_DECL_ERROR_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_option_set_required(clopts_option_t *option, bool required)
{
    assert(option != NULL);

    if (required && option_takes_no_value(option->kind))
    {
        return CLOPTS_DECL_ERROR_REQUIRED_NOT_ALLOWED;
    }

    if (required && (option->default_value != NULL))
    {
        return CLOPTS_DECL_ERROR_REQUIRED_WITH_DEFAULT;
    }

    option->required = required;

    return CLOPTS_DECL_ERROR_NONE;
}

clopts_decl_error_t clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    assert(option != NULL);

    if (negatable && (option->kind != CLOPTS_OPTION_FLAG))
    {
        return CLOPTS_DECL_ERROR_NOT_NEGATABLE;
    }

    option->negatable = negatable;

    return CLOPTS_DECL_ERROR_NONE;
}

clopts_decl_error_t clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    assert(option != NULL);

    if ((delimiter != '\0') && (option->kind != CLOPTS_OPTION_LIST))
    {
        return CLOPTS_DECL_ERROR_ATTRIBUTE_KIND;
    }

    option->delimiter = delimiter;

    return CLOPTS_DECL_ERROR_NONE;
}

clopts_decl_error_t clopts_option_set_count_max(clopts_option_t *option, unsigned count_max)
{
    assert(option != NULL);

    if ((count_max != 0) && (option->kind != CLOPTS_OPTION_COUNT))
    {
        return CLOPTS_DECL_ERROR_ATTRIBUTE_KIND;
    }

    option->count_max = count_max;

    return CLOPTS_DECL_ERROR_NONE;
}

clopts_decl_error_t clopts_option_set_terminal(clopts_option_t *option, bool terminal)
{
    assert(option != NULL);

    if (terminal && (option->kind != CLOPTS_OPTION_FLAG))
    {
        return CLOPTS_DECL_ERROR_ATTRIBUTE_KIND;
    }

    option->terminal = terminal;

    return CLOPTS_DECL_ERROR_NONE;
}

/* ---- read functions: a NULL option is a contract violation ---- */

clopts_option_kind_t clopts_option_get_kind(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->kind;
}

const char *clopts_option_get_long_name(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->long_name;
}

char clopts_option_get_short_name(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->short_name;
}

const char *clopts_option_get_help(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->help;
}

const char *clopts_option_get_value_name(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->value_name;
}

const char *clopts_option_get_env(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->env;
}

const char *clopts_option_get_default(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->default_value;
}

char clopts_option_get_delimiter(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->delimiter;
}

unsigned clopts_option_get_count_max(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->count_max;
}

bool clopts_option_is_required(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->required;
}

bool clopts_option_is_negatable(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->negatable;
}

bool clopts_option_is_terminal(const clopts_option_t *option)
{
    assert(option != NULL);
    return option->terminal;
}
