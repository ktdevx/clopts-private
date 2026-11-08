/*
 * Resolver: environment variable fallback, default values, required checks
 * and the default environment lookup.
 *
 * getenv is called only from clopts_parser_getenv_default, whose result is
 * copied at once (its storage may be invalidated by later getenv / putenv
 * calls). MSVC's deprecation warning for getenv is silenced by the PRIVATE
 * _CRT_SECURE_NO_WARNINGS definition of the clopts target.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

clopts_env_status_t clopts_parser_getenv_default(void *ctx, const char *name, char **out)
{
    const char *value;
    size_t len;
    size_t bytes;
    char *copy;

    (void)ctx;
    assert(name != NULL);
    assert(out != NULL);

    value = getenv(name);
    if (value == NULL)
    {
        return CLOPTS_ENV_UNSET;
    }
    len = strlen(value);
    if (clopts_core_size_add(len, 1, &bytes) != 0)
    {
        return CLOPTS_ENV_FAILED;
    }
    copy = malloc(bytes);
    if (copy == NULL)
    {
        return CLOPTS_ENV_FAILED;
    }
    memcpy(copy, value, bytes);
    *out = copy;
    return CLOPTS_ENV_FOUND;
}

/*
 * Resolves one slot of option that got nothing from argv: the environment
 * first, then default_value. Returns 0 on success and -1 when the lookup or
 * an allocation fails.
 */
static int resolve_slot(clopts_parser_slot_t *slot, const clopts_option_t *option,
                        clopts_getenv_fn getenv_fn, void *env_ctx)
{
    if ((option->kind != CLOPTS_VALUE) && (option->kind != CLOPTS_LIST))
    {
        return 0;
    }
    if (slot->source != CLOPTS_SOURCE_NONE)
    {
        return 0;
    }

    if (option->env != NULL)
    {
        char *found = NULL;
        clopts_env_status_t status = getenv_fn(env_ctx, option->env, &found);

        if (status == CLOPTS_ENV_FAILED)
        {
            return -1;
        }
        if (status == CLOPTS_ENV_FOUND)
        {
            assert(found != NULL);
            if (option->kind == CLOPTS_VALUE)
            {
                clopts_parser_store_own(slot, found);
                clopts_parser_store_set_value(slot, found, CLOPTS_SOURCE_ENV);
                return 0;
            }
            else
            {
                int rc = clopts_parser_store_append_list(slot, found, option->list_delimiter,
                                                         CLOPTS_SOURCE_ENV);

                free(found);
                return rc;
            }
        }
    }

    if (option->default_value != NULL)
    {
        if (option->kind == CLOPTS_VALUE)
        {
            clopts_parser_store_set_value(slot, option->default_value, CLOPTS_SOURCE_DEFAULT);
            return 0;
        }
        return clopts_parser_store_append_list(slot, option->default_value,
                                               option->list_delimiter, CLOPTS_SOURCE_DEFAULT);
    }
    return 0;
}

/* Resolves the count slots of options in order. Returns 0 or -1. */
static int resolve_scope(clopts_parser_slot_t *slots, const clopts_option_t *options,
                         size_t count, clopts_getenv_fn getenv_fn, void *env_ctx)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (resolve_slot(&slots[i], &options[i], getenv_fn, env_ctx) != 0)
        {
            return -1;
        }
    }
    return 0;
}

int clopts_parser_resolve(clopts_result_t *result, clopts_getenv_fn getenv_fn, void *env_ctx,
                          bool include_command)
{
    clopts_parser_store_t *store;

    assert(result != NULL);
    store = result->impl;
    assert(store != NULL);

    if (getenv_fn == NULL)
    {
        getenv_fn = clopts_parser_getenv_default;
    }

    if (resolve_scope(store->global_slots, store->program->options,
                      store->program->option_count, getenv_fn, env_ctx) != 0)
    {
        return -1;
    }
    if (include_command && (store->command != NULL))
    {
        if (resolve_scope(store->command_slots, store->command->options,
                          store->command->option_count, getenv_fn, env_ctx) != 0)
        {
            return -1;
        }
    }
    return 0;
}

/*
 * Fills result->error for a missing required option (positional == NULL) or
 * positional (option == NULL).
 */
static void set_missing(clopts_result_t *result, const clopts_command_t *command,
                        const clopts_option_t *option, const clopts_positional_t *positional)
{
    clopts_error_t error;

    memset(&error, 0, sizeof(error));
    error.usage = (option != NULL) ? CLOPTS_USAGE_MISSING_OPTION : CLOPTS_USAGE_MISSING_ARGUMENT;
    error.command = command;
    error.option = option;
    error.positional = positional;
    error.token = NULL;
    error.argv_index = -1;
    error.name_len = 0;
    error.short_name = '\0';
    result->error = error;
}

/* Returns the first required option of options whose slot got no value, or NULL. */
static const clopts_option_t *first_missing_option(const clopts_parser_slot_t *slots,
                                                   const clopts_option_t *options, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (options[i].required && (slots[i].source == CLOPTS_SOURCE_NONE))
        {
            return &options[i];
        }
    }
    return NULL;
}

int clopts_parser_check_required(clopts_result_t *result, bool include_command)
{
    clopts_parser_store_t *store;
    const clopts_command_t *command;
    const clopts_option_t *missing;
    const clopts_positional_t *positionals;
    size_t positional_count;
    size_t recorded;
    size_t i;

    assert(result != NULL);
    store = result->impl;
    assert(store != NULL);

    command = include_command ? store->command : NULL;

    missing = first_missing_option(store->global_slots, store->program->options,
                                   store->program->option_count);
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
        missing = first_missing_option(store->command_slots, command->options,
                                       command->option_count);
        if (missing != NULL)
        {
            set_missing(result, command, missing, NULL);
            return -1;
        }
        positionals = command->positionals;
        positional_count = command->positional_count;
    }
    else if (store->program->command_count == 0)
    {
        positionals = store->program->positionals;
        positional_count = store->program->positional_count;
    }
    else
    {
        return 0;
    }

    /*
     * Positionals are assigned in order and required ones precede optional
     * ones, so the i-th positional got a token exactly when i < recorded. A
     * required variadic positional therefore needs at least one token.
     */
    recorded = store->positionals.len;
    for (i = 0; i < positional_count; i++)
    {
        if (positionals[i].required && (i >= recorded))
        {
            set_missing(result, command, NULL, &positionals[i]);
            return -1;
        }
    }
    return 0;
}
