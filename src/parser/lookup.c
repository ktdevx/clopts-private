/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Lookups of the scanner: an option by its long or short name in the
 * selected command (when there is one) and then in the program, so a name of
 * the command shadows the same global option. In each scope an exact long
 * name comes before --no-<name> of a negatable FLAG. The names themselves
 * are compared by the decl module (decl/names.c).
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

static clopts_parser_ref_t lookup_no_ref(void)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    return ref;
}

/* Returns a ref to the option at index of scope, or no ref when index is the option count. */
static clopts_parser_ref_t lookup_ref_at(const clopts_scope_t *scope, bool global, size_t index)
{
    clopts_parser_ref_t ref = lookup_no_ref();

    if (index < clopts_scope_option_count(scope))
    {
        ref.option = clopts_scope_option_at(scope, index);
        ref.global = global;
        ref.index = index;
    }
    return ref;
}

/*
 * Looks the long name up in scope (global tells which one it is). When it is
 * not found exactly, also looks up --no-<name> of a negatable FLAG.
 */
static clopts_parser_ref_t lookup_long_in(const clopts_scope_t *scope, bool global, const char *name, size_t name_len)
{
    clopts_parser_ref_t ref;

    ref = lookup_ref_at(scope, global, clopts_decl_index_of_long(scope, name, name_len));
    if ((ref.option != NULL) || (name_len <= 3) || (memcmp(name, "no-", 3) != 0))
    {
        return ref;
    }
    ref = lookup_ref_at(scope, global, clopts_decl_index_of_long(scope, name + 3, name_len - 3));
    if ((ref.option == NULL) || (clopts_option_get_kind(ref.option) != CLOPTS_OPTION_FLAG)
        || !clopts_option_is_negatable(ref.option))
    {
        return lookup_no_ref();
    }
    ref.negated = true;
    return ref;
}

clopts_parser_ref_t clopts_parser_find_long(const clopts_scope_t *global, const clopts_scope_t *selected,
                                            const char *name, size_t name_len)
{
    clopts_parser_ref_t ref;

    assert((global != NULL) && (name != NULL));
    if (selected != NULL)
    {
        ref = lookup_long_in(selected, false, name, name_len);
        if (ref.option != NULL)
        {
            return ref;
        }
    }
    return lookup_long_in(global, true, name, name_len);
}

clopts_parser_ref_t clopts_parser_find_short(const clopts_scope_t *global, const clopts_scope_t *selected, char c)
{
    clopts_parser_ref_t ref;

    assert(global != NULL);
    if (c == '\0')
    {
        return lookup_no_ref();
    }
    if (selected != NULL)
    {
        ref = lookup_ref_at(selected, false, clopts_decl_index_of_short(selected, c));
        if (ref.option != NULL)
        {
            return ref;
        }
    }
    return lookup_ref_at(global, true, clopts_decl_index_of_short(global, c));
}
