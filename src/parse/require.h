/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_REQUIRE_H
#define CLOPTS_SRC_PARSE_REQUIRE_H

#include "clopts/decl/program.h"

#include "parse/matches.h"
#include "parse/usage_error_internal.h"

#include <stdbool.h>

/*
 * Required checks (require.c): step 6 of parsing. Checks the required
 * options and positionals in declaration order (global options, options of
 * the selected command, positionals); an option counts as given when its
 * source is EXPLICIT or ENV. Only the global options are checked when
 * include_command is false. Returns 0 when nothing is missing; otherwise
 * writes the first missing one to *error and returns -1.
 */
int clopts_require(const clopts_matches_t *matches, const clopts_program_t *program, bool include_command,
                   clopts_usage_error_t *error);

#endif /* CLOPTS_SRC_PARSE_REQUIRE_H */
