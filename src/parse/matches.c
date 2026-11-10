/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Matches: the slots and positionals a parse records into.
 */

#include "parse/matches.h"

#include "base/memory.h"
#include "base/vec.h"

#include <assert.h>

struct clopts_matches
{
    const clopts_command_t *command;  /* selected command, or NULL */
    clopts_slot_t *global_slots;      /* one per global option; NULL without any */
    size_t global_count;
    clopts_slot_t *command_slots;     /* one per option of the selected command; NULL without any */
    size_t command_count;
    clopts_vec_t positionals;         /* const char * elements pointing into argv */
};

/*
 * Allocates one unset slot per option of scope. No options give NULL without
 * allocating. Returns 0, or -1 leaving *out unchanged.
 */
static int alloc_slots(const clopts_scope_t *scope, clopts_slot_t **out)
{
    const size_t count = clopts_scope_get_option_count(scope);
    clopts_slot_t *slots;
    size_t i;

    if (count == 0)
    {
        *out = NULL;
        return 0;
    }
    slots = (clopts_slot_t *)clopts_calloc(count, sizeof(*slots));
    if (slots == NULL)
    {
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        clopts_slot_init(&slots[i], clopts_scope_get_option_at(scope, i));
    }
    *out = slots;
    return 0;
}

/* Releases what the slots own and the slot array itself. slots may be NULL. */
static void free_slots(clopts_slot_t *slots, size_t count)
{
    size_t i;

    if (slots == NULL)
    {
        return;
    }
    for (i = 0; i < count; i++)
    {
        clopts_slot_release(&slots[i]);
    }
    clopts_free(slots);
}

/* The slot of slots that records option, or NULL. */
static clopts_slot_t *slot_in(clopts_slot_t *slots, size_t count, const clopts_option_t *option)
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

clopts_matches_t *clopts_matches_create(const clopts_program_t *program)
{
    const clopts_scope_t *scope;
    clopts_matches_t *matches;

    assert(program != NULL);

    matches = (clopts_matches_t *)clopts_calloc(1, sizeof(*matches));
    if (matches == NULL)
    {
        return NULL;
    }
    scope = clopts_program_get_scope(program);
    if (alloc_slots(scope, &matches->global_slots) != 0)
    {
        clopts_free(matches);
        return NULL;
    }
    matches->command = NULL;
    matches->global_count = clopts_scope_get_option_count(scope);
    matches->command_slots = NULL;
    matches->command_count = 0;
    clopts_vec_init(&matches->positionals, sizeof(const char *));
    return matches;
}

void clopts_matches_destroy(clopts_matches_t *matches)
{
    if (matches == NULL)
    {
        return;
    }
    free_slots(matches->global_slots, matches->global_count);
    free_slots(matches->command_slots, matches->command_count);
    clopts_vec_release(&matches->positionals);
    clopts_free(matches);
}

int clopts_matches_select(clopts_matches_t *matches, const clopts_command_t *command)
{
    const clopts_scope_t *scope;
    clopts_slot_t *slots;

    assert(matches != NULL);
    assert(command != NULL);
    assert((matches->command == NULL) && (matches->command_slots == NULL));

    scope = clopts_command_get_scope(command);
    if (alloc_slots(scope, &slots) != 0)
    {
        return -1;
    }
    matches->command = command;
    matches->command_slots = slots;
    matches->command_count = clopts_scope_get_option_count(scope);
    return 0;
}

const clopts_command_t *clopts_matches_get_command(const clopts_matches_t *matches)
{
    assert(matches != NULL);
    return matches->command;
}

clopts_slot_t *clopts_matches_get_slots(clopts_matches_t *matches, bool global, size_t *count)
{
    assert(matches != NULL);
    assert(count != NULL);

    if (global)
    {
        *count = matches->global_count;
        return matches->global_slots;
    }
    *count = matches->command_count;
    return matches->command_slots;
}

const clopts_slot_t *clopts_matches_find_slot(const clopts_matches_t *matches, const clopts_option_t *option)
{
    clopts_slot_t *slot;

    assert(matches != NULL);
    if (option == NULL)
    {
        return NULL;
    }
    slot = slot_in(matches->global_slots, matches->global_count, option);
    if (slot == NULL)
    {
        slot = slot_in(matches->command_slots, matches->command_count, option);
    }
    return slot;
}

clopts_slot_t *clopts_matches_get_slot(clopts_matches_t *matches, const clopts_option_t *option)
{
    clopts_slot_t *slot;

    assert(matches != NULL);
    assert(option != NULL);
    slot = slot_in(matches->global_slots, matches->global_count, option);
    if (slot == NULL)
    {
        slot = slot_in(matches->command_slots, matches->command_count, option);
    }
    assert((slot != NULL) && "the option must be a global option or one of the selected command");
    return slot;
}

int clopts_matches_add_positional(clopts_matches_t *matches, const char *token)
{
    assert(matches != NULL);
    assert(token != NULL);

    return clopts_vec_push(&matches->positionals, &token);
}

size_t clopts_matches_get_positional_count(const clopts_matches_t *matches)
{
    assert(matches != NULL);
    return matches->positionals.len;
}

const char *clopts_matches_get_positional_at(const clopts_matches_t *matches, size_t index)
{
    assert(matches != NULL);
    assert(index < matches->positionals.len);
    return *(const char *const *)clopts_vec_at(&matches->positionals, index);
}
