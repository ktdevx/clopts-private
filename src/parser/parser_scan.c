/*
 * Entry points of parsing and validation (clopts_parse,
 * clopts_parse_with_env, clopts_program_validate) and the scanner: the scan
 * before the subcommand, the main scan, the recovery scan and the final
 * status.
 *
 * Procedure (design, "解析の全体の流れ" and Scan):
 *   1. the common entry scan_enter(): NULL program, freezing, a recorded
 *      OUT_OF_MEMORY (sentinel), the allocation of the result (sentinel on
 *      failure) and a recorded declaration error (DECL_ERROR without reading
 *      argv);
 *   2. scan: the pre-command phase with subcommands, the main scan without;
 *   3. a pending usage error ends the parse, unless the recovery scan finds
 *      a HELP / VERSION request from its token on;
 *   4. a HELP / VERSION request ends the parse;
 *   5. resolve environment variables / defaults, then check required ones;
 *   6. OK, or NO_COMMAND when the program has subcommands and none was
 *      selected.
 * Allocation and environment lookup failures after the result exists end
 * with CLOPTS_OUT_OF_MEMORY on that result, which stays releasable with
 * clopts_result_free(). The scanner never writes to program (except frozen
 * in scan_enter) nor to argv.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/*
 * Common entry of parsing and validation. Freezes program, then returns the
 * sentinel when program is NULL, has a recorded OUT_OF_MEMORY or the result
 * cannot be allocated. Otherwise returns a new result; when program has a
 * recorded declaration error, the result has status DECL_ERROR with that
 * error copied (the root reported as NULL), and *done is set to true.
 */
static clopts_result_t *scan_enter(clopts_program_t *program, bool *done)
{
    clopts_result_t *result;

    *done = true;
    if (program == NULL)
    {
        return clopts_parser_result_oom();
    }
    program->frozen = true;
    if (program->error.status == CLOPTS_OUT_OF_MEMORY)
    {
        return clopts_parser_result_oom();
    }

    result = clopts_parser_result_new(program);
    if (result == NULL)
    {
        return clopts_parser_result_oom();
    }

    if (program->error.status == CLOPTS_DECL_ERROR)
    {
        result->status = CLOPTS_DECL_ERROR;
        result->error.decl = program->error.kind;
        result->error.command = (program->error.command == &program->root) ? NULL : program->error.command;
        result->error.option = program->error.option;
        result->error.positional = program->error.positional;
        return result;
    }

    *done = false;
    return result;
}

/* What the scan asked for besides a normal end. */
typedef enum scan_request
{
    SCAN_REQUEST_NONE,
    SCAN_REQUEST_HELP,
    SCAN_REQUEST_VERSION
} scan_request_t;

/* A pending usage error: the fields of result->error a usage error sets. */
typedef struct scan_error
{
    clopts_usage_error_t usage;
    const clopts_command_t *command;
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const char *token;
    int argv_index;
    size_t name_len;
    char short_name;
} scan_error_t;

/*
 * State of one scan. Every row of the scan tables is a small function taking
 * this struct: it reads argv[pos], records into the result and advances pos.
 */
typedef struct scan_state
{
    const clopts_program_t *program;
    int argc;
    char *const *argv;
    clopts_result_t *result;
    /* Index of the next argv element to look at; argc means the end. */
    int pos;
    /* Selected command (kept equal to result->command), or NULL. */
    const clopts_command_t *command;
    /* A usage error is pending in pending_error (only the first one is kept). */
    bool has_pending;
    scan_error_t pending_error;
    /* "--" was reached: every following token is a positional. */
    bool dashdash;
    /* HELP / VERSION request; the target of HELP is command. */
    scan_request_t request;
    /* An allocation failed while recording. */
    bool out_of_memory;
} scan_state_t;

static void scan_init(scan_state_t *state, const clopts_program_t *program, int argc, char *const *argv,
                      clopts_result_t *result)
{
    memset(state, 0, sizeof(*state));
    state->program = program;
    state->argc = argc;
    state->argv = argv;
    state->result = result;
    state->pos = 1;
    state->command = NULL;
    state->has_pending = false;
    state->dashdash = false;
    state->request = SCAN_REQUEST_NONE;
    state->out_of_memory = false;
}

/* True when there is no token left. argc == 0 has no tokens at all. */
static bool scan_at_end(const scan_state_t *state)
{
    return (state->pos >= state->argc);
}

/* True when the scan has decided the outcome before the end of the tokens. */
static bool scan_decided(const scan_state_t *state)
{
    return (state->has_pending || (state->request != SCAN_REQUEST_NONE) || state->out_of_memory);
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
 * pending: only the first usage error is reported. The error command is the
 * selected command, or NULL before the selection. Returns the pending error
 * so that the caller can complete it, or NULL when none was recorded.
 */
static scan_error_t *scan_fail(scan_state_t *state, clopts_usage_error_t usage, int index)
{
    scan_error_t *error = &state->pending_error;

    assert((index >= 0) && (index < state->argc));
    if (state->has_pending)
    {
        return NULL;
    }
    error->usage = usage;
    error->command = state->command;
    error->option = NULL;
    error->positional = NULL;
    error->token = state->argv[index];
    error->argv_index = index;
    error->name_len = 0;
    error->short_name = '\0';
    state->has_pending = true;
    return error;
}

/*
 * Selects command with clopts_parser_result_select() (called once per
 * parse): allocates its slots and sets result->command, which
 * state->command follows. An allocation failure ends the scan with
 * OUT_OF_MEMORY.
 */
static void scan_select(scan_state_t *state, const clopts_command_t *command)
{
    assert(state->command == NULL);
    if (clopts_parser_result_select(state->result, command) != 0)
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
    return ((strcmp(token, "--help") == 0) || (strcmp(token, "-h") == 0));
}

/* True when token is exactly "--version" and the program has a version. */
static bool is_version_token(const scan_state_t *state, const char *token)
{
    return ((state->program->version != NULL) && (strcmp(token, "--version") == 0));
}

/* True when the name_len bytes at name are exactly the NUL-terminated word. */
static bool name_is(const char *name, size_t name_len, const char *word)
{
    return ((strlen(word) == name_len) && (memcmp(name, word, name_len) == 0));
}

/*
 * Records the value of a VALUE / LIST option found through ref: value is the
 * text after "=" (long) or the rest of a short group, or NULL to consume the
 * next token verbatim. No next token is a missing value: the error recorded
 * on argv[index] is returned so the caller can complete it, or NULL when
 * none was recorded. VALUE keeps the last one given; LIST appends the split
 * items.
 */
static scan_error_t *scan_option_value(scan_state_t *state, const clopts_parser_ref_t *ref, const char *value,
                                       int index)
{
    clopts_parser_slot_t *slot = clopts_parser_result_slot(state->result, ref);
    scan_error_t *error;

    if (value == NULL)
    {
        if (scan_at_end(state))
        {
            error = scan_fail(state, CLOPTS_USAGE_MISSING_VALUE, index);
            if (error != NULL)
            {
                error->option = ref->option;
            }
            return error;
        }
        value = scan_token(state);
        state->pos++;
    }
    if (ref->option->kind == CLOPTS_PARSER_VALUE)
    {
        clopts_parser_slot_set_value(slot, value, CLOPTS_SOURCE_EXPLICIT);
    }
    else if (clopts_parser_slot_append_list(slot, value, ref->option->delimiter,
                                            CLOPTS_SOURCE_EXPLICIT) != 0)
    {
        state->out_of_memory = true;
    }
    return NULL;
}

/*
 * Long option token "--<name>" / "--<name>=<value>". <name> ends at the
 * first "=". The reserved --help and --version (with a version) request HELP
 * of the selected command / VERSION, and take no value (option NULL in the
 * UNEXPECTED_VALUE error). Otherwise the name is looked up exactly, then as
 * --no-<name> of a negatable FLAG, in the selected command (if any) and the
 * global options. FLAG / COUNT take no value; VALUE / LIST take the text
 * after "=" or the next token.
 */
static void scan_long_option(scan_state_t *state)
{
    const int index = state->pos;
    const char *name = scan_token(state) + 2;
    const size_t name_len = strcspn(name, "=");
    const char *value = (name[name_len] == '=') ? (name + name_len + 1) : NULL;
    const bool reserved_help = name_is(name, name_len, "help");
    const bool reserved_version = ((state->program->version != NULL) && name_is(name, name_len, "version"));
    clopts_parser_ref_t ref;
    scan_error_t *error;

    state->pos++;
    if (reserved_help || reserved_version)
    {
        if (value != NULL)
        {
            (void)scan_fail(state, CLOPTS_USAGE_UNEXPECTED_VALUE, index);
        }
        else if (reserved_help)
        {
            scan_request_help(state);
        }
        else
        {
            scan_request_version(state);
        }
        return;
    }

    /* find_negation does not check for an exact match itself. */
    ref = clopts_parser_find_long(state->program, state->command, name, name_len);
    if (ref.option == NULL)
    {
        ref = clopts_parser_find_negation(state->program, state->command, name, name_len);
    }
    if (ref.option == NULL)
    {
        error = scan_fail(state, CLOPTS_USAGE_UNKNOWN_OPTION, index);
        if (error != NULL)
        {
            error->name_len = 2 + name_len;
        }
        return;
    }

    switch (ref.option->kind)
    {
    case CLOPTS_PARSER_FLAG:
    case CLOPTS_PARSER_COUNT:
        if (value != NULL)
        {
            error = scan_fail(state, CLOPTS_USAGE_UNEXPECTED_VALUE, index);
            if (error != NULL)
            {
                error->option = ref.option;
            }
        }
        else if (ref.option->kind == CLOPTS_PARSER_FLAG)
        {
            clopts_parser_slot_set_flag(clopts_parser_result_slot(state->result, &ref), !ref.negated,
                                        CLOPTS_SOURCE_EXPLICIT);
        }
        else
        {
            clopts_parser_slot_add_count(clopts_parser_result_slot(state->result, &ref), ref.option->count_max);
        }
        break;
    case CLOPTS_PARSER_VALUE:
    case CLOPTS_PARSER_LIST:
        (void)scan_option_value(state, &ref, value, index);
        break;
    }
}

/*
 * Short option token "-<c1><c2>...", one character at a time against the
 * selected command (if any) and the global options: "h" requests HELP of the
 * selected command (the whole program before the selection); an unknown
 * character is an unknown option on that character; FLAG / COUNT go on with
 * the next character; VALUE / LIST take the rest of the token verbatim ("="
 * included) when it is not empty, else the next token verbatim, and end the
 * token. A HELP request or a usage error ends the token as well.
 */
static void scan_short_option(scan_state_t *state)
{
    const int index = state->pos;
    const char *chars = scan_token(state) + 1;
    clopts_parser_ref_t ref;
    scan_error_t *error;
    size_t i;

    state->pos++;
    for (i = 0; chars[i] != '\0'; i++)
    {
        if (chars[i] == 'h')
        {
            scan_request_help(state);
            return;
        }
        ref = clopts_parser_find_short(state->program, state->command, chars[i]);
        if (ref.option == NULL)
        {
            error = scan_fail(state, CLOPTS_USAGE_UNKNOWN_OPTION, index);
            if (error != NULL)
            {
                error->short_name = chars[i];
            }
            return;
        }
        switch (ref.option->kind)
        {
        case CLOPTS_PARSER_FLAG:
            clopts_parser_slot_set_flag(clopts_parser_result_slot(state->result, &ref), true,
                                        CLOPTS_SOURCE_EXPLICIT);
            break;
        case CLOPTS_PARSER_COUNT:
            clopts_parser_slot_add_count(clopts_parser_result_slot(state->result, &ref), ref.option->count_max);
            break;
        case CLOPTS_PARSER_VALUE:
        case CLOPTS_PARSER_LIST:
            error = scan_option_value(state, &ref, (chars[i + 1] != '\0') ? (chars + i + 1) : NULL, index);
            if (error != NULL)
            {
                error->short_name = chars[i];
            }
            return;
        }
    }
}

/*
 * Row shared by the pre-command phase and the main scan for a token starting
 * with "-" other than "-" alone and "--" (before "--"): one option token,
 * matched against the selected command (none before the selection, so only
 * the global options) and the global options.
 */
static void scan_option(scan_state_t *state)
{
    if (scan_token(state)[1] == '-')
    {
        scan_long_option(state);
    }
    else
    {
        scan_short_option(state);
    }
}

/*
 * Main-scan row for a positional token: a token not starting with "-", "-"
 * alone, or any token after "--". Positionals are taken by the declared
 * positionals of the scope in order (the selected command, or the root of a
 * program without subcommands); a variadic last positional takes every
 * remaining one. A token beyond the declared ones without a variadic last
 * positional is an unexpected argument. Missing required positionals are
 * reported later by the required check.
 */
static void main_positional(scan_state_t *state)
{
    const clopts_command_t *scope = (state->command != NULL) ? state->command : &state->program->root;
    const size_t declared = scope->positionals.len;
    const size_t taken = state->result->positionals.len;
    const clopts_positional_t *last;

    if (taken >= declared)
    {
        last = (declared == 0)
                   ? NULL
                   : *(clopts_positional_t *const *)clopts_core_vec_at(&scope->positionals, declared - 1);
        if ((last == NULL) || !last->variadic)
        {
            (void)scan_fail(state, CLOPTS_USAGE_UNEXPECTED_ARGUMENT, state->pos);
            state->pos++;
            return;
        }
    }
    if (clopts_parser_result_add_positional(state->result, scan_token(state)) != 0)
    {
        state->out_of_memory = true;
        return;
    }
    state->pos++;
}

/*
 * One token of the main scan, in the order of the design table: after "--"
 * every token is a positional; "--" itself; "-" alone is a positional; other
 * "-" tokens are options; everything else is a positional.
 */
static void main_token(scan_state_t *state)
{
    const char *token = scan_token(state);

    if (state->dashdash)
    {
        main_positional(state);
    }
    else if (strcmp(token, "--") == 0)
    {
        state->dashdash = true;
        state->pos++;
    }
    else if ((token[0] == '-') && (token[1] != '\0'))
    {
        scan_option(state);
    }
    else
    {
        main_positional(state);
    }
}

/*
 * Main scan (selected command, or a program without subcommands), until no
 * token is left or the scan decides the outcome.
 */
static void scan_main(scan_state_t *state)
{
    while (!scan_at_end(state) && !scan_decided(state))
    {
        main_token(state);
    }
}

/*
 * Pre-command row "help": alone it is HELP of the whole program; followed by
 * a command name it is HELP of that command (selected as the help target),
 * and a further token is an unexpected argument; followed by anything else it
 * is an unknown command.
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
 * or follows "--"): a command name selects that command; anything else is an
 * unknown command (exact, case-sensitive, no prefix).
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
 * "--", other "-" tokens except "-" alone (options), "help", then command
 * names.
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
        scan_option(state);
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

/*
 * Recovery scan, run when a usage error is pending: the scan stopped right
 * after the first usage error, which stays the only one reported. The tokens
 * from the one of the pending error up to, not including, "--" are searched
 * for exact --help / -h / --version (with a version); the first one found
 * turns the outcome into HELP (of the selected command, else the whole
 * program) / VERSION. Nothing is looked up or recorded: any other token,
 * including option values and short groups containing "h", is skipped.
 *
 * The search starts at the pending error's own token, not the next one: in
 * "help --help", "help -h", "help copy --help" or "help --version" that
 * token is itself a request "anywhere before --" and wins over the unknown
 * command / unexpected argument it caused. The only other error tokens are
 * options, positionals and command names, which never equal a request token
 * exactly (the scan handles exact requests itself). When "--" was reached
 * before the error, every remaining token follows "--" and none counts
 * ("tool -- --help" stays an unknown command). Tokens consumed as option
 * values before the error precede it and are never searched; after the
 * error nothing is interpreted, so an exact token there counts even where it
 * would have been a value ("--bad -o -h" is HELP).
 */
static void scan_recover(scan_state_t *state)
{
    const char *token;
    int i;

    assert(state->has_pending && (state->pending_error.argv_index >= 0));
    if (state->dashdash)
    {
        return;
    }
    for (i = state->pending_error.argv_index; i < state->argc; i++)
    {
        token = state->argv[i];
        if (strcmp(token, "--") == 0)
        {
            return;
        }
        if (is_help_token(token))
        {
            scan_request_help(state);
            state->has_pending = false;
            return;
        }
        if (is_version_token(state, token))
        {
            scan_request_version(state);
            state->has_pending = false;
            return;
        }
    }
}

/* Finishes the parse with status: the only place that writes result->status after scan_enter(). */
static void finish(clopts_result_t *result, clopts_status_t status)
{
    result->status = status;
}

/* Copies the pending usage error into result->error. */
static void store_pending_error(clopts_result_t *result, const scan_error_t *error)
{
    result->error.usage = error->usage;
    result->error.decl = CLOPTS_DECL_NONE;
    result->error.command = error->command;
    result->error.option = error->option;
    result->error.positional = error->positional;
    result->error.token = error->token;
    result->error.argv_index = error->argv_index;
    result->error.name_len = error->name_len;
    result->error.short_name = error->short_name;
}

/*
 * Steps 3 and 4 of the procedure: ends the parse with the outcome the scan
 * decided (scan_decided() is true).
 */
static void scan_outcome(scan_state_t *state)
{
    clopts_result_t *result = state->result;

    if (state->out_of_memory)
    {
        finish(result, CLOPTS_OUT_OF_MEMORY);
        return;
    }
    if (state->has_pending)
    {
        scan_recover(state);
    }
    if (state->has_pending)
    {
        store_pending_error(result, &state->pending_error);
        finish(result, CLOPTS_USAGE_ERROR);
        return;
    }
    if (state->request == SCAN_REQUEST_HELP)
    {
        finish(result, CLOPTS_HELP);
        return;
    }
    assert(state->request == SCAN_REQUEST_VERSION);
    finish(result, CLOPTS_VERSION);
}

/* Steps 2 to 6 of the procedure on a result that scan_enter() left open. */
static void scan_run(const clopts_program_t *program, int argc, char *const *argv, clopts_getenv_fn getenv_fn,
                     void *env_ctx, clopts_result_t *result)
{
    scan_state_t state;
    bool selected_scope;

    /* 2. */
    scan_init(&state, program, argc, argv, result);
    if (program->commands.len > 0)
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
    assert(state.command == result->command);

    /* 3. and 4. */
    if (scan_decided(&state))
    {
        scan_outcome(&state);
        return;
    }

    /* 5. */
    selected_scope = ((program->commands.len == 0) || (state.command != NULL));
    if (clopts_parser_resolve(result, program, getenv_fn, env_ctx, selected_scope) != 0)
    {
        finish(result, CLOPTS_OUT_OF_MEMORY);
        return;
    }
    if (clopts_parser_check_required(result, program, selected_scope) != 0)
    {
        finish(result, CLOPTS_USAGE_ERROR);
        return;
    }

    /* 6. */
    finish(result, selected_scope ? CLOPTS_OK : CLOPTS_NO_COMMAND);
}

/* Shared body of clopts_parse() and clopts_parse_with_env(). */
static clopts_result_t *parse(clopts_program_t *program, int argc, char *const *argv, clopts_getenv_fn getenv_fn,
                              void *env_ctx)
{
    clopts_result_t *result;
    bool done;

    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    /* 1. */
    result = scan_enter(program, &done);
    if (result == clopts_parser_result_oom())
    {
        return result;
    }
    result->parsed = true;
    if (done)
    {
        return result;
    }

    scan_run(program, argc, argv, getenv_fn, env_ctx, result);
    return result;
}

clopts_result_t *clopts_parse(clopts_program_t *program, int argc, char *const *argv)
{
    return parse(program, argc, argv, NULL, NULL);
}

clopts_result_t *clopts_parse_with_env(clopts_program_t *program, int argc, char *const *argv,
                                       clopts_getenv_fn getenv_fn, void *env_ctx)
{
    return parse(program, argc, argv, getenv_fn, env_ctx);
}

clopts_result_t *clopts_program_validate(clopts_program_t *program)
{
    clopts_result_t *result;
    bool done;

    result = scan_enter(program, &done);
    if (!done)
    {
        result->status = CLOPTS_OK;
    }
    return result;
}
