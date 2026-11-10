/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Entry points of parsing (clopts/parse/parse.h: clopts_parse,
 * clopts_parse_with_env) and the procedure of a parse:
 *
 *   1. the entry: NULL program (sentinel) and the allocation of the matches
 *      (sentinel on failure);
 *   2. scan: the pre-command phase with subcommands, the main scan without
 *      (scan.c);
 *   3. a pending usage error ends the parse, unless the recovery scan finds
 *      a terminal option from its token on (scan.c);
 *   4. resolve environment variables / defaults (resolve.c);
 *   5. a terminal option ends the parse with TERMINATED; otherwise an
 *      environment value that cannot be read ends it with USAGE_ERROR;
 *   6. check required ones (require.c);
 *   7. OK, or NO_COMMAND when the program has subcommands and none was
 *      selected.
 *
 * The outcome becomes the result in one step (result.c), which takes the
 * matches over; an allocation failure or a lookup that reports FAILED gives
 * the out-of-memory sentinel.
 *
 * Without a lookup callback, the process environment is read through the
 * process module (process/env_internal.h), with a lookup context that lives on the
 * stack of parse() for this parse only. The steps themselves see a lookup
 * callback only.
 */

#include "clopts/parse/parse.h"

#include "parse/matches.h"
#include "parse/require.h"
#include "parse/resolve.h"
#include "parse/result_internal.h"
#include "parse/scan.h"
#include "parse/usage_error_internal.h"
#include "process/env_internal.h"

#include <assert.h>

/* Steps 2 to 7 on matches. Returns the status; *terminal and *error as the result reads them. */
static clopts_parse_status_t run(clopts_matches_t *matches, const clopts_program_t *program, int argc,
                                 char *const *argv, clopts_env_fn env_fn, void *env_ctx,
                                 const clopts_option_t **terminal, clopts_usage_error_t *error)
{
    clopts_resolve_status_t resolved;
    bool selected_scope;

    /* 2. 3. */
    switch (clopts_scan(matches, program, argc, argv, terminal, error))
    {
    case CLOPTS_SCAN_DONE:
        break;
    case CLOPTS_SCAN_USAGE_ERROR:
        return CLOPTS_PARSE_USAGE_ERROR;
    case CLOPTS_SCAN_OUT_OF_MEMORY:
    default:
        return CLOPTS_PARSE_OUT_OF_MEMORY;
    }

    /* 4. */
    selected_scope = ((clopts_program_get_command_count(program) == 0) || (clopts_matches_get_command(matches) != NULL));
    resolved = clopts_resolve(matches, program, env_fn, env_ctx, selected_scope, error);
    if (resolved == CLOPTS_RESOLVE_OUT_OF_MEMORY)
    {
        return CLOPTS_PARSE_OUT_OF_MEMORY;
    }

    /* 5. */
    if (*terminal != NULL)
    {
        return CLOPTS_PARSE_TERMINATED;
    }
    if (resolved == CLOPTS_RESOLVE_INVALID)
    {
        return CLOPTS_PARSE_USAGE_ERROR;
    }

    /* 6. */
    if (clopts_require(matches, program, selected_scope, error) != 0)
    {
        return CLOPTS_PARSE_USAGE_ERROR;
    }

    /* 7. */
    return selected_scope ? CLOPTS_PARSE_OK : CLOPTS_PARSE_NO_COMMAND;
}

/*
 * Shared body of clopts_parse() and clopts_parse_with_env(). Without env_fn
 * (NULL), the process environment is the source of the environment values.
 */
static clopts_result_t *parse(const clopts_program_t *program, int argc, char *const *argv,
                              clopts_env_fn env_fn, void *env_ctx)
{
    clopts_matches_t *matches;
    const clopts_option_t *terminal = NULL;
    clopts_usage_error_t error;
    clopts_parse_status_t status;
    clopts_env_t process_env;

    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));
    assert((program == NULL) || (clopts_program_get_error(program) == CLOPTS_ERROR_NONE));

    /* 1. */
    if (program == NULL)
    {
        return clopts_result_sentinel();
    }
    matches = clopts_matches_create(program);
    if (matches == NULL)
    {
        return clopts_result_sentinel();
    }
    clopts_usage_error_init(&error, CLOPTS_USAGE_ERROR_NONE);

    if (env_fn != NULL)
    {
        status = run(matches, program, argc, argv, env_fn, env_ctx, &terminal, &error);
    }
    else
    {
        clopts_env_init(&process_env);
        status = run(matches, program, argc, argv, clopts_env_lookup, &process_env, &terminal, &error);
        clopts_env_release(&process_env);
    }
    return clopts_result_create(status, terminal, &error, matches);
}

clopts_result_t *clopts_parse(const clopts_program_t *program, int argc, char *const *argv)
{
    return parse(program, argc, argv, NULL, NULL);
}

clopts_result_t *clopts_parse_with_env(const clopts_program_t *program, int argc, char *const *argv,
                                       clopts_env_fn env_fn, void *env_ctx)
{
    return parse(program, argc, argv, env_fn, env_ctx);
}
