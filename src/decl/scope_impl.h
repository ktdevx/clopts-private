/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_SCOPE_IMPL_H
#define CLOPTS_SRC_DECL_SCOPE_IMPL_H

#include "decl/scope.h"

#include "base/vec.h"
#include "clopts/base/error.h"

/*
 * The implementation of Scope (scope.c) for the owners that embed one: the
 * program (program.c) and every command (command.c). Only files of the decl
 * module include it (docs/dev/design.md, chapter 3).
 *
 * An attach checks the rules against the other options and positionals of
 * the same scope and changes nothing on failure, so the arrays are written
 * through the functions below only, never directly.
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

/* DUPLICATE, CONFLICT (in that order) or OUT_OF_MEMORY; on success scope owns option. */
clopts_error_t clopts_scope_add_option(clopts_scope_t *scope, clopts_option_t *option);

/* ORDER or OUT_OF_MEMORY; on success scope owns positional. */
clopts_error_t clopts_scope_add_positional(clopts_scope_t *scope, clopts_positional_t *positional);

#endif /* CLOPTS_SRC_DECL_SCOPE_IMPL_H */
