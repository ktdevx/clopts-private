/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Option (clopts/decl/option.h): creation, release and attributes. Everything
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

#include "clopts/decl/option.h"

#include "base/alloc.h"
#include "base/string.h"
#include "decl/name.h"

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
static clopts_error_t option_check_create(clopts_option_kind_t kind, const char *l, char s)
{
    if (!option_valid_kind(kind))
    {
        return CLOPTS_ERROR_INVALID_ARGUMENT;
    }

    if ((l == NULL) && (s == '\0'))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }

    if (((l != NULL) && (!clopts_name_valid_long(l))) || ((s != '\0') && (!clopts_name_valid_short(s))))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_option_create(clopts_option_kind_t kind, const char *long_name, char short_name,
                                    clopts_option_t **out)
{
    clopts_option_t *option;
    clopts_error_t error;

    assert(out != NULL);

    *out = NULL;

    error = option_check_create(kind, long_name, short_name);
    if (error != CLOPTS_ERROR_NONE)
    {
        return error;
    }

    option = (clopts_option_t *)clopts_malloc(sizeof(*option));
    if (option == NULL)
    {
        return CLOPTS_ERROR_OUT_OF_MEMORY;
    }

    option->kind = kind;
    option->long_name = NULL;
    if (long_name != NULL)
    {
        option->long_name = clopts_strdup(long_name);
        if (option->long_name == NULL)
        {
            clopts_free(option);
            return CLOPTS_ERROR_OUT_OF_MEMORY;
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

    return CLOPTS_ERROR_NONE;
}

void clopts_option_destroy(clopts_option_t *option)
{
    if (option == NULL)
    {
        return;
    }

    clopts_free(option->long_name);
    clopts_free(option->help);
    clopts_free(option->value_name);
    clopts_free(option->default_value);
    clopts_free(option->env);
    clopts_free(option);
}

clopts_error_t clopts_option_set_help(clopts_option_t *option, const char *help)
{
    assert(option != NULL);
    return (clopts_strrep(&option->help, help) == 0) ? CLOPTS_ERROR_NONE
                                                                  : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_option_set_value_name(clopts_option_t *option, const char *value_name)
{
    assert(option != NULL);
    return (clopts_strrep(&option->value_name, value_name) == 0) ? CLOPTS_ERROR_NONE
                                                                              : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_option_set_env(clopts_option_t *option, const char *env)
{
    assert(option != NULL);

    if (env != NULL)
    {
        if (option_takes_no_value(option->kind))
        {
            return CLOPTS_ERROR_NOT_APPLICABLE;
        }

        if (!clopts_name_valid_env(env))
        {
            return CLOPTS_ERROR_INVALID_NAME;
        }
    }

    return (clopts_strrep(&option->env, env) == 0) ? CLOPTS_ERROR_NONE
                                                                : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    assert(option != NULL);

    if (default_value != NULL)
    {
        if (option_takes_no_value(option->kind))
        {
            return CLOPTS_ERROR_NOT_APPLICABLE;
        }

        if (option->required)
        {
            return CLOPTS_ERROR_CONFLICT;
        }
    }

    return (clopts_strrep(&option->default_value, default_value) == 0) ? CLOPTS_ERROR_NONE
                                                                                    : CLOPTS_ERROR_OUT_OF_MEMORY;
}

clopts_error_t clopts_option_set_required(clopts_option_t *option, bool required)
{
    assert(option != NULL);

    if (required && option_takes_no_value(option->kind))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }

    if (required && (option->default_value != NULL))
    {
        return CLOPTS_ERROR_CONFLICT;
    }

    option->required = required;

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    assert(option != NULL);

    if (negatable && (option->kind != CLOPTS_OPTION_FLAG))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }

    option->negatable = negatable;

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    assert(option != NULL);

    if ((delimiter != '\0') && (option->kind != CLOPTS_OPTION_LIST))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }

    option->delimiter = delimiter;

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_option_set_count_max(clopts_option_t *option, unsigned count_max)
{
    assert(option != NULL);

    if ((count_max != 0) && (option->kind != CLOPTS_OPTION_COUNT))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }

    option->count_max = count_max;

    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_option_set_terminal(clopts_option_t *option, bool terminal)
{
    assert(option != NULL);

    if (terminal && (option->kind != CLOPTS_OPTION_FLAG))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }

    option->terminal = terminal;

    return CLOPTS_ERROR_NONE;
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
