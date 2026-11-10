/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_SLOT_H
#define CLOPTS_SRC_PARSE_SLOT_H

#include "clopts/parse/result.h"

#include "base/vec.h"

#include <stdbool.h>

/*
 * Slot (slot.c): what a parse has recorded for one option, and the rules of
 * recording it again (the last one wins, a count is clamped, list values are
 * split). The value and its source are always updated together, and
 * value == owned whenever owned is not NULL. The fields may be read freely;
 * they are written only through the clopts_slot_* functions. Every
 * allocation goes through the allocation gateway (base/mem.h).
 */
typedef struct clopts_slot
{
    const clopts_option_t *option;  /* the option of the slot */
    clopts_source_t source;
    bool flag;
    unsigned count;
    const char *value;        /* VALUE: points into argv / the model's default / owned; NULL if none */
    char *owned;              /* copy of an environment value owned by the slot, or NULL */
    clopts_vec_t items;       /* LIST: char * elements, each owned by the slot */
} clopts_slot_t;

/* Makes slot the unset slot of option. */
void clopts_slot_init(clopts_slot_t *slot, const clopts_option_t *option);

/* Releases what slot owns (an environment copy, list items). */
void clopts_slot_release(clopts_slot_t *slot);

/* FLAG: records value (false when negated) with source; the last one wins. */
void clopts_slot_set_flag(clopts_slot_t *slot, bool value, clopts_source_t source);

/* COUNT: adds 1, clamped to count_max (0 means UINT_MAX). The source becomes CLOPTS_SOURCE_EXPLICIT. */
void clopts_slot_add_count(clopts_slot_t *slot, unsigned count_max);

/* VALUE: replaces the value (releasing a previous owned copy) and records source; the last one wins. */
void clopts_slot_set_value(clopts_slot_t *slot, const char *value, clopts_source_t source);

/*
 * VALUE: records owned (an environment value copy) as the value with source,
 * and moves its ownership to slot, releasing the previous owned copy.
 */
void clopts_slot_set_owned_value(clopts_slot_t *slot, char *owned, clopts_source_t source);

/*
 * LIST: splits value at delimiter and appends a copy of every item, empty
 * items included; with delimiter '\0' value is a single item. Records source.
 * Returns 0, or -1 keeping the items appended so far.
 */
int clopts_slot_append_list(clopts_slot_t *slot, const char *value, char delimiter, clopts_source_t source);

#endif /* CLOPTS_SRC_PARSE_SLOT_H */
