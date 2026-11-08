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
 * of the main scan are not in place yet, so the main scan calls this for
 * every token (provisional behavior of the parse skeleton: positionals come
 * with task 3.3, long options with 3.4, short options with 3.5).
 */
static void scan_stop(scan_state_t *state)
{
    state->pos = state->argc;
}

/* True when the scan has decided the outcome before the end of the tokens. */
static bool scan_decided(const scan_state_t *state)
{
    return state->has_pending || (state->request != SCAN_REQUEST_NONE) || state->out_of_memory;
}

/* The current token; there must be one. */
static const char *scan_token(const scan_state_t *state)
{
    assert(!scan_at_end(state));
    return state->argv[state->pos];
}

/*
 * Records the usage error usage on the token argv[index] (its option,
 * positional, name_len and short_name are left empty) unless one is already
 * pending: only the first usage error is reported. error.command is the
 * selected command, or NULL before the selection.
 */
static clopts_error_t *scan_fail(scan_state_t *state, clopts_usage_error_t usage, int index)
{
    clopts_error_t *error = &state->pending_error;

    assert((index >= 0) && (index < state->argc));
    if (state->has_pending)
    {
        return NULL;
    }
    memset(error, 0, sizeof(*error));
    error->usage = usage;
    error->command = state->command;
    error->token = state->argv[index];
    error->argv_index = index;
    state->has_pending = true;
    return error;
}

/*
 * Selects command: allocates its slots and keeps state->command equal to
 * store->command. An allocation failure ends the scan with OUT_OF_MEMORY.
 */
static void scan_select(scan_state_t *state, const clopts_command_t *command)
{
    if (clopts_parser_store_select(state->result, command) != 0)
    {
        state->out_of_memory = true;
        return;
    }
    state->command = command;
}

/* HELP of the selected command (NULL: the whole program); the scan ends. */
static void scan_request_help(scan_state_t *state)
{
    state->request = SCAN_REQUEST_HELP;
}

/* VERSION; the scan ends. */
static void scan_request_version(scan_state_t *state)
{
    state->request = SCAN_REQUEST_VERSION;
}

/* True when token is exactly "--help" or "-h". */
static bool is_help_token(const char *token)
{
    return (strcmp(token, "--help") == 0) || (strcmp(token, "-h") == 0);
}

/* True when token is exactly "--version" and the program has a version. */
static bool is_version_token(const scan_state_t *state, const char *token)
{
    return (state->program->version != NULL) && (strcmp(token, "--version") == 0);
}

/*
 * Main scan (selected command, or a program without subcommands), until no
 * token is left or the scan decides the outcome.
 */
static void scan_main(scan_state_t *state)
{
    while (!scan_at_end(state) && !scan_decided(state))
    {
        scan_stop(state);
    }
}

/*
 * Pre-command row for a token starting with "-" other than "-" alone, "--"
 * and an exact --help / -h / --version: one option token, matched against the global
 * options only (4.3). Provisional entry: every such token is reported as an
 * unknown option until task 3.4 (long) and 3.5 (short) replace this row with
 * the option processing shared with the main scan.
 */
static void prelude_option(scan_state_t *state)
{
    const char *token = scan_token(state);
    clopts_error_t *error = scan_fail(state, CLOPTS_USAGE_UNKNOWN_OPTION, state->pos);

    if (error != NULL)
    {
        if (token[1] == '-')
        {
            error->name_len = strcspn(token, "=");
        }
        else
        {
            error->short_name = token[1];
        }
    }
    state->pos++;
}

/*
 * Pre-command row "help" (9.2, 9.3, 9.11): alone it is HELP of the whole
 * program; followed by a command name it is HELP of that command, and a
 * further token is an unexpected argument; followed by anything else it is an
 * unknown command.
 */
static void prelude_help_command(scan_state_t *state)
{
    const clopts_command_t *command;

    state->pos++;
    if (scan_at_end(state))
    {
        scan_request_help(state);
        return;
    }
    command = clopts_parser_find_command(state->program, scan_token(state));
    if (command == NULL)
    {
        (void)scan_fail(state, CLOPTS_USAGE_UNKNOWN_COMMAND, state->pos);
        state->pos++;
        return;
    }
    scan_select(state, command);
    if (state->out_of_memory)
    {
        return;
    }
    state->pos++;
    if (!scan_at_end(state))
    {
        (void)scan_fail(state, CLOPTS_USAGE_UNEXPECTED_ARGUMENT, state->pos);
        state->pos++;
        return;
    }
    scan_request_help(state);
}

/*
 * Pre-command row for a token that does not start with "-" (or is "-" alone,
 * or follows "--"): a command name selects that command (4.1); anything else
 * is an unknown command (4.2, 4.5: exact, case-sensitive, no prefix).
 */
static void prelude_command(scan_state_t *state)
{
    const clopts_command_t *command;

    command = clopts_parser_find_command(state->program, scan_token(state));
    if (state->dashdash || (command == NULL))
    {
        (void)scan_fail(state, CLOPTS_USAGE_UNKNOWN_COMMAND, state->pos);
        state->pos++;
        return;
    }
    scan_select(state, command);
    if (!state->out_of_memory)
    {
        state->pos++;
    }
}

/*
 * One token of the pre-command phase, in the order of the design table:
 * after "--" every token is a positional, which is an unknown command since no
 * command is selected; exact --help / -h, exact --version (with a version),
 * "--", "-" alone, other "-" tokens (options), "help", then command names.
 */
static void prelude_token(scan_state_t *state)
{
    const char *token = scan_token(state);

    if (state->dashdash)
    {
        prelude_command(state);
    }
    else if (is_help_token(token))
    {
        scan_request_help(state);
    }
    else if (is_version_token(state, token))
    {
        scan_request_version(state);
    }
    else if (strcmp(token, "--") == 0)
    {
        state->dashdash = true;
        state->pos++;
    }
    else if ((token[0] == '-') && (token[1] != '\0'))
    {
        prelude_option(state);
    }
    else if (strcmp(token, "help") == 0)
    {
        prelude_help_command(state);
    }
    else
    {
        prelude_command(state);
    }
}

/*
 * Pre-command phase (programs with subcommands), until a command is selected,
 * no token is left (no command: NO_COMMAND) or the phase decides the outcome.
 */
static void scan_prelude(scan_state_t *state)
{
    while (!scan_at_end(state) && !scan_decided(state) && (state->command == NULL))
    {
        prelude_token(state);
    }
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
