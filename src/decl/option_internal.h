/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_OPTION_INTERNAL_H
#define CLOPTS_SRC_DECL_OPTION_INTERNAL_H

#include "clopts/decl/option.h"

#include "clopts/base/error.h"
#include "clopts/decl/scope.h"

/*
 * Option (option.c): the creation and release used by the scope that holds
 * the option (scope.c). The public setters are in clopts/decl/option.h.
 */

/*
 * Creates an option of kind into *out for scope, which it reports its
 * setter failures to. Checks the option alone: INVALID_ARGUMENT (kind) or
 * INVALID_NAME, then OUT_OF_MEMORY. *out is NULL on failure. The caller adds
 * it to scope.
 */
clopts_error_t clopts_option_create(clopts_option_kind_t kind, const char *long_name, char short_name,
                                    clopts_scope_t *scope, clopts_option_t **out);

/* Releases option. NULL does nothing. */
void clopts_option_destroy(clopts_option_t *option);

#endif /* CLOPTS_SRC_DECL_OPTION_INTERNAL_H */
