/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Resolver: the default environment lookup and resolution from the
 * environment and the defaults (step 4 of parsing, parse.c). The required
 * checks that follow are in check.c.
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
    clopts_option_kind_t kind = clopts_option_get_kind(option);
    const char *env = clopts_option_get_env(option);
    const char *default_value = clopts_option_get_default(option);
    char delimiter = clopts_option_get_delimiter(option);

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
                clopts_parser_slot_set_owned_value(slot, copy, CLOPTS_SOURCE_ENV);
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
                         const clopts_scope_t *scope, clopts_getenv_fn getenv_fn, void *env_ctx)
{
    size_t i;

    assert(slot_count == clopts_scope_option_count(scope));
    for (i = 0; i < slot_count; i++)
    {
        const clopts_option_t *option = clopts_scope_option_at(scope, i);

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
    const clopts_command_t *command;
    clopts_parser_slot_t *slots;
    size_t slot_count;

    assert(result != NULL);
    assert(result != clopts_parser_result_oom());
    assert(program != NULL);

    if (getenv_fn == NULL)
    {
        getenv_fn = clopts_parser_getenv_default;
    }

    slots = clopts_parser_result_get_slots(result, true, &slot_count);
    if (resolve_scope(slots, slot_count, clopts_program_scope(program), getenv_fn, env_ctx) != 0)
    {
        return -1;
    }
    command = clopts_result_get_command(result);
    if (include_command && (command != NULL))
    {
        slots = clopts_parser_result_get_slots(result, false, &slot_count);
        if (resolve_scope(slots, slot_count, clopts_command_scope(command), getenv_fn, env_ctx) != 0)
        {
            return -1;
        }
    }
    return 0;
}
