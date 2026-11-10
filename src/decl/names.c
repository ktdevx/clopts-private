/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Name rules and comparisons, shared by the builder and the lookups of the
 * parser (parser/scan.c).
 *
 * Names are compared byte by byte, exactly and case-sensitively, with no
 * prefix matching. Every function here looks at one scope only.
 */

#include "decl/decl_internal.h"

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

bool clopts_decl_valid_command_name(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (!decl_has_space(name)));
}

bool clopts_decl_valid_long_name(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (strchr(name, '=') == NULL)
            && (!decl_has_space(name)));
}

bool clopts_decl_valid_short_name(char c)
{
    /* '\0' means "no short name" and is not a name. */
    return ((c != '\0') && (c != '-') && (c != '=') && (!decl_is_space(c)));
}

/*
 * True when the bytes at s, up to the NUL, are valid UTF-8: shortest forms
 * only, no surrogates (U+D800..U+DFFF) and nothing above U+10FFFF.
 */
static bool decl_valid_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;

    while (*p != 0)
    {
        unsigned char c = *p;
        size_t len;
        unsigned char lo = 0x80;
        unsigned char hi = 0xBF;
        size_t i;

        if (c < 0x80)
        {
            p++;
            continue;
        }
        if ((c >= 0xC2) && (c <= 0xDF))
        {
            len = 2;
        }
        else if ((c >= 0xE0) && (c <= 0xEF))
        {
            len = 3;
            lo = (c == 0xE0) ? 0xA0 : 0x80;   /* no overlong form */
            hi = (c == 0xED) ? 0x9F : 0xBF;   /* no surrogate */
        }
        else if ((c >= 0xF0) && (c <= 0xF4))
        {
            len = 4;
            lo = (c == 0xF0) ? 0x90 : 0x80;   /* no overlong form */
            hi = (c == 0xF4) ? 0x8F : 0xBF;   /* nothing above U+10FFFF */
        }
        else
        {
            return false;
        }
        /* The second byte has the narrowed range, the others 80..BF; a NUL stops the check. */
        if ((p[1] < lo) || (p[1] > hi))
        {
            return false;
        }
        for (i = 2; i < len; i++)
        {
            if ((p[i] < 0x80) || (p[i] > 0xBF))
            {
                return false;
            }
        }
        p += len;
    }
    return true;
}

bool clopts_decl_valid_env_name(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (strchr(name, '=') == NULL) && decl_valid_utf8(name));
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
    if ((query->long_name != NULL) && decl_long_equals(clopts_option_get_long_name(option), query->long_name, query->long_len))
    {
        return true;
    }
    return ((query->short_name != '\0') && (clopts_option_get_short_name(option) == query->short_name));
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

size_t clopts_decl_index_of_long(const clopts_scope_t *scope, const char *name, size_t name_len)
{
    decl_query_t query;

    assert((scope != NULL) && (name != NULL));
    query.long_name = name;
    query.long_len = name_len;
    query.short_name = '\0';
    return decl_index_in(scope, &query);
}

size_t clopts_decl_index_of_short(const clopts_scope_t *scope, char c)
{
    decl_query_t query;

    assert(scope != NULL);
    if (c == '\0')
    {
        return clopts_scope_option_count(scope);
    }
    query.long_name = NULL;
    query.long_len = 0;
    query.short_name = c;
    return decl_index_in(scope, &query);
}

const clopts_option_t *clopts_decl_find_conflict(const clopts_scope_t *scope, const char *l, char s)
{
    decl_query_t query;

    assert(scope != NULL);
    query.long_name = l;
    query.long_len = (l != NULL) ? strlen(l) : 0;
    query.short_name = s;
    return decl_find_in(scope, &query);
}

const clopts_option_t *clopts_decl_find_named(const clopts_scope_t *scope, const char *name, size_t name_len)
{
    decl_query_t query;

    assert((scope != NULL) && (name != NULL));
    query.long_name = name;
    query.long_len = name_len;
    query.short_name = '\0';
    return decl_find_in(scope, &query);
}
