/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_SCOPE_INTERNAL_H
#define CLOPTS_SRC_DECL_SCOPE_INTERNAL_H

#include "clopts/decl/scope.h"

#include "base/vec.h"
#include "clopts/base/error.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Scope (scope.c): the options and positionals of the program or of a
 * command, in order of addition, the declaration error of the program they
 * belong to, and every comparison of the names of one scope. The program
 * (program.c) and every command (command.c) embed one; the rest of the module
 * looks names up in it (decl/lookup.h).
 *
 * Names are compared byte by byte, exactly and case-sensitively, with no
 * prefix matching.
 *
 * Every builder function of the module reports its failure through the
 * scope of the handle it changes (clopts_scope_fail()), which keeps the
 * first error in the program, and does nothing once the program has one
 * (clopts_scope_failed()). The fields may be read freely; they are written
 * only through the clopts_scope_* functions, never directly.
 *
 * Invariants, guaranteed by the builder: everything added satisfies every
 * declaration rule (a builder call that would break one fails and changes
 * nothing), and the address of every handle stays the same until it is
 * released (the arrays hold pointers only).
 */
struct clopts_scope
{
    clopts_vec_t options;      /* clopts_option_t * elements, in order of addition */
    clopts_vec_t positionals;  /* clopts_positional_t * elements, in order of addition */
    clopts_error_t *error;     /* the declaration error of the program the scope belongs to */
};

/* Makes scope empty, reporting to *error (the error of its program). */
void clopts_scope_init(clopts_scope_t *scope, clopts_error_t *error);

/* Releases the options and positionals of scope and its arrays, leaving it empty. */
void clopts_scope_release(clopts_scope_t *scope);

/* True when the program of scope has a declaration error: builder functions do nothing then. */
bool clopts_scope_failed(const clopts_scope_t *scope);

/* Keeps error as the error of the program of scope, unless it has one already. NONE does nothing. */
void clopts_scope_fail(clopts_scope_t *scope, clopts_error_t error);

/*
 * Creates an option of kind and adds it to scope. Returns it, or NULL after
 * clopts_scope_fail(): INVALID_ARGUMENT, INVALID_NAME (option.c), DUPLICATE,
 * CONFLICT or OUT_OF_MEMORY, in that order. NULL when scope has failed.
 */
clopts_option_t *clopts_scope_add_option(clopts_scope_t *scope, clopts_option_kind_t kind, const char *long_name,
                                         char short_name);

/*
 * Creates a positional and adds it to scope after the others. Returns it, or
 * NULL after clopts_scope_fail(): INVALID_NAME (positional.c), ORDER or
 * OUT_OF_MEMORY. NULL when scope has failed.
 */
clopts_positional_t *clopts_scope_add_positional(clopts_scope_t *scope, const char *name);

/*
 * The check of making option, which is in scope, negatable: CONFLICT when
 * scope has an option named no-<long name of option>, otherwise NONE.
 */
clopts_error_t clopts_scope_check_negatable(const clopts_scope_t *scope, const clopts_option_t *option);

/*
 * The check of giving positional, which is in scope, the attributes required
 * and variadic: ORDER when the positionals of scope would be out of order
 * (an optional one before a required one, or a variadic one before another
 * one), otherwise NONE.
 */
clopts_error_t clopts_scope_check_positional(const clopts_scope_t *scope, const clopts_positional_t *positional,
                                             bool required, bool variadic);

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

#endif /* CLOPTS_SRC_DECL_SCOPE_INTERNAL_H */
