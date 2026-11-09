/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Resolver: the default environment lookup, resolution from the environment
 * and the defaults, and the required checks.
 *
 * A lookup callback lends its string: it stays valid only until the next
 * call of the callback. The resolver copies a found string through the
 * allocation gateway right after the callback returns, before any further
 * call, and never reads the borrowed string afterwards. getenv is called only
 * from clopts_parser_getenv_default; MSVC's deprecation warning for it is
 * silenced by the PRIVATE _CRT_SECURE_NO_WARNINGS definition of the target.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <stdlib.h>

clopts_env_status_t clopts_parser_getenv_default(void *ctx, const char *name, const char **value)
{
    const char *found;

    (void)ctx;
    assert(name != NULL);
    assert(value != NULL);

    found = getenv(name);
    if (found == NULL)
    {
        return CLOPTS_ENV_UNSET;
    }
    *value = found;
    return CLOPTS_ENV_FOUND;
}

/*
 * Resolves the slot of option when argv gave it nothing: the environment
 * first, then the default value. Returns 0, or -1 when the lookup fails or a
 * copy cannot be allocated (the default is not used then).
 */
static int resolve_slot(clopts_parser_slot_t *slot, const clopts_option_t *option,
                        clopts_getenv_fn getenv_fn, void *env_ctx)
{
    clopts_option_kind_t kind = clopts_option_kind_of(option);
    const char *env = clopts_option_env(option);
    const char *default_value = clopts_option_default(option);
    char delimiter = clopts_option_delimiter(option);

    if ((kind != CLOPTS_OPTION_VALUE) && (kind != CLOPTS_OPTION_LIST))
    {
        return 0;
    }
    if (slot->source != CLOPTS_SOURCE_NONE)
    {
        return 0;
    }

    if (env != NULL)
    {
        const char *borrowed = NULL;
        clopts_env_status_t status = getenv_fn(env_ctx, env, &borrowed);

        if (status == CLOPTS_ENV_FAILED)
        {
            return -1;
        }
        if (status == CLOPTS_ENV_FOUND)
        {
            char *copy;

            assert((borrowed != NULL) && "a lookup that reports FOUND must set *value");
            if (borrowed == NULL)
            {
                return -1;
            }
            /* Copy before the callback is called again; the loan ends there. */
            copy = clopts_core_strdup(borrowed);
            if (copy == NULL)
            {
                return -1;
            }
            if (kind == CLOPTS_OPTION_VALUE)
            {
                clopts_parser_slot_own(slot, copy);
                clopts_parser_slot_set_value(slot, copy, CLOPTS_SOURCE_ENV);
                return 0;
            }
            else
            {
                int rc = clopts_parser_slot_append_list(slot, copy, delimiter, CLOPTS_SOURCE_ENV);

                /* The items are copies of their own; the whole string is no longer needed. */
                clopts_core_free(copy);
                return rc;
            }
        }
    }

    if (default_value != NULL)
    {
        if (kind == CLOPTS_OPTION_VALUE)
        {
            /* Borrows the model's copy, which outlives the result's use of it. */
            clopts_parser_slot_set_value(slot, default_value, CLOPTS_SOURCE_DEFAULT);
            return 0;
        }
        return clopts_parser_slot_append_list(slot, default_value, delimiter, CLOPTS_SOURCE_DEFAULT);
    }
    return 0;
}

/* Resolves the slots of the options of scope in declaration order. Returns 0 or -1. */
static int resolve_scope(clopts_parser_slot_t *slots, size_t slot_count,
                         const clopts_command_t *scope, clopts_getenv_fn getenv_fn, void *env_ctx)
{
    size_t i;

    assert(slot_count == clopts_command_option_count(scope));
    for (i = 0; i < slot_count; i++)
    {
        const clopts_option_t *option = clopts_command_option_at(scope, i);

        if (resolve_slot(&slots[i], option, getenv_fn, env_ctx) != 0)
        {
            return -1;
        }
    }
    return 0;
}

int clopts_parser_resolve(clopts_result_t *result, const clopts_program_t *program,
                          clopts_getenv_fn getenv_fn, void *env_ctx, bool include_command)
{
    assert(result != NULL);
    assert(result != clopts_parser_result_oom());
    assert(program != NULL);

    if (getenv_fn == NULL)
    {
        getenv_fn = clopts_parser_getenv_default;
    }

    if (resolve_scope(result->global_slots, result->global_count, clopts_program_root(program),
                      getenv_fn, env_ctx) != 0)
    {
        return -1;
    }
    if (include_command && (result->command != NULL))
    {
        if (resolve_scope(result->command_slots, result->command_count, result->command,
                          getenv_fn, env_ctx) != 0)
        {
            return -1;
        }
    }
    return 0;
}

/*
 * Records a missing required option (positional == NULL) or positional
 * (option == NULL) in result->error. result->status is left to the scanner.
 */
static void set_missing(clopts_result_t *result, const clopts_command_t *command,
                        const clopts_option_t *option, const clopts_positional_t *positional)
{
    result->error.usage = (option != NULL) ? CLOPTS_USAGE_MISSING_OPTION : CLOPTS_USAGE_MISSING_ARGUMENT;
    result->error.command = command;
    result->error.option = option;
    result->error.positional = positional;
    result->error.token = NULL;
    result->error.argv_index = -1;
    result->error.name_len = 0;
    result->error.short_name = '\0';
}

/* Returns true when slot got a value that satisfies required (from argv or the environment). */
static bool slot_given(const clopts_parser_slot_t *slot)
{
    return ((slot->source == CLOPTS_SOURCE_EXPLICIT) || (slot->source == CLOPTS_SOURCE_ENV));
}

/* Returns the first required option of scope whose slot was not given, or NULL. */
static const clopts_option_t *first_missing_option(const clopts_parser_slot_t *slots, size_t slot_count,
                                                   const clopts_command_t *scope)
{
    size_t i;

    assert(slot_count == clopts_command_option_count(scope));
    for (i = 0; i < slot_count; i++)
    {
        const clopts_option_t *option = clopts_command_option_at(scope, i);

        if (clopts_option_required(option) && !slot_given(&slots[i]))
        {
            return option;
        }
    }
    return NULL;
}

int clopts_parser_check_required(clopts_result_t *result, const clopts_program_t *program, bool include_command)
{
    const clopts_command_t *command;
    const clopts_command_t *positional_scope;
    const clopts_option_t *missing;
    size_t recorded;
    size_t i;

    assert(result != NULL);
    assert(result != clopts_parser_result_oom());
    assert(program != NULL);

    command = include_command ? result->command : NULL;

    missing = first_missing_option(result->global_slots, result->global_count, clopts_program_root(program));
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
        missing = first_missing_option(result->command_slots, result->command_count, command);
        if (missing != NULL)
        {
            set_missing(result, command, missing, NULL);
            return -1;
        }
        positional_scope = command;
    }
    else if (clopts_program_command_count(program) == 0)
    {
        positional_scope = clopts_program_root(program);
    }
    else
    {
        /* NO_COMMAND: only the root options are checked. */
        return 0;
    }

    /*
     * Positionals are assigned in order and required ones precede optional
     * ones, so the i-th positional got a token exactly when i < recorded. A
     * required variadic positional therefore needs at least one token.
     */
    recorded = result->positionals.len;
    for (i = 0; i < clopts_command_positional_count(positional_scope); i++)
    {
        const clopts_positional_t *positional =
            clopts_command_positional_at(positional_scope, i);

        if (clopts_positional_required(positional) && (i >= recorded))
        {
            set_missing(result, command, NULL, positional);
            return -1;
        }
    }
    return 0;
}
