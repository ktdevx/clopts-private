/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Resolution: the environment and the defaults (step 4 of parsing,
 * parse.c). The required checks that follow are in require.c. The values
 * come from a lookup callback only; which source it reads (the process
 * environment or the caller's) is decided by parse.c.
 *
 * A lookup callback lends its string: it stays valid only until the next
 * call of the callback. The resolver copies a found string through the
 * allocation gateway right after the callback returns, before any further
 * call, and never reads the borrowed string afterwards.
 */

#include "parse/resolve.h"

#include "base/mem.h"

#include <assert.h>

/* What one resolution works with. */
typedef struct resolve_state
{
    clopts_matches_t *matches;
    clopts_usage_error_t *error;            /* where an INVALID_ENV_VALUE error goes */
    clopts_env_fn env_fn;
    void *env_ctx;
    bool invalid;                           /* an INVALID_ENV_VALUE error was recorded */
} resolve_state_t;

/* Records the first environment value that cannot be read, on option; later ones are not reported. */
static void resolve_invalid(resolve_state_t *state, const clopts_option_t *option)
{
    if (state->invalid)
    {
        return;
    }
    clopts_usage_error_init(state->error, CLOPTS_USAGE_ERROR_INVALID_ENV_VALUE);
    state->error->option = option;
    state->invalid = true;
}

/*
 * Records the value borrowed from the lookup for option, copying it before
 * the callback is called again (the loan ends there). Returns 0 or -1.
 */
static int resolve_env_value(clopts_slot_t *slot, const clopts_option_t *option, const char *borrowed)
{
    char *copy;
    int rc;

    copy = clopts_mem_strdup(borrowed);
    if (copy == NULL)
    {
        return -1;
    }
    if (clopts_option_get_kind(option) == CLOPTS_OPTION_VALUE)
    {
        clopts_slot_set_owned_value(slot, copy, CLOPTS_SOURCE_ENV);
        return 0;
    }
    rc = clopts_slot_append_list(slot, copy, clopts_option_get_delimiter(option), CLOPTS_SOURCE_ENV);
    /* The items are copies of their own; the whole string is no longer needed. */
    clopts_mem_free(copy);
    return rc;
}

/*
 * Resolves the slot of option when argv gave it nothing: the environment
 * first, then the default value. A value that cannot be read leaves the slot
 * unset (the default is not used) and is recorded as an error. Returns 0, or
 * -1 when the lookup fails or a copy cannot be allocated.
 */
static int resolve_slot(resolve_state_t *state, clopts_slot_t *slot, const clopts_option_t *option)
{
    clopts_option_kind_t kind = clopts_option_get_kind(option);
    const char *env = clopts_option_get_env(option);
    const char *default_value = clopts_option_get_default(option);

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

        switch (state->env_fn(state->env_ctx, env, &borrowed))
        {
        case CLOPTS_ENV_FOUND:
            assert((borrowed != NULL) && "a lookup that reports FOUND must set *value");
            return (borrowed != NULL) ? resolve_env_value(slot, option, borrowed) : -1;
        case CLOPTS_ENV_INVALID:
            resolve_invalid(state, option);
            return 0;
        case CLOPTS_ENV_UNSET:
            break;
        case CLOPTS_ENV_FAILED:
        default:
            return -1;
        }
    }

    if (default_value != NULL)
    {
        if (kind == CLOPTS_OPTION_VALUE)
        {
            /* Borrows the model's copy, which outlives the result's use of it. */
            clopts_slot_set_value(slot, default_value, CLOPTS_SOURCE_DEFAULT);
            return 0;
        }
        return clopts_slot_append_list(slot, default_value, clopts_option_get_delimiter(option),
                                       CLOPTS_SOURCE_DEFAULT);
    }
    return 0;
}

/* Resolves the slots of the options of scope in declaration order. Returns 0 or -1. */
static int resolve_scope(resolve_state_t *state, clopts_slot_t *slots, size_t slot_count,
                         const clopts_scope_t *scope)
{
    size_t i;

    assert(slot_count == clopts_scope_get_option_count(scope));
    for (i = 0; i < slot_count; i++)
    {
        if (resolve_slot(state, &slots[i], clopts_scope_get_option_at(scope, i)) != 0)
        {
            return -1;
        }
    }
    return 0;
}

/* Resolves the global options, then those of the selected command when include_command. Returns 0 or -1. */
static int resolve_all(resolve_state_t *state, const clopts_program_t *program, bool include_command)
{
    const clopts_command_t *command = clopts_matches_get_command(state->matches);
    clopts_slot_t *slots;
    size_t slot_count;

    slots = clopts_matches_get_slots(state->matches, true, &slot_count);
    if (resolve_scope(state, slots, slot_count, clopts_program_get_scope(program)) != 0)
    {
        return -1;
    }
    if (include_command && (command != NULL))
    {
        slots = clopts_matches_get_slots(state->matches, false, &slot_count);
        if (resolve_scope(state, slots, slot_count, clopts_command_get_scope(command)) != 0)
        {
            return -1;
        }
    }
    return 0;
}

clopts_resolve_status_t clopts_resolve(clopts_matches_t *matches, const clopts_program_t *program,
                                       clopts_env_fn env_fn, void *env_ctx, bool include_command,
                                       clopts_usage_error_t *error)
{
    resolve_state_t state;
    int rc;

    assert(matches != NULL);
    assert(program != NULL);
    assert(env_fn != NULL);
    assert(error != NULL);

    state.matches = matches;
    state.error = error;
    state.env_fn = env_fn;
    state.env_ctx = env_ctx;
    state.invalid = false;

    rc = resolve_all(&state, program, include_command);
    if (rc != 0)
    {
        return CLOPTS_RESOLVE_OUT_OF_MEMORY;
    }
    return state.invalid ? CLOPTS_RESOLVE_INVALID : CLOPTS_RESOLVE_DONE;
}
