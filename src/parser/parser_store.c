/*
 * ResultStore: the storage behind clopts_result_t.impl, the public accessors,
 * clopts_result_run and clopts_result_free.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 * Allocates count zeroed slots and initializes their item vecs. count == 0
 * gives NULL. Returns 0 on success and -1 on size overflow or allocation
 * failure, leaving *out unchanged.
 */
static int alloc_slots(size_t count, clopts_parser_slot_t **out)
{
    clopts_parser_slot_t *slots;
    size_t bytes;
    size_t i;

    if (count == 0)
    {
        *out = NULL;
        return 0;
    }
    if (clopts_core_size_mul(count, sizeof(*slots), &bytes) != 0)
    {
        return -1;
    }
    slots = calloc(count, sizeof(*slots));
    if (slots == NULL)
    {
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        slots[i].source = CLOPTS_SOURCE_NONE;
        slots[i].value = NULL;
        slots[i].owned = NULL;
        clopts_core_vec_init(&slots[i].items, sizeof(char *));
    }
    *out = slots;
    return 0;
}

int clopts_parser_store_begin(clopts_result_t *result, const clopts_program_t *program)
{
    clopts_parser_store_t *store;
    clopts_parser_slot_t *global_slots;

    assert(result != NULL);
    assert(program != NULL);

    clopts_result_free(result);

    if (alloc_slots(program->option_count, &global_slots) != 0)
    {
        return -1;
    }
    store = malloc(sizeof(*store));
    if (store == NULL)
    {
        free(global_slots);
        return -1;
    }
    store->program = program;
    store->command = NULL;
    store->global_slots = global_slots;
    store->command_slots = NULL;
    clopts_core_vec_init(&store->positionals, sizeof(const char *));
    result->impl = store;
    return 0;
}

int clopts_parser_store_select(clopts_result_t *result, const clopts_command_t *command)
{
    clopts_parser_store_t *store;
    clopts_parser_slot_t *command_slots;

    assert(result != NULL);
    assert(command != NULL);
    store = result->impl;
    assert(store != NULL);
    assert(store->command == NULL);

    if (alloc_slots(command->option_count, &command_slots) != 0)
    {
        return -1;
    }
    store->command = command;
    store->command_slots = command_slots;
    return 0;
}

clopts_parser_slot_t *clopts_parser_store_slot(clopts_parser_store_t *store,
                                               const clopts_parser_ref_t *ref)
{
    assert(store != NULL);
    assert(ref != NULL);
    assert(ref->option != NULL);

    if (ref->global)
    {
        assert(ref->index < store->program->option_count);
        assert(ref->option == &store->program->options[ref->index]);
        return &store->global_slots[ref->index];
    }
    assert(store->command != NULL);
    assert(ref->index < store->command->option_count);
    assert(ref->option == &store->command->options[ref->index]);
    return &store->command_slots[ref->index];
}

void clopts_parser_store_set_flag(clopts_parser_slot_t *slot, bool value, clopts_source_t source)
{
    assert(slot != NULL);

    slot->flag = value;
    slot->source = source;
}

void clopts_parser_store_add_count(clopts_parser_slot_t *slot, unsigned count_max)
{
    unsigned limit;

    assert(slot != NULL);

    limit = (count_max == 0u) ? CLOPTS_COUNT_MAX_DEFAULT : count_max;
    if (slot->count < limit)
    {
        slot->count++;
    }
    slot->source = CLOPTS_SOURCE_EXPLICIT;
}

void clopts_parser_store_set_value(clopts_parser_slot_t *slot, const char *value,
                                   clopts_source_t source)
{
    assert(slot != NULL);

    /* Keep value == owned: an owned copy that is no longer the value is released. */
    if ((slot->owned != NULL) && (slot->owned != value))
    {
        free(slot->owned);
        slot->owned = NULL;
    }
    slot->value = value;
    slot->source = source;
}

/*
 * Appends a copy of the len bytes at item as a NUL-terminated string. Returns
 * 0 on success and -1 on failure, leaving the items unchanged.
 */
static int append_item(clopts_parser_slot_t *slot, const char *item, size_t len)
{
    size_t bytes;
    char *copy;

    if (clopts_core_size_add(len, 1, &bytes) != 0)
    {
        return -1;
    }
    copy = malloc(bytes);
    if (copy == NULL)
    {
        return -1;
    }
    memcpy(copy, item, len);
    copy[len] = '\0';
    if (clopts_core_vec_push(&slot->items, &copy) != 0)
    {
        free(copy);
        return -1;
    }
    return 0;
}

int clopts_parser_store_append_list(clopts_parser_slot_t *slot, const char *value,
                                    char delimiter, clopts_source_t source)
{
    const char *start;

    assert(slot != NULL);
    assert(value != NULL);

    start = value;
    for (;;)
    {
        const char *end = start;

        if (delimiter != '\0')
        {
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

void clopts_parser_store_own(clopts_parser_slot_t *slot, char *owned)
{
    assert(slot != NULL);
    assert(owned != NULL);

    if (slot->owned != owned)
    {
        free(slot->owned);
    }
    slot->owned = owned;
    slot->value = owned;
}

int clopts_parser_store_add_positional(clopts_parser_store_t *store, const char *token)
{
    assert(store != NULL);
    assert(token != NULL);

    return clopts_core_vec_push(&store->positionals, &token);
}

/* Returns the slot at index of slots whose option table is options, or NULL. */
static const clopts_parser_slot_t *find_in(const clopts_parser_slot_t *slots,
                                           const clopts_option_t *options, size_t count,
                                           const clopts_option_t *option)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (&options[i] == option)
        {
            return &slots[i];
        }
    }
    return NULL;
}

/*
 * Resolves option to its slot: the selected command's options first, then the
 * global options, by comparing element addresses. An option of neither table
 * is a contract violation; with NDEBUG it gives NULL.
 */
static const clopts_parser_slot_t *find_slot(const clopts_result_t *result,
                                             const clopts_option_t *option)
{
    const clopts_parser_store_t *store;
    const clopts_parser_slot_t *slot = NULL;

    assert(result != NULL);
    assert(option != NULL);
    assert((result->status == CLOPTS_OK) || (result->status == CLOPTS_NO_COMMAND));

    store = result->impl;
    assert(store != NULL);
    if (store == NULL)
    {
        return NULL;
    }

    if (store->command != NULL)
    {
        slot = find_in(store->command_slots, store->command->options,
                       store->command->option_count, option);
    }
    if (slot == NULL)
    {
        slot = find_in(store->global_slots, store->program->options, store->program->option_count,
                       option);
    }
    assert((slot != NULL) && "option is not in a declaration table of this result");
    return slot;
}

bool clopts_result_flag(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->flag : false;
}

unsigned clopts_result_count(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->count : 0u;
}

const char *clopts_result_value(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->value : NULL;
}

size_t clopts_result_list_count(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->items.len : 0u;
}

const char *clopts_result_list_at(const clopts_result_t *result, const clopts_option_t *option,
                                  size_t index)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    if (slot == NULL)
    {
        return NULL;
    }
    assert(index < slot->items.len);
    if (index >= slot->items.len)
    {
        return NULL;
    }
    return *(char *const *)clopts_core_vec_at(&slot->items, index);
}

clopts_source_t clopts_result_source(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_parser_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->source : CLOPTS_SOURCE_NONE;
}

size_t clopts_result_positional_count(const clopts_result_t *result)
{
    assert(result != NULL);
    assert((result->status == CLOPTS_OK) || (result->status == CLOPTS_NO_COMMAND));
    assert(result->impl != NULL);

    return (result->impl != NULL) ? result->impl->positionals.len : 0u;
}

const char *clopts_result_positional_at(const clopts_result_t *result, size_t index)
{
    assert(result != NULL);
    assert((result->status == CLOPTS_OK) || (result->status == CLOPTS_NO_COMMAND));
    assert(result->impl != NULL);
    assert(index < result->impl->positionals.len);

    if ((result->impl == NULL) || (index >= result->impl->positionals.len))
    {
        return NULL;
    }
    return *(const char *const *)clopts_core_vec_at(&result->impl->positionals, index);
}

int clopts_result_run(const clopts_result_t *result)
{
    const clopts_parser_store_t *store;
    clopts_handler_fn handler;
    void *userdata;

    assert(result != NULL);
    assert(result->status == CLOPTS_OK);
    store = result->impl;
    assert(store != NULL);

    if (store->program->command_count == 0)
    {
        handler = store->program->handler;
        userdata = store->program->userdata;
    }
    else
    {
        assert(store->command != NULL);
        handler = store->command->handler;
        userdata = store->command->userdata;
    }
    assert(handler != NULL);
    return handler(result, userdata);
}

/*
 * Releases what the slots own (environment copies, list items and the item
 * arrays) and the slot array itself. slots may be NULL.
 */
static void free_slots(clopts_parser_slot_t *slots, size_t count)
{
    size_t i;

    if (slots == NULL)
    {
        return;
    }
    for (i = 0; i < count; i++)
    {
        size_t j;

        for (j = 0; j < slots[i].items.len; j++)
        {
            free(*(char **)clopts_core_vec_at(&slots[i].items, j));
        }
        clopts_core_vec_free(&slots[i].items);
        free(slots[i].owned);
    }
    free(slots);
}

void clopts_result_free(clopts_result_t *result)
{
    clopts_parser_store_t *store;

    assert(result != NULL);

    store = result->impl;
    if (store == NULL)
    {
        return;
    }

    free_slots(store->global_slots, store->program->option_count);
    if (store->command != NULL)
    {
        free_slots(store->command_slots, store->command->option_count);
    }
    clopts_core_vec_free(&store->positionals);
    free(store);
    result->impl = NULL;
}
