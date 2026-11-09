/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_POSITIONAL_H
#define CLOPTS_POSITIONAL_H

#include <stdbool.h>

#include "clopts/decl_types.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Positional: creation, release and attributes. A positional is created
 * unattached, configured, and then attached with
 * clopts_command_add_positional() (clopts/command.h), which checks the order.
 */

/* Creates an unattached positional named name (copied, not empty), used in help and messages. */
CLOPTS_API clopts_decl_error_t clopts_positional_create(const char *name, clopts_positional_t **out);

/* Releases an unattached positional, as clopts_option_destroy() does for an option. */
CLOPTS_API void clopts_positional_destroy(clopts_positional_t *positional);

/* One-line description in help. NULL resets it. */
CLOPTS_API clopts_decl_error_t clopts_positional_set_help(clopts_positional_t *positional, const char *help);

/* Required positionals must precede optional ones (checked when attached). */
CLOPTS_API clopts_decl_error_t clopts_positional_set_required(clopts_positional_t *positional, bool required);

/* Takes all remaining tokens. Must be the last positional (checked when the next one is attached). */
CLOPTS_API clopts_decl_error_t clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic);

/* ---- read functions ---- */

CLOPTS_API const char *clopts_positional_name(const clopts_positional_t *positional);
/* NULL when not set. */
CLOPTS_API const char *clopts_positional_help(const clopts_positional_t *positional);
CLOPTS_API bool clopts_positional_required(const clopts_positional_t *positional);
CLOPTS_API bool clopts_positional_variadic(const clopts_positional_t *positional);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_POSITIONAL_H */
