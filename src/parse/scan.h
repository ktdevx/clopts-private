/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_SCAN_H
#define CLOPTS_SRC_PARSE_SCAN_H

#include "clopts/decl/program.h"

#include "parse/matches.h"
#include "parse/usage_error.h"

/* Outcome of clopts_scan(). */
typedef enum clopts_scan_status
{
    CLOPTS_SCAN_DONE,           /* every token was read, or a terminal option ended the scan */
    CLOPTS_SCAN_USAGE_ERROR,    /* *error holds the first usage error */
    CLOPTS_SCAN_OUT_OF_MEMORY
} clopts_scan_status_t;

/*
 * Scan (scan.c): steps 2 and 3 of parsing (parse.c). Reads argv[1..argc-1]
 * into matches, selecting a command when program has subcommands, then runs
 * the recovery scan after a usage error. Sets *terminal to the terminal
 * option that ended the scan, or NULL when the outcome is not DONE. error is
 * written only for USAGE_ERROR.
 */
clopts_scan_status_t clopts_scan(clopts_matches_t *matches, const clopts_program_t *program, int argc,
                                 char *const *argv, const clopts_option_t **terminal, clopts_usage_error_t *error);

#endif /* CLOPTS_SRC_PARSE_SCAN_H */
