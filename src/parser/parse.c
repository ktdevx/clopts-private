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
 *   6. check required ones (below);
 *   7. OK, or NO_COMMAND when the program has subcommands and none was
 *      selected.
 *
 * Without a lookup callback, the process environment is read through the
 * process module (clopts_env_lookup), with a lookup context that
 * lives on the stack of parse() for this parse only. This is the only place
 * in the parser that knows the process; the steps themselves see a lookup
 * callback only.
 *
 * Allocation failures and lookups that report FAILED after the result exists
 * end with CLOPTS_PARSE_OUT_OF_MEMORY on that result, which stays releasable with
 * clopts_result_destroy(). This file writes the final status of the result;
 * nothing else in the parse does.
 */

#include "parser/parser_internal.h"
#include "process/env.h"

#include <assert.h>

/* ---- required checks ---- */

/*
 * Records a missing required option (positional == NULL) or positional
 * (option == NULL) as the error of result.
 */
static void set_missing(clopts_result_t *result, const clopts_command_t *command,
                        const clopts_option_t *option, const clopts_positional_t *positional)
{
    clopts_parser_error_t error;

    error.usage = (option != NULL) ? CLOPTS_USAGE_MISSING_OPTION : CLOPTS_USAGE_MISSING_ARGUMENT;
    error.command = command;
    error.option = option;
    error.positional = positional;
    error.token = NULL;
    error.argv_index = -1;
    error.name_len = 0;
    error.short_name = '\0';
    clopts_parser_result_set_error(result, &error);
}

/* Returns true when slot got a value that satisfies required (from argv or the environment). */
static bool slot_given(const clopts_parser_slot_t *slot)
{
    return ((slot->source == CLOPTS_SOURCE_EXPLICIT) || (slot->source == CLOPTS_SOURCE_ENV));
}

/* Returns the first required option of scope whose slot was not given, or NULL. */
static const clopts_option_t *first_missing_option(const clopts_parser_slot_t *slots, size_t slot_count,
                                                   const clopts_scope_t *scope)
{
    size_t i;

    assert(slot_count == clopts_scope_get_option_count(scope));
    for (i = 0; i < slot_count; i++)
    {
        const clopts_option_t *option = clopts_scope_get_option_at(scope, i);

        if (clopts_option_is_required(option) && !slot_given(&slots[i]))
        {
            return option;
        }
    }
    return NULL;
}

int clopts_parser_check_required(clopts_result_t *result, const clopts_program_t *program, bool include_command)
{
    const clopts_command_t *command;
    const clopts_scope_t *positional_scope;
    const clopts_option_t *missing;
    const clopts_parser_slot_t *slots;
    size_t slot_count;
    size_t recorded;
    size_t i;

    assert(result != NULL);
    assert(result != clopts_parser_result_oom());
    assert(program != NULL);

    command = include_command ? clopts_result_get_command(result) : NULL;

    slots = clopts_parser_result_get_slots(result, true, &slot_count);
    missing = first_missing_option(slots, slot_count, clopts_program_get_scope(program));
    if (missing != NULL)
    {
        set_missing(result, command, missing, NULL);
        return -1;
    }
    if (!include_command)
    {
        return 0;
    }

    if (command != NULL)
    {
        slots = clopts_parser_result_get_slots(result, false, &slot_count);
        missing = first_missing_option(slots, slot_count, clopts_command_get_scope(command));
        if (missing != NULL)
        {
            set_missing(result, command, missing, NULL);
            return -1;
        }
        positional_scope = clopts_command_get_scope(command);
    }
    else if (clopts_program_get_command_count(program) == 0)
    {
        positional_scope = clopts_program_get_scope(program);
    }
    else
    {
        /* NO_COMMAND: only the global options are checked. */
        return 0;
    }

    /*
     * Positionals are assigned in order and required ones precede optional
     * ones, so the i-th positional got a token exactly when i < recorded. A
     * required variadic positional therefore needs at least one token.
     */
    recorded = clopts_parser_result_get_positional_count(result);
    for (i = 0; i < clopts_scope_get_positional_count(positional_scope); i++)
    {
        const clopts_positional_t *positional = clopts_scope_get_positional_at(positional_scope, i);

        if (clopts_positional_is_required(positional) && (i >= recorded))
        {
            set_missing(result, command, NULL, positional);
            return -1;
        }
    }
    return 0;
}

/* ---- procedure ---- */

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

/*
 * Shared body of clopts_parse() and clopts_parse_with_env(). Without getenv_fn
 * (NULL), the process environment is the source of the environment values.
 */
static clopts_result_t *parse(const clopts_program_t *program, int argc, char *const *argv, clopts_getenv_fn getenv_fn,
                              void *env_ctx)
{
    clopts_result_t *result;
    clopts_env_t process_env;

    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    /* 1. */
    result = parse_enter(program);
    if (result == clopts_parser_result_oom())
    {
        return result;
    }

    if (getenv_fn != NULL)
    {
        parse_run(program, argc, argv, getenv_fn, env_ctx, result);
        return result;
    }

    clopts_env_init(&process_env);
    parse_run(program, argc, argv, clopts_env_lookup, &process_env, result);
    clopts_env_release(&process_env);
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
