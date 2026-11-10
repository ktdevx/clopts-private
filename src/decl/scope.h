/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_SCOPE_H
#define CLOPTS_SRC_DECL_SCOPE_H

#include "clopts/decl/scope.h"

#include "base/vec.h"
#include "clopts/decl/decl_error.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Scope (scope.c): the options and positionals of the program or of a
 * command, in order of attachment. The program (program.c) and every command
 * (command.c) embed one; everything else reads it through the functions of
 * clopts/decl/scope.h and below.
 *
 * Every comparison of the names of one scope is made here: an attach checks
 * the rules against the other options of the same scope, a parse looks a
 * written name up (decl/lookup.h), and clopts_scope_shadows_option() asks
 * whether the written names of an option are found. Names are compared byte
 * by byte, exactly and case-sensitively, with no prefix matching.
 *
 * Invariants, guaranteed by the builder: everything attached satisfies every
 * declaration rule (a builder call that would break one fails and changes
 * nothing), and the address of every handle stays the same until it is
 * released (the arrays hold pointers only).
 */
struct clopts_scope
{
    clopts_vec_t options;      /* clopts_option_t * elements, in order of attachment */
    clopts_vec_t positionals;  /* clopts_positional_t * elements, in order of attachment */
};

/* Makes scope empty. */
void clopts_scope_init(clopts_scope_t *scope);

/* Releases the options and positionals of scope and its arrays, leaving it empty. */
void clopts_scope_release(clopts_scope_t *scope);

/* DUPLICATE_OPTION, NEGATION_CONFLICT (in that order) or OUT_OF_MEMORY; on success scope owns option. */
clopts_decl_error_t clopts_scope_add_option(clopts_scope_t *scope, clopts_option_t *option);

/* VARIADIC_POSITION, POSITIONAL_ORDER (in that order) or OUT_OF_MEMORY; on success scope owns positional. */
clopts_decl_error_t clopts_scope_add_positional(clopts_scope_t *scope, clopts_positional_t *positional);

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
