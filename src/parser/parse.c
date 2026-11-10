/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Entry points of parsing (clopts/parse.h: clopts_parse,
 * clopts_parse_with_env) and the procedure of a parse:
 *
 *   1. the entry: NULL program (sentinel) and the allocation of the result
 *      (sentinel on failure);
 *   2. scan: the pre-command phase with subcommands, the main scan without
 *      (scan.c);
 *   3. a pending usage error ends the parse, unless the recovery scan finds
 *      a terminal option from its token on (scan.c);
 *   4. resolve environment variables / defaults (resolve.c);
 *   5. a terminal option ends the parse with TERMINATED; otherwise an
 *      environment value that cannot be read ends it with USAGE_ERROR;
 *   6. check required ones (check.c);
 *   7. OK, or NO_COMMAND when the program has subcommands and none was
 *      selected.
 *
 * Allocation failures and lookups that report FAILED after the result exists
 * end with CLOPTS_PARSE_OUT_OF_MEMORY on that result, which stays releasable with
 * clopts_result_destroy(). This file writes the final status of the result;
 * nothing else in the parse does.
 */

#include "parser/parser_internal.h"

#include <assert.h>

/*
 * Entry of parsing. Returns the sentinel when program is NULL or the result
 * cannot be allocated, and a new result otherwise.
 */
static clopts_result_t *parse_enter(const clopts_program_t *program)
{
    clopts_result_t *result;

    if (program == NULL)
    {
        return clopts_parser_result_oom();
    }

    result = clopts_parser_result_new(program);
    if (result == NULL)
    {
        return clopts_parser_result_oom();
    }
    return result;
}

/* Steps 2 to 7 of the procedure on a result that parse_enter() left open. */
static void parse_run(const clopts_program_t *program, int argc, char *const *argv, clopts_getenv_fn getenv_fn,
                      void *env_ctx, clopts_result_t *result)
{
    const clopts_option_t *terminal;
    clopts_parser_resolve_status_t resolved;
    bool selected_scope;

    /* 2. 3. */
    switch (clopts_parser_scan(result, program, argc, argv, &terminal))
    {
    case CLOPTS_PARSER_SCAN_DONE:
        break;
    case CLOPTS_PARSER_SCAN_USAGE_ERROR:
        clopts_parser_result_finish(result, CLOPTS_PARSE_USAGE_ERROR);
        return;
    case CLOPTS_PARSER_SCAN_OUT_OF_MEMORY:
        clopts_parser_result_finish(result, CLOPTS_PARSE_OUT_OF_MEMORY);
        return;
    }

    /* 4. */
    selected_scope = ((clopts_program_get_command_count(program) == 0) || (clopts_result_get_command(result) != NULL));
    resolved = clopts_parser_resolve(result, program, getenv_fn, env_ctx, selected_scope);
    if (resolved == CLOPTS_PARSER_RESOLVE_OUT_OF_MEMORY)
    {
        clopts_parser_result_finish(result, CLOPTS_PARSE_OUT_OF_MEMORY);
        return;
    }

    /* 5. */
    if (terminal != NULL)
    {
        clopts_parser_result_set_terminal(result, terminal);
        clopts_parser_result_finish(result, CLOPTS_PARSE_TERMINATED);
        return;
    }
    if (resolved == CLOPTS_PARSER_RESOLVE_INVALID)
    {
        clopts_parser_result_finish(result, CLOPTS_PARSE_USAGE_ERROR);
        return;
    }

    /* 6. */
    if (clopts_parser_check_required(result, program, selected_scope) != 0)
    {
        clopts_parser_result_finish(result, CLOPTS_PARSE_USAGE_ERROR);
        return;
    }

    /* 7. */
    clopts_parser_result_finish(result, selected_scope ? CLOPTS_PARSE_OK : CLOPTS_PARSE_NO_COMMAND);
}

/* Shared body of clopts_parse() and clopts_parse_with_env(). */
static clopts_result_t *parse(const clopts_program_t *program, int argc, char *const *argv, clopts_getenv_fn getenv_fn,
                              void *env_ctx)
{
    clopts_result_t *result;

    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    /* 1. */
    result = parse_enter(program);
    if (result == clopts_parser_result_oom())
    {
        return result;
    }

    parse_run(program, argc, argv, getenv_fn, env_ctx, result);
    return result;
}

clopts_result_t *clopts_parse(const clopts_program_t *program, int argc, char *const *argv)
{
    return parse(program, argc, argv, NULL, NULL);
}

clopts_result_t *clopts_parse_with_env(const clopts_program_t *program, int argc, char *const *argv,
                                       clopts_getenv_fn getenv_fn, void *env_ctx)
{
    return parse(program, argc, argv, getenv_fn, env_ctx);
}
