/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_POSITIONAL_INTERNAL_H
#define CLOPTS_SRC_DECL_POSITIONAL_INTERNAL_H

#include "clopts/decl/positional.h"

#include "clopts/base/error.h"
#include "clopts/decl/scope.h"

/*
 * Positional (positional.c): the creation and release used by the scope that
 * holds the positional (scope.c). The public setters are in
 * clopts/decl/positional.h.
 */

/*
 * Creates an optional, non-variadic positional named name into *out for
 * scope, which it reports its setter failures to: INVALID_NAME (NULL or
 * empty), then OUT_OF_MEMORY. *out is NULL on failure. The caller adds it to
 * scope.
 */
clopts_error_t clopts_positional_create(const char *name, clopts_scope_t *scope, clopts_positional_t **out);

/* Releases positional. NULL does nothing. */
void clopts_positional_destroy(clopts_positional_t *positional);

#endif /* CLOPTS_SRC_DECL_POSITIONAL_INTERNAL_H */
