/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Scope: the options and positionals of the program or of a command, in
 * order of addition, the declaration error of their program, the checks
 * against the other options and positionals of the same scope, and every
 * comparison of the names of one scope. The program (program.c) and every
 * command (command.c) embed one; nothing else reads a scope but through the
 * functions here.
 *
 * Every add checks its rules in a fixed order and stops at the first rule
 * that fails. Nothing is changed before every check has passed and every
 * allocation has succeeded, so a failing call leaves the scope as it was.
 */

#include "decl/scope_internal.h"

#include "decl/option_internal.h"
#include "decl/positional_internal.h"

#include <assert.h>
#include <string.h>

/* Option index of scope; requires index < the option count. */
static clopts_option_t *scope_option(const clopts_scope_t *scope, size_t index)
{
    return *(clopts_option_t **)clopts_vec_at(&scope->options, index);
}

/* Positional index of scope; requires index < the positional count. */
static clopts_positional_t *scope_positional(const clopts_scope_t *scope, size_t index)
{
    return *(clopts_positional_t **)clopts_vec_at(&scope->positionals, index);
}

void clopts_scope_init(clopts_scope_t *scope, clopts_error_t *error)
{
    assert((scope != NULL) && (error != NULL));
    clopts_vec_init(&scope->options, sizeof(clopts_option_t *));
    clopts_vec_init(&scope->positionals, sizeof(clopts_positional_t *));
    scope->error = error;
}

void clopts_scope_release(clopts_scope_t *scope)
{
    size_t i;

    assert(scope != NULL);
    for (i = 0; i < scope->options.len; i++)
    {
        clopts_option_destroy(scope_option(scope, i));
    }
    clopts_vec_release(&scope->options);

    for (i = 0; i < scope->positionals.len; i++)
    {
        clopts_positional_destroy(scope_positional(scope, i));
    }
    clopts_vec_release(&scope->positionals);
}

bool clopts_scope_failed(const clopts_scope_t *scope)
{
    assert(scope != NULL);
    return *scope->error != CLOPTS_ERROR_NONE;
}

void clopts_scope_fail(clopts_scope_t *scope, clopts_error_t error)
{
    assert(scope != NULL);
    if (*scope->error == CLOPTS_ERROR_NONE)
    {
        *scope->error = error;
    }
}

/* ---- names ---------------------------------------------------------------- */

/* True when long_name (NULL: none) is prefix followed by the len bytes at name. */
static bool long_is(const char *long_name, const char *prefix, const char *name, size_t len)
{
    size_t prefix_len = strlen(prefix);

    return (long_name != NULL) && (strlen(long_name) == prefix_len + len) &&
           (memcmp(long_name, prefix, prefix_len) == 0) && (memcmp(long_name + prefix_len, name, len) == 0);
}

/* Index of the first option whose long name is prefix + the len bytes at name, or the option count. */
static size_t index_of_long(const clopts_scope_t *scope, const char *prefix, const char *name, size_t len)
{
    size_t i;

    for (i = 0; i < scope->options.len; i++)
    {
        if (long_is(clopts_option_get_long_name(scope_option(scope, i)), prefix, name, len))
        {
            return i;
        }
    }
    return scope->options.len;
}

/* Index of the first option whose short name is c, or the option count (always for c '\0'). */
static size_t index_of_short(const clopts_scope_t *scope, char c)
{
    size_t i;

    if (c != '\0')
    {
        for (i = 0; i < scope->options.len; i++)
        {
            if (clopts_option_get_short_name(scope_option(scope, i)) == c)
            {
                return i;
            }
        }
    }
    return scope->options.len;
}

/* True when option is a FLAG that can be given as --no-<name>. */
static bool is_negatable_flag(const clopts_option_t *option)
{
    return (clopts_option_get_kind(option) == CLOPTS_OPTION_FLAG) && clopts_option_is_negatable(option);
}

/*
 * The lookup of the long name written as "--" + prefix + the len bytes at
 * name (prefix is "" or "no-"): the exact long name first, then --no-<rest>
 * of a negatable FLAG. Allocates nothing, so a negation can be looked up
 * without building its text.
 */
static const clopts_option_t *find_written_long(const clopts_scope_t *scope, const char *prefix, const char *name,
                                                size_t len, bool *negated)
{
    const char *rest;
    size_t rest_len;
    size_t i;

    i = index_of_long(scope, prefix, name, len);
    if (i < scope->options.len)
    {
        *negated = false;
        return scope_option(scope, i);
    }

    /* The written name is "no-" followed by at least one byte: rest is that part. */
    if (prefix[0] != '\0')
    {
        rest = name;
        rest_len = len;
    }
    else if ((len > 3) && (memcmp(name, "no-", 3) == 0))
    {
        rest = name + 3;
        rest_len = len - 3;
    }
    else
    {
        return NULL;
    }
    if (rest_len == 0)
    {
        return NULL;
    }
    i = index_of_long(scope, "", rest, rest_len);
    if ((i == scope->options.len) || !is_negatable_flag(scope_option(scope, i)))
    {
        return NULL;
    }
    *negated = true;
    return scope_option(scope, i);
}

const clopts_option_t *clopts_scope_find_long(const clopts_scope_t *scope, const char *name, size_t name_len,
                                              bool *negated)
{
    assert((scope != NULL) && (name != NULL) && (negated != NULL));
    return find_written_long(scope, "", name, name_len, negated);
}

const clopts_option_t *clopts_scope_find_short(const clopts_scope_t *scope, char c)
{
    size_t i;

    assert(scope != NULL);
    i = index_of_short(scope, c);
    return (i < scope->options.len) ? scope_option(scope, i) : NULL;
}

bool clopts_scope_shadows_option(const clopts_scope_t *scope, const clopts_option_t *option)
{
    const char *l;
    char s;
    bool negated;

    assert((scope != NULL) && (option != NULL));
    l = clopts_option_get_long_name(option);
    s = clopts_option_get_short_name(option);

    if ((l != NULL) && (find_written_long(scope, "", l, strlen(l), &negated) == NULL))
    {
        return false;
    }
    if ((l != NULL) && is_negatable_flag(option) &&
        (find_written_long(scope, "no-", l, strlen(l), &negated) == NULL))
    {
        return false;
    }
    if ((s != '\0') && (index_of_short(scope, s) == scope->options.len))
    {
        return false;
    }
    return true;
}

/* ---- add ------------------------------------------------------------------- */

/* Checks of adding option to scope, in order. */
static clopts_error_t check_option(const clopts_scope_t *scope, const clopts_option_t *option)
{
    const char *l = clopts_option_get_long_name(option);
    const size_t count = scope->options.len;
    size_t i;

    /* The same long name or the same short name. */
    if (((l != NULL) && (index_of_long(scope, "", l, strlen(l)) < count)) ||
        (index_of_short(scope, clopts_option_get_short_name(option)) < count))
    {
        return CLOPTS_ERROR_DUPLICATE;
    }

    /* "no-<name>" when a negatable FLAG <name> is there. */
    if ((l != NULL) && (strncmp(l, "no-", 3) == 0))
    {
        i = index_of_long(scope, "", l + 3, strlen(l) - 3);
        if ((i < count) && is_negatable_flag(scope_option(scope, i)))
        {
            return CLOPTS_ERROR_CONFLICT;
        }
    }

    return CLOPTS_ERROR_NONE;
}

clopts_option_t *clopts_scope_add_option(clopts_scope_t *scope, clopts_option_kind_t kind, const char *long_name,
                                         char short_name)
{
    clopts_option_t *option;
    clopts_error_t error;

    assert(scope != NULL);
    if (clopts_scope_failed(scope))
    {
        return NULL;
    }

    error = clopts_option_create(kind, long_name, short_name, scope, &option);
    if (error == CLOPTS_ERROR_NONE)
    {
        error = check_option(scope, option);
    }
    if ((error == CLOPTS_ERROR_NONE) && (clopts_vec_push(&scope->options, &option) != 0))
    {
        error = CLOPTS_ERROR_OUT_OF_MEMORY;
    }
    if (error != CLOPTS_ERROR_NONE)
    {
        clopts_option_destroy(option);
        clopts_scope_fail(scope, error);
        return NULL;
    }
    return option;
}

clopts_positional_t *clopts_scope_add_positional(clopts_scope_t *scope, const char *name)
{
    clopts_positional_t *positional;
    clopts_error_t error;

    assert(scope != NULL);
    if (clopts_scope_failed(scope))
    {
        return NULL;
    }

    error = clopts_positional_create(name, scope, &positional);
    /* A new positional is optional and not variadic: only a variadic last one is in its way. */
    if ((error == CLOPTS_ERROR_NONE) && (scope->positionals.len > 0) &&
        clopts_positional_is_variadic(scope_positional(scope, scope->positionals.len - 1)))
    {
        error = CLOPTS_ERROR_ORDER;
    }
    if ((error == CLOPTS_ERROR_NONE) && (clopts_vec_push(&scope->positionals, &positional) != 0))
    {
        error = CLOPTS_ERROR_OUT_OF_MEMORY;
    }
    if (error != CLOPTS_ERROR_NONE)
    {
        clopts_positional_destroy(positional);
        clopts_scope_fail(scope, error);
        return NULL;
    }
    return positional;
}

/* ---- checks of the setters --------------------------------------------------- */

clopts_error_t clopts_scope_check_negatable(const clopts_scope_t *scope, const clopts_option_t *option)
{
    const char *l;

    assert((scope != NULL) && (option != NULL));
    l = clopts_option_get_long_name(option);
    if ((l != NULL) && (index_of_long(scope, "no-", l, strlen(l)) < scope->options.len))
    {
        return CLOPTS_ERROR_CONFLICT;
    }
    return CLOPTS_ERROR_NONE;
}

clopts_error_t clopts_scope_check_positional(const clopts_scope_t *scope, const clopts_positional_t *positional,
                                             bool required, bool variadic)
{
    const clopts_positional_t *p;
    bool seen_optional = false;
    bool is_required;
    size_t i;

    assert((scope != NULL) && (positional != NULL));
    for (i = 0; i < scope->positionals.len; i++)
    {
        p = scope_positional(scope, i);
        is_required = (p == positional) ? required : clopts_positional_is_required(p);
        if (is_required && seen_optional)
        {
            return CLOPTS_ERROR_ORDER;
        }
        if (!is_required)
        {
            seen_optional = true;
        }
        if (((p == positional) ? variadic : clopts_positional_is_variadic(p)) && (i + 1 < scope->positionals.len))
        {
            return CLOPTS_ERROR_ORDER;
        }
    }
    return CLOPTS_ERROR_NONE;
}

/* ---- read functions -------------------------------------------------------- */

size_t clopts_scope_get_option_count(const clopts_scope_t *scope)
{
    assert(scope != NULL);
    return scope->options.len;
}

const clopts_option_t *clopts_scope_get_option_at(const clopts_scope_t *scope, size_t index)
{
    assert(scope != NULL);
    assert((index < scope->options.len) && "index must be less than the option count");
    if (index >= scope->options.len)
    {
        return NULL;
    }
    return scope_option(scope, index);
}

size_t clopts_scope_get_positional_count(const clopts_scope_t *scope)
{
    assert(scope != NULL);
    return scope->positionals.len;
}

const clopts_positional_t *clopts_scope_get_positional_at(const clopts_scope_t *scope, size_t index)
{
    assert(scope != NULL);
    assert((index < scope->positionals.len) && "index must be less than the positional count");
    if (index >= scope->positionals.len)
    {
        return NULL;
    }
    return scope_positional(scope, index);
}
