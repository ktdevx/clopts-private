/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Scanner: steps 2 and 3 of parsing (parse.c). The scan before the
 * subcommand, the main scan and the recovery scan record into the result
 * through the functions of result.c.
 *
 *   2. scan: the pre-command phase with subcommands, the main scan without;
 *   3. a pending usage error is recorded in the result, unless the recovery
 *      scan finds a terminal option from its token on.
 *
 * The scanner never writes the status of the result, nor to program or argv,
 * and it knows no built-in option or command.
 *
 * Options are looked up by name in the selected command (when there is one)
 * and then in the program, so a name of the command shadows the same global
 * option. In each scope an exact long name comes before --no-<name> of a
 * negatable FLAG. The names themselves are compared by the decl module
 * (decl/names.c).
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/* ---- lookups ---- */

static clopts_parser_ref_t lookup_no_ref(void)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    return ref;
}

/* Returns a ref to the option at index of scope, or no ref when index is the option count. */
static clopts_parser_ref_t lookup_ref_at(const clopts_scope_t *scope, bool global, size_t index)
{
    clopts_parser_ref_t ref = lookup_no_ref();

    if (index < clopts_scope_option_count(scope))
    {
        ref.option = clopts_scope_option_at(scope, index);
        ref.global = global;
        ref.index = index;
    }
    return ref;
}

/*
 * Looks the long name up in scope (global tells which one it is). When it is
 * not found exactly, also looks up --no-<name> of a negatable FLAG.
 */
static clopts_parser_ref_t lookup_long_in(const clopts_scope_t *scope, bool global, const char *name, size_t name_len)
{
    clopts_parser_ref_t ref;

    ref = lookup_ref_at(scope, global, clopts_decl_index_of_long(scope, name, name_len));
    if ((ref.option != NULL) || (name_len <= 3) || (memcmp(name, "no-", 3) != 0))
    {
        return ref;
    }
    ref = lookup_ref_at(scope, global, clopts_decl_index_of_long(scope, name + 3, name_len - 3));
    if ((ref.option == NULL) || (clopts_option_get_kind(ref.option) != CLOPTS_OPTION_FLAG)
        || !clopts_option_is_negatable(ref.option))
    {
        return lookup_no_ref();
    }
    ref.negated = true;
    return ref;
}

/*
 * Looks up the long name given by the name_len bytes at name in selected
 * (the scope of the selected command; NULL when none) and then in global (the
 * scope of the program), so a name of the command shadows the same global
 * option. In each scope the exact long name is looked up first and then,
 * when name starts with "no-", the rest as a negatable FLAG (negated is set).
 */
static clopts_parser_ref_t lookup_long(const clopts_scope_t *global, const clopts_scope_t *selected,
                                       const char *name, size_t name_len)
{
    clopts_parser_ref_t ref;

    assert((global != NULL) && (name != NULL));
    if (selected != NULL)
    {
        ref = lookup_long_in(selected, false, name, name_len);
        if (ref.option != NULL)
        {
            return ref;
        }
    }
    return lookup_long_in(global, true, name, name_len);
}

/* Looks up the short name c in selected (NULL when none) and then in global. */
static clopts_parser_ref_t lookup_short(const clopts_scope_t *global, const clopts_scope_t *selected, char c)
{
    clopts_parser_ref_t ref;

    assert(global != NULL);
    if (c == '\0')
    {
        return lookup_no_ref();
    }
    if (selected != NULL)
    {
        ref = lookup_ref_at(selected, false, clopts_decl_index_of_short(selected, c));
        if (ref.option != NULL)
        {
            return ref;
        }
    }
    return lookup_ref_at(global, true, clopts_decl_index_of_short(global, c));
}

/* ---- scan ---- */

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
    /* Selected command (kept equal to the command of the result), or NULL. */
    const clopts_command_t *command;
    /* A usage error is pending in pending_error (only the first one is kept). */
    bool has_pending;
    clopts_parser_error_t pending_error;
    /* "--" was reached: every following token is a positional. */
    bool dashdash;
    /* The terminal option that ended the scan, or NULL. */
    const clopts_option_t *terminal;
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
    state->terminal = NULL;
    state->out_of_memory = false;
}

/* The scope of the program: the global options, and the positionals of a program without subcommands. */
static const clopts_scope_t *scan_global(const scan_state_t *state)
{
    return clopts_program_scope(state->program);
}

/* The scope of the selected command, or NULL before the selection. */
static const clopts_scope_t *scan_selected(const scan_state_t *state)
{
    return (state->command != NULL) ? clopts_command_scope(state->command) : NULL;
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
static clopts_parser_error_t *scan_fail(scan_state_t *state, clopts_usage_error_t usage, int index)
{
    clopts_parser_error_t *error = &state->pending_error;

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
 * parse): allocates its slots and sets the command of the result, which
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

/*
 * Records a FLAG found through ref (negated or not) given by argv. A
 * terminal one ends the scan, unless it was given as --no-<name>.
 */
static void scan_flag(scan_state_t *state, const clopts_parser_ref_t *ref)
{
    clopts_parser_slot_set_flag(clopts_parser_result_slot(state->result, ref), !ref->negated, CLOPTS_SOURCE_EXPLICIT);
    if (clopts_option_is_terminal(ref->option) && !ref->negated)
    {
        state->terminal = ref->option;
    }
}

/*
 * Records the value of a VALUE / LIST option found through ref: value is the
 * text after "=" (long) or the rest of a short group, or NULL to consume the
 * next token verbatim. No next token is a missing value: the error recorded
 * on argv[index] is returned so the caller can complete it, or NULL when
 * none was recorded. VALUE keeps the last one given; LIST appends the split
 * items.
 */
static clopts_parser_error_t *scan_option_value(scan_state_t *state, const clopts_parser_ref_t *ref,
                                                const char *value, int index)
{
    clopts_parser_slot_t *slot = clopts_parser_result_slot(state->result, ref);
    clopts_parser_error_t *error;

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
    if (clopts_option_get_kind(ref->option) == CLOPTS_OPTION_VALUE)
    {
        clopts_parser_slot_set_value(slot, value, CLOPTS_SOURCE_EXPLICIT);
    }
    else if (clopts_parser_slot_append_list(slot, value, clopts_option_get_delimiter(ref->option),
                                            CLOPTS_SOURCE_EXPLICIT) != 0)
    {
        state->out_of_memory = true;
    }
    return NULL;
}

/*
 * Long option token "--<name>" / "--<name>=<value>". <name> ends at the
 * first "=". The name is looked up in the selected command (if any) and then
 * in the global options; in each, exactly first and then as --no-<name> of a
 * negatable FLAG. FLAG / COUNT take no value; VALUE / LIST take the text
 * after "=" or the next token.
 */
static void scan_long_option(scan_state_t *state)
{
    const int index = state->pos;
    const char *name = scan_token(state) + 2;
    const size_t name_len = strcspn(name, "=");
    const char *value = (name[name_len] == '=') ? (name + name_len + 1) : NULL;
    clopts_parser_ref_t ref;
    clopts_parser_error_t *error;

    state->pos++;
    ref = lookup_long(scan_global(state), scan_selected(state), name, name_len);
    if (ref.option == NULL)
    {
        error = scan_fail(state, CLOPTS_USAGE_UNKNOWN_OPTION, index);
        if (error != NULL)
        {
            error->name_len = 2 + name_len;
        }
        return;
    }

    switch (clopts_option_get_kind(ref.option))
    {
    case CLOPTS_OPTION_FLAG:
    case CLOPTS_OPTION_COUNT:
        if (value != NULL)
        {
            error = scan_fail(state, CLOPTS_USAGE_UNEXPECTED_VALUE, index);
            if (error != NULL)
            {
                error->option = ref.option;
            }
        }
        else if (clopts_option_get_kind(ref.option) == CLOPTS_OPTION_FLAG)
        {
            scan_flag(state, &ref);
        }
        else
        {
            clopts_parser_slot_add_count(clopts_parser_result_slot(state->result, &ref), clopts_option_get_count_max(ref.option));
        }
        break;
    case CLOPTS_OPTION_VALUE:
    case CLOPTS_OPTION_LIST:
        (void)scan_option_value(state, &ref, value, index);
        break;
    }
}

/*
 * Short option token "-<c1><c2>...", one character at a time against the
 * selected command (if any) and the global options: an unknown character is
 * an unknown option on that character; FLAG / COUNT go on with the next
 * character; VALUE / LIST take the rest of the token verbatim ("=" included)
 * when it is not empty, else the next token verbatim, and end the token. A
 * terminal FLAG or a usage error ends the token as well.
 */
static void scan_short_option(scan_state_t *state)
{
    const int index = state->pos;
    const char *chars = scan_token(state) + 1;
    clopts_parser_ref_t ref;
    clopts_parser_error_t *error;
    size_t i;

    state->pos++;
    for (i = 0; chars[i] != '\0'; i++)
    {
        ref = lookup_short(scan_global(state), scan_selected(state), chars[i]);
        if (ref.option == NULL)
        {
            error = scan_fail(state, CLOPTS_USAGE_UNKNOWN_OPTION, index);
            if (error != NULL)
            {
                error->short_name = chars[i];
            }
            return;
        }
        switch (clopts_option_get_kind(ref.option))
        {
        case CLOPTS_OPTION_FLAG:
            scan_flag(state, &ref);
            if (state->terminal != NULL)
            {
                return;
            }
            break;
        case CLOPTS_OPTION_COUNT:
            clopts_parser_slot_add_count(clopts_parser_result_slot(state->result, &ref), clopts_option_get_count_max(ref.option));
            break;
        case CLOPTS_OPTION_VALUE:
        case CLOPTS_OPTION_LIST:
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
 * positionals of the scope in order (the selected command, or the program
 * itself when it has no subcommands); a variadic last positional takes every
 * remaining one. A token beyond the declared ones without a variadic last
 * positional is an unexpected argument. Missing required positionals are
 * reported later by the required check.
 */
static void main_positional(scan_state_t *state)
{
    const clopts_scope_t *scope = (state->command != NULL) ? scan_selected(state) : scan_global(state);
    const size_t declared = clopts_scope_positional_count(scope);
    const size_t taken = clopts_parser_result_get_positional_count(state->result);
    const clopts_positional_t *last;

    if (taken >= declared)
    {
        last = (declared == 0)
                   ? NULL
                   : clopts_scope_positional_at(scope, declared - 1);
        if ((last == NULL) || !clopts_positional_is_variadic(last))
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
 * Pre-command row for a token that does not start with "-" (or is "-" alone,
 * or follows "--"): a command name selects that command; anything else is an
 * unknown command (exact, case-sensitive, no prefix).
 */
static void prelude_command(scan_state_t *state)
{
    const clopts_command_t *command;

    command = clopts_program_find_command(state->program, scan_token(state));
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
 * One token of the pre-command phase: after "--" every token is a
 * positional, which is an unknown command since no command is selected;
 * "--"; other "-" tokens except "-" alone (options); then command names.
 */
static void prelude_token(scan_state_t *state)
{
    const char *token = scan_token(state);

    if (state->dashdash)
    {
        prelude_command(state);
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
 * Returns the terminal FLAG that token names exactly in the scope of the
 * scan ("--<long>" with no "=", or "-<c>" alone), or NULL; *ref gets where
 * it was found.
 */
static const clopts_option_t *recover_terminal(const scan_state_t *state, const char *token, clopts_parser_ref_t *ref)
{
    if ((token[0] != '-') || (token[1] == '\0'))
    {
        return NULL;
    }
    if (token[1] == '-')
    {
        if ((token[2] == '\0') || (strchr(token + 2, '=') != NULL))
        {
            return NULL;
        }
        *ref = lookup_long(scan_global(state), scan_selected(state), token + 2, strlen(token + 2));
        if (ref->negated)
        {
            return NULL;
        }
    }
    else
    {
        if (token[2] != '\0')
        {
            return NULL;
        }
        *ref = lookup_short(scan_global(state), scan_selected(state), token[1]);
    }
    if ((ref->option == NULL) || (clopts_option_get_kind(ref->option) != CLOPTS_OPTION_FLAG) || !clopts_option_is_terminal(ref->option))
    {
        return NULL;
    }
    return ref->option;
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
    clopts_parser_ref_t ref;
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
        if (recover_terminal(state, token, &ref) != NULL)
        {
            scan_flag(state, &ref);
            state->has_pending = false;
            return;
        }
    }
}

clopts_parser_scan_status_t clopts_parser_scan(clopts_result_t *result, const clopts_program_t *program,
                                               int argc, char *const *argv, const clopts_option_t **terminal)
{
    scan_state_t state;

    assert(result != NULL);
    assert(program != NULL);
    assert(terminal != NULL);

    /* 2. */
    scan_init(&state, program, argc, argv, result);
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
    assert(state.command == clopts_result_get_command(result));

    /* 3. */
    *terminal = NULL;
    if (state.out_of_memory)
    {
        return CLOPTS_PARSER_SCAN_OUT_OF_MEMORY;
    }
    if (state.has_pending)
    {
        scan_recover(&state);
    }
    if (state.has_pending)
    {
        clopts_parser_result_set_error(result, &state.pending_error);
        return CLOPTS_PARSER_SCAN_USAGE_ERROR;
    }
    *terminal = state.terminal;
    return CLOPTS_PARSER_SCAN_DONE;
}
