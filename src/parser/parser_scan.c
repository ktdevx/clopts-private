/*
 * Scanner: clopts_parse / clopts_parse_with_env, the scan before the
 * subcommand, the main scan, the recovery scan and the final status.
 *
 * Procedure (design, Scanner):
 *   1. reset command / error and release the previous storage, then check the
 *      declaration (DECL_ERROR without reading argv);
 *   2. allocate the result store;
 *   3. scan: the pre-command phase with subcommands, the main scan without;
 *   4. a pending usage error ends the parse;
 *   5. a HELP / VERSION request ends the parse;
 *   6. resolve environment variables / defaults, then check required ones;
 *   7. OK, or NO_COMMAND when the program has subcommands and none was
 *      selected.
 * Allocation and environment lookup failures end with CLOPTS_OUT_OF_MEMORY;
 * the result stays releasable with clopts_result_free().
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/* What the scan asked for besides a normal end. */
typedef enum scan_request
{
    SCAN_REQUEST_NONE,
    SCAN_REQUEST_HELP,
    SCAN_REQUEST_VERSION
} scan_request_t;

/*
 * State of one scan. Every row of the scan tables is a small function taking
 * this struct: it reads argv[pos], records into the store and advances pos.
 */
typedef struct scan_state
{
    const clopts_program_t *program;
    int argc;
    char *const *argv;
    clopts_result_t *result;
    clopts_parser_store_t *store;
    /* Index of the next argv element to look at; argc means the end. */
    int pos;
    /* Selected command (kept equal to store->command), or NULL. */
    const clopts_command_t *command;
    /* A usage error is pending in pending_error (only the first one is kept). */
    bool has_pending;
    clopts_error_t pending_error;
    /* "--" was reached: every following token is a positional. */
    bool dashdash;
    /* HELP / VERSION request; the target of HELP is command. */
    scan_request_t request;
    /* An allocation failed while recording. */
    bool out_of_memory;
} scan_state_t;

static void scan_init(scan_state_t *state, const clopts_program_t *program, int argc,
                      char *const *argv, clopts_result_t *result)
{
    memset(state, 0, sizeof(*state));
    state->program = program;
    state->argc = argc;
    state->argv = argv;
    state->result = result;
    state->store = result->impl;
    state->pos = 1;
    state->request = SCAN_REQUEST_NONE;
}

/* True when there is no token left. argc == 0 has no tokens at all. */
static bool scan_at_end(const scan_state_t *state)
{
    return (state->pos >= state->argc);
}

/*
 * Ends the scan at the current token as if no token were left. The token rows
 * of the pre-command phase and of the main scan are not in place yet, so the
 * phases call this for every token (provisional behavior of the parse
 * skeleton, replaced by the token rows).
 */
static void scan_stop(scan_state_t *state)
{
    state->pos = state->argc;
}

/*
 * Main scan (selected command, or a program without subcommands), until no
 * token is left or the scan decides the outcome.
 */
static void scan_main(scan_state_t *state)
{
    while (!scan_at_end(state))
    {
        scan_stop(state);
    }
}

/*
 * Pre-command phase (programs with subcommands), until a command is selected,
 * no token is left (no command: NO_COMMAND) or the phase decides the outcome.
 */
static void scan_prelude(scan_state_t *state)
{
    while (!scan_at_end(state))
    {
        scan_stop(state);
    }
}

/* True when the scan has decided the outcome before the end of the tokens. */
static bool scan_decided(const scan_state_t *state)
{
    return state->has_pending || (state->request != SCAN_REQUEST_NONE) || state->out_of_memory;
}

/* Finishes the parse with status, which is also returned. */
static clopts_status_t finish(clopts_result_t *result, clopts_status_t status)
{
    result->status = status;
    return status;
}

/*
 * Steps 4 and 5 of the procedure: ends the parse with the outcome the scan
 * decided (scan_decided() is true) and returns that status.
 */
static clopts_status_t scan_outcome(scan_state_t *state)
{
    clopts_result_t *result = state->result;

    result->command = state->command;
    if (state->out_of_memory)
    {
        return finish(result, CLOPTS_OUT_OF_MEMORY);
    }
    if (state->has_pending)
    {
        result->error = state->pending_error;
        return finish(result, CLOPTS_USAGE_ERROR);
    }
    if (state->request == SCAN_REQUEST_HELP)
    {
        return finish(result, CLOPTS_HELP);
    }
    assert(state->request == SCAN_REQUEST_VERSION);
    return finish(result, CLOPTS_VERSION);
}

static clopts_status_t parse(const clopts_program_t *program, int argc, char *const *argv,
                             clopts_getenv_fn getenv_fn, void *env_ctx, clopts_result_t *result)
{
    scan_state_t state;
    bool selected_scope;

    assert(program != NULL);
    assert(result != NULL);
    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    /* 1. */
    clopts_result_free(result);
    result->command = NULL;
    memset(&result->error, 0, sizeof(result->error));
    result->error.argv_index = -1;
    if (clopts_parser_decl_check(program, &result->error) != 0)
    {
        return finish(result, CLOPTS_DECL_ERROR);
    }

    /* 2. */
    if (clopts_parser_store_begin(result, program) != 0)
    {
        return finish(result, CLOPTS_OUT_OF_MEMORY);
    }

    /* 3. */
    scan_init(&state, program, argc, argv, result);
    if (program->command_count > 0)
    {
        scan_prelude(&state);
        if ((state.command != NULL) && !scan_decided(&state))
        {
            scan_main(&state);
        }
    }
    else
    {
        scan_main(&state);
    }
    assert(state.command == state.store->command);

    /* 4. and 5. */
    if (scan_decided(&state))
    {
        return scan_outcome(&state);
    }
    result->command = state.command;

    /* 6. */
    selected_scope = (program->command_count == 0) || (state.command != NULL);
    if (clopts_parser_resolve(result, getenv_fn, env_ctx, selected_scope) != 0)
    {
        return finish(result, CLOPTS_OUT_OF_MEMORY);
    }
    if (clopts_parser_check_required(result, selected_scope) != 0)
    {
        return finish(result, CLOPTS_USAGE_ERROR);
    }

    /* 7. */
    return finish(result, selected_scope ? CLOPTS_OK : CLOPTS_NO_COMMAND);
}

clopts_status_t clopts_parse(const clopts_program_t *program, int argc, char *const *argv,
                             clopts_result_t *result)
{
    return parse(program, argc, argv, NULL, NULL, result);
}

clopts_status_t clopts_parse_with_env(const clopts_program_t *program, int argc, char *const *argv,
                                      clopts_getenv_fn getenv_fn, void *env_ctx,
                                      clopts_result_t *result)
{
    return parse(program, argc, argv, getenv_fn, env_ctx, result);
}
