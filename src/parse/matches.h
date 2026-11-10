/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_MATCHES_H
#define CLOPTS_SRC_PARSE_MATCHES_H

#include "clopts/decl/program.h"

#include "parse/slot.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Matches (matches.c): what a parse records while it reads argv and the
 * environment: the selected command, one slot per option of the program and
 * of the selected command, and the positionals. The scan, the resolution and
 * the required checks write and read them; the finished result
 * (parse/result.h) takes them over and is read only.
 *
 * A slot has the same index as its option in its scope and is found from
 * the option by its address. Matches never point to the program and are
 * released without reading the declaration, so they may outlive it. Every
 * allocation goes through the allocation gateway (base/memory.h), and whatever
 * was allocated is linked to the matches at once, so
 * clopts_matches_destroy() releases it even after a failure.
 */

typedef struct clopts_matches clopts_matches_t;

/* Allocates the matches of program, with one unset slot per global option. NULL on failure. */
clopts_matches_t *clopts_matches_create(const clopts_program_t *program);

/* Releases matches and everything its slots own. NULL does nothing. */
void clopts_matches_destroy(clopts_matches_t *matches);

/* Selects command (once) and allocates its slots. Returns 0, or -1 leaving matches unchanged. */
int clopts_matches_select(clopts_matches_t *matches, const clopts_command_t *command);

/* The selected command, or NULL. */
const clopts_command_t *clopts_matches_get_command(const clopts_matches_t *matches);

/*
 * Returns the slots of the global options (global true) or of the selected
 * command (NULL without one, or without options) and sets *count to their
 * number. Slot i records option i of its scope.
 */
clopts_slot_t *clopts_matches_get_slots(clopts_matches_t *matches, bool global, size_t *count);

/* The slot of option (a global option or one of the selected command), or NULL. */
const clopts_slot_t *clopts_matches_find_slot(const clopts_matches_t *matches, const clopts_option_t *option);

/* The slot of option to record into; option must have one. */
clopts_slot_t *clopts_matches_get_slot(clopts_matches_t *matches, const clopts_option_t *option);

/* Appends token (an argv element, borrowed) as a positional. Returns 0 or -1. */
int clopts_matches_add_positional(clopts_matches_t *matches, const char *token);

size_t clopts_matches_get_positional_count(const clopts_matches_t *matches);

/* index must be less than clopts_matches_get_positional_count(). */
const char *clopts_matches_get_positional_at(const clopts_matches_t *matches, size_t index);

#endif /* CLOPTS_SRC_PARSE_MATCHES_H */
