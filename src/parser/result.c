/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Result handle: allocation, the out-of-memory sentinel, recording into
 * slots, the accessors and release.
 *
 * Every allocation goes through the allocation gateway of core. Whatever
 * was allocated is linked to the result at once, so clopts_result_destroy()
 * releases it even after a failure. A result does not point to its
 * program: every slot knows its option, and an accessor finds the slot of an
 * option by its address, searching the global slots and then the slots of
 * the selected command (an option does not know its index).
 * clopts_result_destroy() never reads the declaration, so the result may be
 * released after the program.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

/*
 * A result. Its layout is private to this file: the rest of the parser and
 * the tests read and record into it through the functions of this file.
 */
struct clopts_result
{
    clopts_parse_status_t status;
    const clopts_command_t *command;  /* selected command, or NULL */
    const clopts_option_t *terminal;  /* TERMINATED: the terminal option, else NULL */
    clopts_parser_error_t error;
    clopts_parser_slot_t *global_slots;   /* one per global option */
    size_t global_count;
    clopts_parser_slot_t *command_slots;  /* one per option of the selected command; NULL without one */
    size_t command_count;
    clopts_vec_t positionals;    /* const char * elements pointing into argv */
};

/*
 * The out-of-memory sentinel. Never written to; shared between threads
 * safely because it is a const static object.
 */
static const struct clopts_result parser_result_sentinel = {
    .status = CLOPTS_PARSE_OUT_OF_MEMORY,
    .command = NULL,
    .terminal = NULL,
    .error = {
        .usage = CLOPTS_USAGE_NONE,
        .command = NULL,
        .option = NULL,
        .positional = NULL,
        .token = NULL,
        .argv_index = -1,
        .name_len = 0,
        .short_name = '\0'
    },
    .global_slots = NULL,
    .global_count = 0,
    .command_slots = NULL,
    .command_count = 0,
    .positionals = { .data = NULL, .len = 0, .cap = 0, .elem_size = 0 }
};

/* ---- internal API ---- */

/*
 * Allocates one slot per option of scope, in the unset state, through the
 * gateway. No options give NULL without allocating. Returns 0, or -1 leaving
 * *out unchanged.
 */
static int alloc_slots(const clopts_scope_t *scope, clopts_parser_slot_t **out)
{
    const size_t count = clopts_scope_get_option_count(scope);
    clopts_parser_slot_t *slots;
    size_t i;

    if (count == 0)
    {
        *out = NULL;
        return 0;
    }
    slots = (clopts_parser_slot_t *)clopts_mem_calloc(count, sizeof(*slots));
    if (slots == NULL)
    {
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        slots[i].option = clopts_scope_get_option_at(scope, i);
        slots[i].source = CLOPTS_SOURCE_NONE;
        slots[i].flag = false;
        slots[i].count = 0;
        slots[i].value = NULL;
        slots[i].owned = NULL;
        clopts_vec_init(&slots[i].items, sizeof(char *));
    }
    *out = slots;
    return 0;
}

/*
 * Releases what the slots own (environment copies, list items and the item
 * buffers) and the slot array itself. slots may be NULL.
 */
static void free_slots(clopts_parser_slot_t *slots, size_t count)
{
    size_t i;
    size_t j;

    if (slots == NULL)
    {
        return;
    }
    for (i = 0; i < count; i++)
    {
        for (j = 0; j < slots[i].items.len; j++)
        {
            clopts_mem_free(*(char **)clopts_vec_at(&slots[i].items, j));
        }
        clopts_vec_free(&slots[i].items);
        clopts_mem_free(slots[i].owned);
    }
    clopts_mem_free(slots);
}

clopts_result_t *clopts_parser_result_new(const clopts_program_t *program)
{
    clopts_result_t *result;
    clopts_parser_slot_t *global_slots;

    assert(program != NULL);

    result = (clopts_result_t *)clopts_mem_calloc(1, sizeof(*result));
    if (result == NULL)
    {
        return NULL;
    }
    if (alloc_slots(clopts_program_get_scope(program), &global_slots) != 0)
    {
        clopts_mem_free(result);
        return NULL;
    }
    result->status = CLOPTS_PARSE_OK;
    result->command = NULL;
    result->terminal = NULL;
    result->error.usage = CLOPTS_USAGE_NONE;
    result->error.command = NULL;
    result->error.option = NULL;
    result->error.positional = NULL;
    result->error.token = NULL;
    result->error.argv_index = -1;
    result->error.name_len = 0;
    result->error.short_name = '\0';
    result->global_slots = global_slots;
    result->global_count = clopts_scope_get_option_count(clopts_program_get_scope(program));
    result->command_slots = NULL;
    result->command_count = 0;
    clopts_vec_init(&result->positionals, sizeof(const char *));
    return result;
}

clopts_result_t *clopts_parser_result_oom(void)
{
    return (clopts_result_t *)&parser_result_sentinel;
}

int clopts_parser_result_select(clopts_result_t *result, const clopts_command_t *command)
{
    clopts_parser_slot_t *command_slots;

    assert(result != NULL);
    assert(result != &parser_result_sentinel);
    assert(command != NULL);
    assert((result->command == NULL) && (result->command_slots == NULL));

    if (alloc_slots(clopts_command_get_scope(command), &command_slots) != 0)
    {
        return -1;
    }
    result->command = command;
    result->command_slots = command_slots;
    result->command_count = clopts_scope_get_option_count(clopts_command_get_scope(command));
    return 0;
}

clopts_parser_slot_t *clopts_parser_result_slot(clopts_result_t *result, const clopts_lookup_t *ref)
{
    assert(result != NULL);
    assert(result != &parser_result_sentinel);
    assert(ref != NULL);
    assert(ref->option != NULL);

    if (ref->global)
    {
        assert(ref->index < result->global_count);
        return &result->global_slots[ref->index];
    }
    assert(result->command != NULL);
    assert(ref->index < result->command_count);
    return &result->command_slots[ref->index];
}

void clopts_parser_slot_set_flag(clopts_parser_slot_t *slot, bool value, clopts_source_t source)
{
    assert(slot != NULL);

    slot->flag = value;
    slot->source = source;
}

void clopts_parser_slot_add_count(clopts_parser_slot_t *slot, unsigned count_max)
{
    unsigned limit;

    assert(slot != NULL);

    limit = (count_max == 0u) ? UINT_MAX : count_max;
    if (slot->count < limit)
    {
        slot->count++;
    }
    slot->source = CLOPTS_SOURCE_EXPLICIT;
}

void clopts_parser_slot_set_value(clopts_parser_slot_t *slot, const char *value,
                                  clopts_source_t source)
{
    assert(slot != NULL);

    /* Keep value == owned: an owned copy that is no longer the value is released. */
    if ((slot->owned != NULL) && (slot->owned != value))
    {
        clopts_mem_free(slot->owned);
        slot->owned = NULL;
    }
    slot->value = value;
    slot->source = source;
}

/* Appends a copy of the len bytes at item. Returns 0, or -1 leaving the items unchanged. */
static int append_item(clopts_parser_slot_t *slot, const char *item, size_t len)
{
    char *copy;

    copy = clopts_mem_strndup(item, len);
    if (copy == NULL)
    {
        return -1;
    }
    if (clopts_vec_push(&slot->items, &copy) != 0)
    {
        clopts_mem_free(copy);
        return -1;
    }
    return 0;
}

int clopts_parser_slot_append_list(clopts_parser_slot_t *slot, const char *value,
                                   char delimiter, clopts_source_t source)
{
    const char *start;
    const char *end;

    assert(slot != NULL);
    assert(value != NULL);

    start = value;
    for (;;)
    {
        if (delimiter != '\0')
        {
            end = start;
            while ((*end != '\0') && (*end != delimiter))
            {
                end++;
            }
        }
        else
        {
            end = start + strlen(start);
        }

        if (append_item(slot, start, (size_t)(end - start)) != 0)
        {
            return -1;
        }
        /* The source follows the items: it changes once an item was added. */
        slot->source = source;

        if (*end == '\0')
        {
            return 0;
        }
        start = end + 1;
    }
}

void clopts_parser_slot_set_owned_value(clopts_parser_slot_t *slot, char *owned, clopts_source_t source)
{
    assert(slot != NULL);
    assert(owned != NULL);

    if (slot->owned != owned)
    {
        clopts_mem_free(slot->owned);
    }
    slot->owned = owned;
    slot->value = owned;
    slot->source = source;
}

int clopts_parser_result_add_positional(clopts_result_t *result, const char *token)
{
    assert(result != NULL);
    assert(result != &parser_result_sentinel);
    assert(token != NULL);

    return clopts_vec_push(&result->positionals, &token);
}

void clopts_parser_result_set_error(clopts_result_t *result, const clopts_parser_error_t *error)
{
    assert(result != NULL);
    assert(result != &parser_result_sentinel);
    assert(error != NULL);

    result->error = *error;
}

void clopts_parser_result_set_terminal(clopts_result_t *result, const clopts_option_t *option)
{
    assert(result != NULL);
    assert(result != &parser_result_sentinel);

    result->terminal = option;
}

void clopts_parser_result_finish(clopts_result_t *result, clopts_parse_status_t status)
{
    assert(result != NULL);
    assert(result != &parser_result_sentinel);

    result->status = status;
}

/* ---- internal readers ---- */

/* Returns the slot of slots that records option, or NULL. */
static clopts_parser_slot_t *slot_of(clopts_parser_slot_t *slots, size_t count, const clopts_option_t *option)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (slots[i].option == option)
        {
            return &slots[i];
        }
    }
    return NULL;
}

clopts_parser_slot_t *clopts_parser_result_get_slots(clopts_result_t *result, bool global, size_t *count)
{
    assert(result != NULL);
    assert(count != NULL);

    if (global)
    {
        *count = result->global_count;
        return result->global_slots;
    }
    *count = result->command_count;
    return result->command_slots;
}

clopts_parser_slot_t *clopts_parser_result_find_slot(clopts_result_t *result, const clopts_option_t *option)
{
    clopts_parser_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);

    slot = slot_of(result->global_slots, result->global_count, option);
    if (slot == NULL)
    {
        slot = slot_of(result->command_slots, result->command_count, option);
    }
    return slot;
}

size_t clopts_parser_result_get_positional_count(const clopts_result_t *result)
{
    assert(result != NULL);

    return result->positionals.len;
}

const clopts_parser_error_t *clopts_parser_result_get_error(const clopts_result_t *result)
{
    assert(result != NULL);

    return &result->error;
}

const clopts_option_t *clopts_parser_result_get_terminal(const clopts_result_t *result)
{
    assert(result != NULL);

    return result->terminal;
}

/* ---- public accessors ---- */

/* Returns result, or the sentinel when result is NULL. */
static const clopts_result_t *result_or_sentinel(const clopts_result_t *result)
{
    return (result == NULL) ? &parser_result_sentinel : result;
}

/* Returns true when result carries an error (status USAGE_ERROR). */
static bool result_has_error(const clopts_result_t *result)
{
    return ((result != NULL) && (result->status == CLOPTS_PARSE_USAGE_ERROR));
}

/* Returns true when the value accessors of result are meaningful (status OK, TERMINATED or NO_COMMAND). */
static bool result_has_values(const clopts_result_t *result)
{
    return ((result->status == CLOPTS_PARSE_OK) || (result->status == CLOPTS_PARSE_TERMINATED)
            || (result->status == CLOPTS_PARSE_NO_COMMAND));
}

/*
 * Returns the slot of option in result, or NULL when the accessors must
 * return the unset value: a NULL result (read as the sentinel), the
 * sentinel, a NULL option, a status other than OK / TERMINATED /
 * NO_COMMAND, or an option that is attached neither to the program nor to the
 * selected command (an option of another command or program, or an
 * unattached one). With NO_COMMAND only the global options have values.
 */
static const clopts_parser_slot_t *find_slot(const clopts_result_t *result, const clopts_option_t *option)
{
    result = result_or_sentinel(result);
    if ((result == &parser_result_sentinel) || (option == NULL) || (!result_has_values(result)))
    {
        return NULL;
    }
    if (result->status == CLOPTS_PARSE_NO_COMMAND)
    {
        return slot_of(result->global_slots, result->global_count, option);
    }
    return clopts_parser_result_find_slot((clopts_result_t *)result, option);
}

clopts_parse_status_t clopts_result_get_status(const clopts_result_t *result)
{
    return result_or_sentinel(result)->status;
}

const clopts_command_t *clopts_result_get_command(const clopts_result_t *result)
{
    return result_or_sentinel(result)->command;
}

const clopts_option_t *clopts_result_get_terminal_option(const clopts_result_t *result)
{
    result = result_or_sentinel(result);
    return (result->status == CLOPTS_PARSE_TERMINATED) ? result->terminal : NULL;
}

clopts_usage_error_t clopts_result_get_usage_error(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return CLOPTS_USAGE_NONE;
    }
    return result->error.usage;
}

const clopts_command_t *clopts_result_get_error_command(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return NULL;
    }
    return result->error.command;
}

const clopts_option_t *clopts_result_get_error_option(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return NULL;
    }
    return result->error.option;
}

const clopts_positional_t *clopts_result_get_error_positional(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return NULL;
    }
    return result->error.positional;
}

const char *clopts_result_get_error_token(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return NULL;
    }
    return result->error.token;
}

int clopts_result_get_error_argv_index(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return -1;
    }
    return result->error.argv_index;
}

size_t clopts_result_get_error_name_len(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return 0;
    }
    return result->error.name_len;
}

char clopts_result_get_error_short_name(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return '\0';
    }
    return result->error.short_name;
}

bool clopts_result_get_flag(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->flag : false;
}

unsigned clopts_result_get_count(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->count : 0u;
}

const char *clopts_result_get_value(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->value : NULL;
}

size_t clopts_result_get_list_count(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->items.len : 0u;
}

const char *clopts_result_get_list_at(const clopts_result_t *result, const clopts_option_t *option, size_t index)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);
    size_t count = (slot != NULL) ? slot->items.len : 0u;

    assert((index < count) && "index must be less than clopts_result_get_list_count()");
    if (index >= count)
    {
        return NULL;
    }
    return *(char *const *)clopts_vec_at(&slot->items, index);
}

clopts_source_t clopts_result_get_source(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->source : CLOPTS_SOURCE_NONE;
}

size_t clopts_result_get_positional_count(const clopts_result_t *result)
{
    result = result_or_sentinel(result);
    if (!result_has_values(result))
    {
        return 0;
    }
    return result->positionals.len;
}

const char *clopts_result_get_positional_at(const clopts_result_t *result, size_t index)
{
    size_t count = clopts_result_get_positional_count(result);

    assert((index < count) && "index must be less than clopts_result_get_positional_count()");
    if (index >= count)
    {
        return NULL;
    }
    return *(const char *const *)clopts_vec_at(&result->positionals, index);
}

void clopts_result_destroy(clopts_result_t *result)
{
    if ((result == NULL) || (result == &parser_result_sentinel))
    {
        return;
    }
    free_slots(result->global_slots, result->global_count);
    free_slots(result->command_slots, result->command_count);
    clopts_vec_free(&result->positionals);
    clopts_mem_free(result);
}
