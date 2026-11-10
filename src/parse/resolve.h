/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_RESOLVE_H
#define CLOPTS_SRC_PARSE_RESOLVE_H

#include "clopts/decl/program.h"
#include "clopts/process/env.h"

#include "parse/matches.h"
#include "parse/usage_error.h"

#include <stdbool.h>

/* Outcome of clopts_resolve(). */
typedef enum clopts_resolve_status
{
    CLOPTS_RESOLVE_DONE,
    CLOPTS_RESOLVE_INVALID,        /* *error holds an INVALID_ENV_VALUE error */
    CLOPTS_RESOLVE_OUT_OF_MEMORY   /* a lookup reported FAILED, or a copy could not be allocated */
} clopts_resolve_status_t;

/*
 * Resolution (resolve.c): step 4 of parsing. Fills the VALUE / LIST slots
 * that got nothing from argv, from the environment and then from the default
 * value, in declaration order. A found environment value is copied before
 * the callback is called again. env_fn must not be NULL: parse.c chooses
 * the source. Only the global options are resolved when include_command is
 * false. A value the lookup reports as INVALID leaves its slot unset; the
 * first one is written to *error, and the resolution goes on. Stops at the
 * first FAILED lookup or allocation failure.
 */
clopts_resolve_status_t clopts_resolve(clopts_matches_t *matches, const clopts_program_t *program,
                                       clopts_env_fn env_fn, void *env_ctx, bool include_command,
                                       clopts_usage_error_t *error);

#endif /* CLOPTS_SRC_PARSE_RESOLVE_H */
