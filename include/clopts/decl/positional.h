/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_POSITIONAL_H
#define CLOPTS_DECL_POSITIONAL_H

#include <stdbool.h>

#include "clopts/base/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Positional: attributes. A positional is created by adding it to the
 * program (clopts_program_add_positional(), clopts/decl/program.h) or to a
 * command (clopts_command_add_positional(), clopts/decl/command.h), and is
 * then configured with the setters below. The positionals of one program or
 * command must keep their order: the required ones first, and a variadic one
 * last. A setter that fails changes nothing and leaves its error in the
 * program; a NULL positional does nothing (the rules are in
 * clopts/decl/program.h).
 */

typedef struct clopts_positional clopts_positional_t;

/* One-line description in help. NULL resets it. */
CLOPTS_API void clopts_positional_set_help(clopts_positional_t *positional, const char *help);

/*
 * The positional must be given. ORDER (required: an optional positional
 * precedes it; optional: a required positional follows it). Make each
 * positional required before adding the next one.
 */
CLOPTS_API void clopts_positional_set_required(clopts_positional_t *positional, bool required);

/* Takes all remaining tokens. ORDER (another positional follows it). */
CLOPTS_API void clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic);

/* ---- read functions ---- */

CLOPTS_API const char *clopts_positional_get_name(const clopts_positional_t *positional);
/* NULL when not set. */
CLOPTS_API const char *clopts_positional_get_help(const clopts_positional_t *positional);
CLOPTS_API bool clopts_positional_is_required(const clopts_positional_t *positional);
CLOPTS_API bool clopts_positional_is_variadic(const clopts_positional_t *positional);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_POSITIONAL_H */
