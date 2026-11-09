/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_INTERNAL_H
#define CLOPTS_DECL_INTERNAL_H

#include "clopts/decl.h"
#include "core/core_internal.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Internal declaration model: the only representation of a declaration, built
 * by the builder functions (option.c, positional.c, scope.c, command.c and
 * program.c). The layouts of a program, a command, an option and a
 * positional are private to program.c, command.c, option.c and positional.c.
 * The program and every command embed a scope (below) that holds
 * their options and positionals. Everything outside those files reads them
 * through the read functions only: the public ones, and the scope functions
 * for the parser (help-and-messages reads the public ones only).
 *
 * Invariants, guaranteed by the builder:
 *   - Everything attached to a program satisfies every declaration rule:
 *     a builder call that would break one fails and changes nothing. This
 *     holds as long as the caller keeps the ownership contract (no setter or
 *     attach after the attach, no handle attached twice), which is not
 *     checked.
 *   - The address of every handle stays the same until it is released:
 *     children are allocated one by one and the arrays hold pointers only.
 *   - Nothing points to its owner: an option or positional does not point to
 *     its program or command, and a command does not point to its program.
 *   - A program does not change once it is first parsed or written: this is
 *     a contract of the caller (clopts/program.h), not checked.
 */

/*
 * Internal contracts of the decl module, shared by its files (program.c,
 * command.c, option.c, positional.c, scope.c and names.c), its tests
 * (tests/decl/) and the parser, which reads a declaration through the scope
 * functions and looks names up with the index functions below. None of these
 * symbols is exported from the shared library. Every allocation goes through
 * the allocation gateway of core (clopts_core_malloc and the others,
 * core/core_internal.h).
 */

/*
 * Scope (scope.c): the options and positionals of the program or of a
 * command, in order of attachment. The program and every command embed one;
 * everything else reads it through the functions below. An attach checks the
 * rules against the other options and positionals of the same scope, in a
 * fixed order, and changes nothing on failure.
 */
typedef struct clopts_scope
{
    clopts_core_vec_t options;      /* clopts_option_t * elements, in order of attachment */
    clopts_core_vec_t positionals;  /* clopts_positional_t * elements, in order of attachment */
} clopts_scope_t;

/* Makes scope empty. */
void clopts_scope_init(clopts_scope_t *scope);

/* Releases the options and positionals of scope and its arrays, leaving it empty. */
void clopts_scope_release(clopts_scope_t *scope);

/* DUPLICATE_OPTION, NEGATION_CONFLICT (in that order) or OUT_OF_MEMORY; on success scope owns option. */
clopts_error_t clopts_scope_add_option(clopts_scope_t *scope, clopts_option_t *option);

/* VARIADIC_POSITION, POSITIONAL_ORDER (in that order) or OUT_OF_MEMORY; on success scope owns positional. */
clopts_error_t clopts_scope_add_positional(clopts_scope_t *scope, clopts_positional_t *positional);

size_t clopts_scope_option_count(const clopts_scope_t *scope);
/* index must be less than clopts_scope_option_count() (assert). */
const clopts_option_t *clopts_scope_option_at(const clopts_scope_t *scope, size_t index);
size_t clopts_scope_positional_count(const clopts_scope_t *scope);
/* index must be less than clopts_scope_positional_count() (assert). */
const clopts_positional_t *clopts_scope_positional_at(const clopts_scope_t *scope, size_t index);

/* The scope of command (command.c). */
const clopts_scope_t *clopts_command_scope(const clopts_command_t *command);

/* The scope of the program itself: its global options and its positionals (program.c). */
const clopts_scope_t *clopts_program_scope(const clopts_program_t *program);

/*
 * Name rules and comparisons (names.c).
 *
 * Names are compared byte by byte, exactly and case-sensitively; there is no
 * prefix matching. Every comparison of names, by the builder and by the
 * lookups of the parser, goes through the functions below.
 */

/* Program and command name rule: not NULL, not empty, no leading '-', no white space. */
bool clopts_decl_valid_command_name(const char *name);

/* Long name rule: not empty, no leading '-', no '=' and no white space. */
bool clopts_decl_valid_long_name(const char *name);

/* Short name rule: not '-', '=' or white space. */
bool clopts_decl_valid_short_name(char c);

/*
 * Returns the index in scope of the first option whose long name is the
 * name_len bytes at name, or clopts_scope_option_count(scope) when there is
 * none.
 */
size_t clopts_decl_index_of_long(const clopts_scope_t *scope, const char *name, size_t name_len);

/*
 * Returns the index in scope of the first option whose short name is c, or
 * clopts_scope_option_count(scope) when there is none (always for c '\0').
 */
size_t clopts_decl_index_of_short(const clopts_scope_t *scope, char c);

/*
 * Returns the first option of scope whose long name equals l (which may be
 * NULL) or whose short name equals s (when s is not '\0'), or NULL.
 */
const clopts_option_t *clopts_decl_find_conflict(const clopts_scope_t *scope, const char *l, char s);

/*
 * Returns the first option of scope whose long name is the name_len bytes
 * at name, or NULL (used for the no-x conflict checks).
 */
const clopts_option_t *clopts_decl_find_named(const clopts_scope_t *scope, const char *name, size_t name_len);

#endif /* CLOPTS_DECL_INTERNAL_H */
