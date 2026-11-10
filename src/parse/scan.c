/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Scan: steps 2 and 3 of parsing (parse.c). The pre-command phase, the main
 * scan and the recovery scan record into the matches.
 *
 *   2. scan: the pre-command phase with subcommands, the main scan without;
 *   3. a pending usage error is the outcome, unless the recovery scan finds a
 *      terminal option from its token on.
 *
 * The scan never writes to program or argv, and it knows no built-in option
 * or command. What an argv element looks like is read by token.c; which
 * option a written name names is decided by the decl module
 * (decl/lookup.h): the selected command first, then the program.
 */

#include "parse/scan.h"

#include "decl/lookup.h"
#include "parse/token.h"

#include <assert.h>
#include <string.h>

/*
 * State of one scan. Every row of the scan tables is a small function taking
 * this struct: it reads argv[pos], records into the matches and advances pos.
 */
typedef struct scan_state
{
    const clopts_program_t *program;
    int argc;
    char *const *argv;
    clopts_matches_t *matches;
    /* Index of the next argv element to look at; argc means the end. */
    int pos;
    /* Selected command (kept equal to the command of the matches), or NULL. */
    const clopts_command_t *command;
    /* A usage error is pending in pending_error (only the first one is kept). */
    bool has_pending;
    clopts_usage_error_t pending_error;
    /* "--" was reached: every following token is a positional. */
    bool dashdash;
    /* The terminal option that ended the scan, or NULL. */
    const clopts_option_t *terminal;
    /* An allocation failed while recording. */
    bool out_of_memory;
} scan_state_t;

static void scan_init(scan_state_t *state, clopts_matches_t *matches, const clopts_program_t *program, int argc,
                      char *const *argv)
{
    memset(state, 0, sizeof(*state));
    state->program = program;
    state->argc = argc;
    state->argv = argv;
    state->matches = matches;
    state->pos = 1;
    state->command = NULL;
    state->has_pending = false;
    clopts_usage_error_init(&state->pending_error, CLOPTS_USAGE_ERROR_NONE, NULL);
    state->dashdash = false;
    state->terminal = NULL;
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
    return (state->has_pending || (state->terminal != NULL) || state->out_of_memory);
}

/* The current token; there must be one. */
static const char *scan_text(const scan_state_t *state)
{
    assert(!scan_at_end(state));
    return state->argv[state->pos];
}

/*
 * Records the usage error kind on the token argv[index] (its option,
 * positional, name_len and short_name are left empty) unless one is already
 * pending: only the first usage error is reported. The error command is the
 * selected command, or NULL before the selection. Returns the pending error
 * so that the caller can complete it, or NULL when none was recorded.
 */
static clopts_usage_error_t *scan_fail(scan_state_t *state, clopts_usage_error_kind_t kind, int index)
{
    clopts_usage_error_t *error = &state->pending_error;

    assert((index >= 0) && (index < state->argc));
    if (state->has_pending)
    {
        return NULL;
    }
    clopts_usage_error_init(error, kind, state->command);
    error->token = state->argv[index];
    error->argv_index = index;
    state->has_pending = true;
    return error;
}

/*
 * Selects command (once per parse): allocates its slots in the matches,
 * which state->command follows. An allocation failure ends the scan with
 * OUT_OF_MEMORY.
 */
static void scan_select(scan_state_t *state, const clopts_command_t *command)
{
    assert(state->command == NULL);
    if (clopts_matches_select(state->matches, command) != 0)
    {
        state->out_of_memory = true;
        return;
    }
    state->command = command;
}

/*
 * Records a FLAG found through found (negated or not) given by argv. A
 * terminal one ends the scan, unless it was given as --no-<name>.
 */
static void scan_flag(scan_state_t *state, const clopts_lookup_t *found)
{
    clopts_slot_set_flag(clopts_matches_slot(state->matches, found->option), !found->negated,
                         CLOPTS_SOURCE_EXPLICIT);
    if (clopts_option_is_terminal(found->option) && !found->negated)
    {
        state->terminal = found->option;
    }
}

/* Records one more COUNT option given by argv. */
static void scan_count(scan_state_t *state, const clopts_option_t *option)
{
    clopts_slot_add_count(clopts_matches_slot(state->matches, option), clopts_option_get_count_max(option));
}

/*
 * Records the value of a VALUE / LIST option: value is the text after "="
 * (long) or the rest of a short group, or NULL to consume the next token
 * verbatim. No next token is a missing value: the error recorded on
 * argv[index] is returned so the caller can complete it, or NULL when none
 * was recorded. VALUE keeps the last one given; LIST appends the split items.
 */
static clopts_usage_error_t *scan_option_value(scan_state_t *state, const clopts_option_t *option, const char *value,
                                               int index)
{
    clopts_slot_t *slot = clopts_matches_slot(state->matches, option);
    clopts_usage_error_t *error;

    if (value == NULL)
    {
        if (scan_at_end(state))
        {
            error = scan_fail(state, CLOPTS_USAGE_ERROR_MISSING_VALUE, index);
            if (error != NULL)
            {
                error->option = option;
            }
            return error;
        }
        value = scan_text(state);
        state->pos++;
    }
    if (clopts_option_get_kind(option) == CLOPTS_OPTION_VALUE)
    {
        clopts_slot_set_value(slot, value, CLOPTS_SOURCE_EXPLICIT);
    }
    else if (clopts_slot_append_list(slot, value, clopts_option_get_delimiter(option), CLOPTS_SOURCE_EXPLICIT) != 0)
    {
        state->out_of_memory = true;
    }
    return NULL;
}

/*
 * Long option token "--<name>" / "--<name>=<value>". FLAG / COUNT take no
 * value; VALUE / LIST take the text after "=" or the next token.
 */
static void scan_long_option(scan_state_t *state, const clopts_token_t *token)
{
    const int index = state->pos;
    clopts_lookup_t found;
    clopts_usage_error_t *error;

    state->pos++;
    found = clopts_lookup_long(state->program, state->command, token->name, token->name_len);
    if (found.option == NULL)
    {
        error = scan_fail(state, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, index);
        if (error != NULL)
        {
            error->name_len = 2 + token->name_len;
        }
        return;
    }

    switch (clopts_option_get_kind(found.option))
    {
    case CLOPTS_OPTION_FLAG:
    case CLOPTS_OPTION_COUNT:
        if (token->value != NULL)
        {
            error = scan_fail(state, CLOPTS_USAGE_ERROR_UNEXPECTED_VALUE, index);
            if (error != NULL)
            {
                error->option = found.option;
            }
        }
        else if (clopts_option_get_kind(found.option) == CLOPTS_OPTION_FLAG)
        {
            scan_flag(state, &found);
        }
        else
        {
            scan_count(state, found.option);
        }
        break;
    case CLOPTS_OPTION_VALUE:
    case CLOPTS_OPTION_LIST:
        (void)scan_option_value(state, found.option, token->value, index);
        break;
    }
}

/*
 * Short option token "-<c1><c2>...", one character at a time: an unknown
 * character is an unknown option on that character; FLAG / COUNT go on with
 * the next character; VALUE / LIST take the rest of the token verbatim ("="
 * included) when it is not empty, else the next token verbatim, and end the
 * token. A terminal FLAG or a usage error ends the token as well.
 */
static void scan_short_option(scan_state_t *state, const clopts_token_t *token)
{
    const int index = state->pos;
    const char *chars = token->name;
    clopts_lookup_t found;
    clopts_usage_error_t *error;
    size_t i;

    state->pos++;
    for (i = 0; i < token->name_len; i++)
    {
        found = clopts_lookup_short(state->program, state->command, chars[i]);
        if (found.option == NULL)
        {
            error = scan_fail(state, CLOPTS_USAGE_ERROR_UNKNOWN_OPTION, index);
            if (error != NULL)
            {
                error->short_name = chars[i];
            }
            return;
        }
        switch (clopts_option_get_kind(found.option))
        {
        case CLOPTS_OPTION_FLAG:
            scan_flag(state, &found);
            if (state->terminal != NULL)
            {
                return;
            }
            break;
        case CLOPTS_OPTION_COUNT:
            scan_count(state, found.option);
            break;
        case CLOPTS_OPTION_VALUE:
        case CLOPTS_OPTION_LIST:
            error = scan_option_value(state, found.option, (i + 1 < token->name_len) ? (chars + i + 1) : NULL, index);
            if (error != NULL)
            {
                error->short_name = chars[i];
            }
            return;
        }
    }
}

/*
 * Main-scan row for a positional token: a word, or any token after "--".
 * Positionals are taken by the declared positionals of the scope in order
 * (the selected command, or the program itself when it has no subcommands);
 * a variadic last positional takes every remaining one. A token beyond the
 * declared ones without a variadic last positional is an unexpected
 * argument. Missing required positionals are reported later by the required
 * check.
 */
static void main_positional(scan_state_t *state)
{
    const clopts_scope_t *scope = (state->command != NULL) ? clopts_command_get_scope(state->command)
                                                           : clopts_program_get_scope(state->program);
    const size_t declared = clopts_scope_get_positional_count(scope);
    const size_t taken = clopts_matches_get_positional_count(state->matches);
    const clopts_positional_t *last;

    if (taken >= declared)
    {
        last = (declared == 0) ? NULL : clopts_scope_get_positional_at(scope, declared - 1);
        if ((last == NULL) || !clopts_positional_is_variadic(last))
        {
            (void)scan_fail(state, CLOPTS_USAGE_ERROR_UNEXPECTED_ARGUMENT, state->pos);
            state->pos++;
            return;
        }
    }
    if (clopts_matches_add_positional(state->matches, scan_text(state)) != 0)
    {
        state->out_of_memory = true;
        return;
    }
    state->pos++;
}

/*
 * Pre-command row for a word, or any token after "--": a command name
 * selects that command; anything else is an unknown command (exact,
 * case-sensitive, no prefix). After "--" no command is selected.
 */
static void prelude_command(scan_state_t *state)
{
    const clopts_command_t *command;

    command = clopts_program_find_command(state->program, scan_text(state));
    if (state->dashdash || (command == NULL))
    {
        (void)scan_fail(state, CLOPTS_USAGE_ERROR_UNKNOWN_COMMAND, state->pos);
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
 * One token of the main scan (in_prelude false) or of the pre-command phase
 * (true). After "--" every token is a positional (an unknown command before
 * the selection); "--" itself; options; everything else, "-" alone
 * included, is a positional (a command name before the selection).
 */
static void scan_token(scan_state_t *state, bool in_prelude)
{
    const clopts_token_t token = clopts_token_read(scan_text(state));

    if (state->dashdash || (token.kind == CLOPTS_TOKEN_WORD))
    {
        if (in_prelude)
        {
            prelude_command(state);
        }
        else
        {
            main_positional(state);
        }
        return;
    }
    switch (token.kind)
    {
    case CLOPTS_TOKEN_DASHDASH:
        state->dashdash = true;
        state->pos++;
        break;
    case CLOPTS_TOKEN_LONG:
        scan_long_option(state, &token);
        break;
    case CLOPTS_TOKEN_SHORT:
        scan_short_option(state, &token);
        break;
    case CLOPTS_TOKEN_WORD:
    default:
        break;
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
        scan_token(state, true);
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
        scan_token(state, false);
    }
}

/*
 * Returns the terminal FLAG that text names exactly in the scope of the scan
 * ("--<long>" with no "=", or "-<c>" alone), or NULL; *found gets where it
 * was found.
 */
static const clopts_option_t *recover_terminal(const scan_state_t *state, const char *text, clopts_lookup_t *found)
{
    const clopts_token_t token = clopts_token_read(text);

    if ((token.kind == CLOPTS_TOKEN_LONG) && (token.value == NULL))
    {
        *found = clopts_lookup_long(state->program, state->command, token.name, token.name_len);
        if (found->negated)
        {
            return NULL;
        }
    }
    else if ((token.kind == CLOPTS_TOKEN_SHORT) && (token.name_len == 1))
    {
        *found = clopts_lookup_short(state->program, state->command, token.name[0]);
    }
    else
    {
        return NULL;
    }
    if ((found->option == NULL) || (clopts_option_get_kind(found->option) != CLOPTS_OPTION_FLAG) ||
        !clopts_option_is_terminal(found->option))
    {
        return NULL;
    }
    return found->option;
}

/*
 * Recovery scan, run when a usage error is pending: the scan stopped right
 * after the first usage error, which stays the only one reported. The tokens
 * from the one of the pending error up to, not including, "--" are searched
 * for an exact "--<long>" or "-<c>" of a terminal FLAG of the scope (the
 * selected command, then the program); the first one found is recorded and
 * turns the outcome into TERMINATED. Nothing else is looked up or recorded:
 * any other token, option values and short groups included, is skipped.
 * When "--" was reached before the error, every remaining token follows
 * "--" and none counts. After the error nothing is interpreted, so an exact
 * token there counts even where it would have been a value ("--bad -o -h"
 * is TERMINATED when -h is terminal).
 */
static void scan_recover(scan_state_t *state)
{
    clopts_lookup_t found;
    const char *text;
    int i;

    assert(state->has_pending && (state->pending_error.argv_index >= 0));
    if (state->dashdash)
    {
        return;
    }
    for (i = state->pending_error.argv_index; i < state->argc; i++)
    {
        text = state->argv[i];
        if (clopts_token_read(text).kind == CLOPTS_TOKEN_DASHDASH)
        {
            return;
        }
        if (recover_terminal(state, text, &found) != NULL)
        {
            scan_flag(state, &found);
            state->has_pending = false;
            return;
        }
    }
}

clopts_scan_status_t clopts_scan(clopts_matches_t *matches, const clopts_program_t *program, int argc,
                                 char *const *argv, const clopts_option_t **terminal, clopts_usage_error_t *error)
{
    scan_state_t state;

    assert(matches != NULL);
    assert(program != NULL);
    assert(terminal != NULL);
    assert(error != NULL);

    /* 2. */
    scan_init(&state, matches, program, argc, argv);
    if (clopts_program_get_command_count(program) > 0)
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
    assert(state.command == clopts_matches_get_command(matches));

    /* 3. */
    *terminal = NULL;
    if (state.out_of_memory)
    {
        return CLOPTS_SCAN_OUT_OF_MEMORY;
    }
    if (state.has_pending)
    {
        scan_recover(&state);
    }
    if (state.has_pending)
    {
        *error = state.pending_error;
        return CLOPTS_SCAN_USAGE_ERROR;
    }
    *terminal = state.terminal;
    return CLOPTS_SCAN_DONE;
}
