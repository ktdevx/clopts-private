/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_RESULT_H
#define CLOPTS_SRC_PARSE_RESULT_H

#include "clopts/parse/result.h"

#include "parse/matches.h"

/*
 * Result (result.c): the outcome of a parse, made once when the parse ends
 * and never written afterwards. The value accessors read the matches it
 * took over (parse/matches.h).
 */

/*
 * Makes the result of a parse that ended with status and moves matches
 * into it. terminal is read for TERMINATED only, error (copied) for
 * USAGE_ERROR only; either may be NULL otherwise. OUT_OF_MEMORY, NULL
 * matches or an allocation failure release matches and give the sentinel.
 */
clopts_result_t *clopts_result_create(clopts_parse_status_t status, const clopts_option_t *terminal,
                                      const clopts_usage_error_t *error, clopts_matches_t *matches);

/*
 * The shared out-of-memory sentinel: a static const result with status
 * CLOPTS_PARSE_OUT_OF_MEMORY and nothing else. Nothing writes to it and
 * clopts_result_destroy() ignores it.
 */
clopts_result_t *clopts_result_sentinel(void);

#endif /* CLOPTS_SRC_PARSE_RESULT_H */
