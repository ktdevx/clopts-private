/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_LOOKUP_H
#define CLOPTS_SRC_DECL_LOOKUP_H

#include "clopts/decl/command.h"
#include "clopts/decl/option.h"
#include "clopts/decl/program.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Lookup (lookup.c): which option a written name names during a parse. The
 * selected command (when there is one) is looked up first and then the
 * program, so a name of the command shadows the same global option. Within
 * each scope the rules of clopts_scope_find_long() and
 * clopts_scope_find_short() apply (decl/scope_internal.h).
 */

/* Where a looked-up option was found. */
typedef struct clopts_lookup
{
    const clopts_option_t *option;  /* NULL when not found */
    bool negated;                   /* found through --no-<name> */
} clopts_lookup_t;

/*
 * Looks up the long name written as "--<name>" (the name_len bytes at name)
 * in command (NULL: none selected) and then in program.
 */
clopts_lookup_t clopts_lookup_long(const clopts_program_t *program, const clopts_command_t *command,
                                   const char *name, size_t name_len);

/* Looks up the short name written as "-<c>" in command (NULL: none selected) and then in program. */
clopts_lookup_t clopts_lookup_short(const clopts_program_t *program, const clopts_command_t *command, char c);

#endif /* CLOPTS_SRC_DECL_LOOKUP_H */
