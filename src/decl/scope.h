/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_SCOPE_H
#define CLOPTS_SRC_DECL_SCOPE_H

#include "clopts/decl/scope.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Scope (scope.c): the options and positionals of the program or of a
 * command, in order of attachment, and every comparison of the names of one
 * scope. This header is what the rest of the module looks names up with
 * (decl/lookup.h); the layout and the attach functions, which only the
 * owners that embed a scope use, are in decl/scope_impl.h.
 *
 * Names are compared byte by byte, exactly and case-sensitively, with no
 * prefix matching.
 */

/*
 * Looks up the long name written as "--<name>", given as the name_len bytes
 * at name: the option whose long name is exactly name, and otherwise, when
 * name is "no-" followed by at least one byte, the negatable FLAG named by
 * the rest (*negated is then true). Returns the option, or NULL. negated
 * must not be NULL.
 */
const clopts_option_t *clopts_scope_find_long(const clopts_scope_t *scope, const char *name, size_t name_len,
                                              bool *negated);

/* Looks up the short name written as "-<c>". Returns the option, or NULL (always for c '\0'). */
const clopts_option_t *clopts_scope_find_short(const clopts_scope_t *scope, char c);

#endif /* CLOPTS_SRC_DECL_SCOPE_H */
