/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Slot: the recording rules of one option.
 */

#include "parse/slot.h"

#include "base/memory.h"
#include "base/string.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

void clopts_slot_init(clopts_slot_t *slot, const clopts_option_t *option)
{
    assert(slot != NULL);
    slot->option = option;
    slot->source = CLOPTS_SOURCE_NONE;
    slot->flag = false;
    slot->count = 0;
    slot->value = NULL;
    slot->owned = NULL;
    clopts_vec_init(&slot->items, sizeof(char *));
}

void clopts_slot_release(clopts_slot_t *slot)
{
    size_t i;

    assert(slot != NULL);
    for (i = 0; i < slot->items.len; i++)
    {
        clopts_free(*(char **)clopts_vec_at(&slot->items, i));
    }
    clopts_vec_release(&slot->items);
    clopts_free(slot->owned);
    slot->owned = NULL;
    slot->value = NULL;
}

void clopts_slot_set_flag(clopts_slot_t *slot, bool value, clopts_source_t source)
{
    assert(slot != NULL);

    slot->flag = value;
    slot->source = source;
}

void clopts_slot_add_count(clopts_slot_t *slot, unsigned count_max)
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

void clopts_slot_set_value(clopts_slot_t *slot, const char *value, clopts_source_t source)
{
    assert(slot != NULL);

    /* Keep value == owned: an owned copy that is no longer the value is released. */
    if ((slot->owned != NULL) && (slot->owned != value))
    {
        clopts_free(slot->owned);
        slot->owned = NULL;
    }
    slot->value = value;
    slot->source = source;
}

void clopts_slot_set_owned_value(clopts_slot_t *slot, char *owned, clopts_source_t source)
{
    assert(slot != NULL);
    assert(owned != NULL);

    if (slot->owned != owned)
    {
        clopts_free(slot->owned);
    }
    slot->owned = owned;
    slot->value = owned;
    slot->source = source;
}

/* Appends a copy of the len bytes at item. Returns 0, or -1 leaving the items unchanged. */
static int append_item(clopts_slot_t *slot, const char *item, size_t len)
{
    char *copy;

    copy = clopts_strndup(item, len);
    if (copy == NULL)
    {
        return -1;
    }
    if (clopts_vec_push(&slot->items, &copy) != 0)
    {
        clopts_free(copy);
        return -1;
    }
    return 0;
}

int clopts_slot_append_list(clopts_slot_t *slot, const char *value, char delimiter, clopts_source_t source)
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
