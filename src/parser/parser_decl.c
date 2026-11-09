/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Name rules and lookups, shared by the builder and the scanner.
 *
 * Names are compared byte by byte, exactly and case-sensitively, with no
 * prefix matching. The checks of the builder look at one command only. The
 * find_* functions used by the scanner search the selected command (when
 * there is one) and then the program, so a name of the command shadows the
 * same global option; in each scope an exact long name comes before
 * --no-<name>.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/* ---- name rules ---------------------------------------------------------- */

/* True for the white space characters of the C locale. */
static bool decl_is_space(char c)
{
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\v') || (c == '\f') || (c == '\r'));
}

/* True when s contains a white space character. */
static bool decl_has_space(const char *s)
{
    for (; *s != '\0'; s++)
    {
        if (decl_is_space(*s))
        {
            return true;
        }
    }
    return false;
}

bool clopts_parser_valid_command_name(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (!decl_has_space(name)));
}

bool clopts_parser_valid_long_name(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (strchr(name, '=') == NULL)
            && (!decl_has_space(name)));
}

bool clopts_parser_valid_short_name(char c)
{
    /* '\0' means "no short name" and is not a name. */
    return ((c != '\0') && (c != '-') && (c != '=') && (!decl_is_space(c)));
}

/* ---- scopes ---------------------------------------------------------------- */

/* True when long_name is exactly the name_len bytes at name. */
static bool decl_long_equals(const char *long_name, const char *name, size_t name_len)
{
    return ((long_name != NULL) && (strlen(long_name) == name_len) && (memcmp(long_name, name, name_len) == 0));
}

/* What an option search in one scope looks for. */
typedef struct decl_query
{
    const char *long_name;  /* NULL: do not compare long names */
    size_t long_len;
    char short_name;        /* '\0': do not compare short names */
} decl_query_t;

/* True when option matches query. */
static bool decl_matches(const clopts_option_t *option, const decl_query_t *query)
{
    if ((query->long_name != NULL) && decl_long_equals(clopts_option_long_name(option), query->long_name, query->long_len))
    {
        return true;
    }
    return ((query->short_name != '\0') && (clopts_option_short_name(option) == query->short_name));
}

/* Returns the index in scope of the first option matching query, or clopts_scope_option_count(scope). */
static size_t decl_index_in(const clopts_scope_t *scope, const decl_query_t *query)
{
    size_t i;

    for (i = 0; i < clopts_scope_option_count(scope); i++)
    {
        if (decl_matches(clopts_scope_option_at(scope, i), query))
        {
            return i;
        }
    }
    return clopts_scope_option_count(scope);
}

/* Returns the first option matching query in scope, or NULL. */
static const clopts_option_t *decl_find_in(const clopts_scope_t *scope, const decl_query_t *query)
{
    size_t index = decl_index_in(scope, query);

    return (index < clopts_scope_option_count(scope)) ? clopts_scope_option_at(scope, index) : NULL;
}

const clopts_option_t *clopts_parser_find_conflict(const clopts_scope_t *scope, const char *l, char s)
{
    decl_query_t query;

    assert(scope != NULL);
    query.long_name = l;
    query.long_len = (l != NULL) ? strlen(l) : 0;
    query.short_name = s;
    return decl_find_in(scope, &query);
}

const clopts_option_t *clopts_parser_find_named(const clopts_scope_t *scope, const char *name, size_t name_len)
{
    decl_query_t query;

    assert((scope != NULL) && (name != NULL));
    query.long_name = name;
    query.long_len = name_len;
    query.short_name = '\0';
    return decl_find_in(scope, &query);
}

/* ---- lookups for the scanner --------------------------------------------- */

static clopts_parser_ref_t decl_no_ref(void)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    return ref;
}

/*
 * Looks query up in scope (global tells which one it is). For a long name
 * that is not found exactly, also looks up --no-<name> of a negatable FLAG.
 */
static clopts_parser_ref_t decl_lookup_in(const clopts_scope_t *scope, bool global, const decl_query_t *query)
{
    clopts_parser_ref_t ref = decl_no_ref();
    decl_query_t negation;
    size_t index;

    index = decl_index_in(scope, query);
    if (index < clopts_scope_option_count(scope))
    {
        ref.option = clopts_scope_option_at(scope, index);
        ref.global = global;
        ref.index = index;
        return ref;
    }
    if ((query->long_name == NULL) || (query->long_len <= 3) || (memcmp(query->long_name, "no-", 3) != 0))
    {
        return ref;
    }
    negation.long_name = query->long_name + 3;
    negation.long_len = query->long_len - 3;
    negation.short_name = '\0';
    index = decl_index_in(scope, &negation);
    if (index < clopts_scope_option_count(scope))
    {
        const clopts_option_t *option = clopts_scope_option_at(scope, index);

        if ((clopts_option_kind_of(option) == CLOPTS_OPTION_FLAG) && clopts_option_negatable(option))
        {
            ref.option = option;
            ref.global = global;
            ref.index = index;
            ref.negated = true;
        }
    }
    return ref;
}

/* Searches selected (NULL when no command is selected) and then global for the first option matching query. */
static clopts_parser_ref_t decl_lookup(const clopts_scope_t *global, const clopts_scope_t *selected,
                                       const decl_query_t *query)
{
    clopts_parser_ref_t ref;

    if (selected != NULL)
    {
        ref = decl_lookup_in(selected, false, query);
        if (ref.option != NULL)
        {
            return ref;
        }
    }
    return decl_lookup_in(global, true, query);
}

clopts_parser_ref_t clopts_parser_find_long(const clopts_scope_t *global, const clopts_scope_t *selected,
                                            const char *name, size_t name_len)
{
    decl_query_t query;

    assert((global != NULL) && (name != NULL));
    query.long_name = name;
    query.long_len = name_len;
    query.short_name = '\0';
    return decl_lookup(global, selected, &query);
}

clopts_parser_ref_t clopts_parser_find_short(const clopts_scope_t *global, const clopts_scope_t *selected, char c)
{
    decl_query_t query;

    assert(global != NULL);
    if (c == '\0')
    {
        return decl_no_ref();
    }
    query.long_name = NULL;
    query.long_len = 0;
    query.short_name = c;
    return decl_lookup(global, selected, &query);
}
