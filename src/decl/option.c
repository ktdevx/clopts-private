/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Option (clopts/decl/option.h): creation, release and attributes. An option
 * knows the scope it is added to (scope.c): its setters report their failure
 * there, do nothing once the program has an error, and ask the scope for the
 * check that needs the other options (clopts_option_set_negatable()). The
 * checks of adding it are made by the scope.
 *
 * Every setter checks the rules that belong to it, in a fixed order, and
 * stops at the first rule that fails, changing nothing. Resetting an
 * attribute (NULL, false, '\0' or 0) is not checked. A setter given a NULL
 * option does nothing; a read function given one is a contract violation
 * (assert).
 */

#include "decl/option_internal.h"

#include "base/alloc_internal.h"
#include "base/str.h"
#include "decl/name.h"
#include "decl/scope_internal.h"

#include <assert.h>

/*
 * Internal representation of an option. It is private to this file: the rest
 * of the library reads it through the public read functions. It is owned by
 * the scope it was added to.
 */
struct clopts_option
{
    clopts_scope_t *scope;      /* the scope that holds it */
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
                                    clopts_scope_t *scope, clopts_option_t **out)
{
    clopts_option_t *option;
    clopts_error_t error;

    assert((scope != NULL) && (out != NULL));

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

    option->scope = scope;
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

/* True when a setter given option has nothing to do: option is NULL, or its program has an error. */
static bool option_skip(const clopts_option_t *option)
{
    return (option == NULL) || clopts_scope_failed(option->scope);
}

/* Reports error (not NONE) for option; returns true when there is one. */
static bool option_fail(clopts_option_t *option, clopts_error_t error)
{
    if (error == CLOPTS_ERROR_NONE)
    {
        return false;
    }
    clopts_scope_fail(option->scope, error);
    return true;
}

/* Replaces the string attribute *slot with a copy of text, reporting OUT_OF_MEMORY on failure. */
static void option_set_string(clopts_option_t *option, char **slot, const char *text)
{
    if (clopts_strrep(slot, text) != 0)
    {
        (void)option_fail(option, CLOPTS_ERROR_OUT_OF_MEMORY);
    }
}

void clopts_option_set_help(clopts_option_t *option, const char *help)
{
    if (option_skip(option))
    {
        return;
    }
    option_set_string(option, &option->help, help);
}

void clopts_option_set_value_name(clopts_option_t *option, const char *value_name)
{
    if (option_skip(option))
    {
        return;
    }
    option_set_string(option, &option->value_name, value_name);
}

/* Checks of clopts_option_set_env(option, env), in order. */
static clopts_error_t option_check_env(const clopts_option_t *option, const char *env)
{
    if (env == NULL)
    {
        return CLOPTS_ERROR_NONE;
    }
    if (option_takes_no_value(option->kind))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }
    if (!clopts_name_valid_env(env))
    {
        return CLOPTS_ERROR_INVALID_NAME;
    }
    return CLOPTS_ERROR_NONE;
}

void clopts_option_set_env(clopts_option_t *option, const char *env)
{
    if (option_skip(option) || option_fail(option, option_check_env(option, env)))
    {
        return;
    }
    option_set_string(option, &option->env, env);
}

/* Checks of clopts_option_set_default(option, default_value), in order. */
static clopts_error_t option_check_default(const clopts_option_t *option, const char *default_value)
{
    if (default_value == NULL)
    {
        return CLOPTS_ERROR_NONE;
    }
    if (option_takes_no_value(option->kind))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }
    if (option->required)
    {
        return CLOPTS_ERROR_CONFLICT;
    }
    return CLOPTS_ERROR_NONE;
}

void clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    if (option_skip(option) || option_fail(option, option_check_default(option, default_value)))
    {
        return;
    }
    option_set_string(option, &option->default_value, default_value);
}

/* Checks of clopts_option_set_required(option, required), in order. */
static clopts_error_t option_check_required(const clopts_option_t *option, bool required)
{
    if (!required)
    {
        return CLOPTS_ERROR_NONE;
    }
    if (option_takes_no_value(option->kind))
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }
    if (option->default_value != NULL)
    {
        return CLOPTS_ERROR_CONFLICT;
    }
    return CLOPTS_ERROR_NONE;
}

void clopts_option_set_required(clopts_option_t *option, bool required)
{
    if (option_skip(option) || option_fail(option, option_check_required(option, required)))
    {
        return;
    }
    option->required = required;
}

/* Checks of clopts_option_set_negatable(option, negatable), in order. */
static clopts_error_t option_check_negatable(const clopts_option_t *option, bool negatable)
{
    if (!negatable)
    {
        return CLOPTS_ERROR_NONE;
    }
    if (option->kind != CLOPTS_OPTION_FLAG)
    {
        return CLOPTS_ERROR_NOT_APPLICABLE;
    }
    return clopts_scope_check_negatable(option->scope, option);
}

void clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    if (option_skip(option) || option_fail(option, option_check_negatable(option, negatable)))
    {
        return;
    }
    option->negatable = negatable;
}

void clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    if (option_skip(option) ||
        option_fail(option, ((delimiter != '\0') && (option->kind != CLOPTS_OPTION_LIST)) ? CLOPTS_ERROR_NOT_APPLICABLE
                                                                                         : CLOPTS_ERROR_NONE))
    {
        return;
    }
    option->delimiter = delimiter;
}

void clopts_option_set_count_max(clopts_option_t *option, unsigned count_max)
{
    if (option_skip(option) ||
        option_fail(option, ((count_max != 0) && (option->kind != CLOPTS_OPTION_COUNT)) ? CLOPTS_ERROR_NOT_APPLICABLE
                                                                                       : CLOPTS_ERROR_NONE))
    {
        return;
    }
    option->count_max = count_max;
}

void clopts_option_set_terminal(clopts_option_t *option, bool terminal)
{
    if (option_skip(option) ||
        option_fail(option, (terminal && (option->kind != CLOPTS_OPTION_FLAG)) ? CLOPTS_ERROR_NOT_APPLICABLE
                                                                              : CLOPTS_ERROR_NONE))
    {
        return;
    }
    option->terminal = terminal;
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
