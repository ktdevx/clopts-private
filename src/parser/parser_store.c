/*
 * ResultStore: the storage behind clopts_result_t.impl, the public accessors,
 * clopts_result_run and clopts_result_free.
 *
 * Skeleton: clopts_result_free is complete; every other function returns a
 * fixed value until the store is implemented.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <stdlib.h>

int clopts_parser_store_begin(clopts_result_t *result, const clopts_program_t *program)
{
    (void)result;
    (void)program;
    return -1;
}

int clopts_parser_store_select(clopts_result_t *result, const clopts_command_t *command)
{
    (void)result;
    (void)command;
    return -1;
}

clopts_parser_slot_t *clopts_parser_store_slot(clopts_parser_store_t *store,
                                               const clopts_parser_ref_t *ref)
{
    (void)store;
    (void)ref;
    return NULL;
}

void clopts_parser_store_set_flag(clopts_parser_slot_t *slot, bool value, clopts_source_t source)
{
    (void)slot;
    (void)value;
    (void)source;
}

void clopts_parser_store_add_count(clopts_parser_slot_t *slot, unsigned count_max)
{
    (void)slot;
    (void)count_max;
}

void clopts_parser_store_set_value(clopts_parser_slot_t *slot, const char *value,
                                   clopts_source_t source)
{
    (void)slot;
    (void)value;
    (void)source;
}

int clopts_parser_store_append_list(clopts_parser_slot_t *slot, const char *value,
                                    char delimiter, clopts_source_t source)
{
    (void)slot;
    (void)value;
    (void)delimiter;
    (void)source;
    return -1;
}

void clopts_parser_store_own(clopts_parser_slot_t *slot, char *owned)
{
    (void)slot;
    (void)owned;
}

int clopts_parser_store_add_positional(clopts_parser_store_t *store, const char *token)
{
    (void)store;
    (void)token;
    return -1;
}

bool clopts_result_flag(const clopts_result_t *result, const clopts_option_t *option)
{
    (void)result;
    (void)option;
    return false;
}

unsigned clopts_result_count(const clopts_result_t *result, const clopts_option_t *option)
{
    (void)result;
    (void)option;
    return 0;
}

const char *clopts_result_value(const clopts_result_t *result, const clopts_option_t *option)
{
    (void)result;
    (void)option;
    return NULL;
}

size_t clopts_result_list_count(const clopts_result_t *result, const clopts_option_t *option)
{
    (void)result;
    (void)option;
    return 0;
}

const char *clopts_result_list_at(const clopts_result_t *result, const clopts_option_t *option,
                                  size_t index)
{
    (void)result;
    (void)option;
    (void)index;
    return NULL;
}

clopts_source_t clopts_result_source(const clopts_result_t *result, const clopts_option_t *option)
{
    (void)result;
    (void)option;
    return CLOPTS_SOURCE_NONE;
}

size_t clopts_result_positional_count(const clopts_result_t *result)
{
    (void)result;
    return 0;
}

const char *clopts_result_positional_at(const clopts_result_t *result, size_t index)
{
    (void)result;
    (void)index;
    return NULL;
}

int clopts_result_run(const clopts_result_t *result)
{
    (void)result;
    return 0;
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
