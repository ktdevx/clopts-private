/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_SCOPE_H
#define CLOPTS_DECL_SCOPE_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/base/export.h"
#include "clopts/decl/option.h"
#include "clopts/decl/positional.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Scope: the options and positionals of the program itself or of a
 * subcommand, in the order of attachment, read through one set of functions
 * whichever owns them. The program and every command have one
 * (clopts_program_get_scope(), clopts_command_get_scope()); it stays valid
 * as long as its owner does. A scope is read-only: options and positionals
 * are attached through its owner, whose rules differ (clopts/decl/program.h,
 * clopts/decl/command.h). A NULL scope or option is a contract violation
 * (assert).
 */

typedef struct clopts_scope clopts_scope_t;

/* Number of options, in the order of attachment. */
CLOPTS_API size_t clopts_scope_get_option_count(const clopts_scope_t *scope);
/* index must be less than clopts_scope_get_option_count() (caught by assert). */
CLOPTS_API const clopts_option_t *clopts_scope_get_option_at(const clopts_scope_t *scope, size_t index);
/* Number of positionals, in the order of attachment. */
CLOPTS_API size_t clopts_scope_get_positional_count(const clopts_scope_t *scope);
/* index must be less than clopts_scope_get_positional_count() (caught by assert). */
CLOPTS_API const clopts_positional_t *clopts_scope_get_positional_at(const clopts_scope_t *scope, size_t index);

/*
 * True when scope shadows option: every form option can be given in (its
 * long name, --no-<long name> when it is negatable, its short name) names an
 * option of scope, so a parse that looks names up in scope first never
 * reaches option. A global option shadowed by the scope of a subcommand
 * cannot be given after that subcommand (clopts/parse/parse.h). The names are
 * compared as a parse compares them (docs/parsing.md).
 */
CLOPTS_API bool clopts_scope_shadows_option(const clopts_scope_t *scope, const clopts_option_t *option);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_SCOPE_H */
