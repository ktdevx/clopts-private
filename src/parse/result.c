/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Result: made once by clopts_result_create() when a parse ends, then read
 * only. It owns the matches of the parse, which hold the values. A result
 * does not point to its program, and clopts_result_destroy() never reads the
 * declaration, so the result may be released after the program.
 */

#include "parse/result.h"

#include "base/memory.h"
#include "parse/usage_error.h"

#include <assert.h>
#include <stdbool.h>

struct clopts_result
{
    clopts_parse_status_t status;
    const clopts_option_t *terminal;  /* TERMINATED: the terminal option, else NULL */
    clopts_usage_error_t error;       /* USAGE_ERROR: the error, else kind NONE */
    clopts_matches_t *matches;        /* NULL only in the sentinel */
};

/*
 * The out-of-memory sentinel. Never written to; shared between threads
 * safely because it is a const static object.
 */
static const struct clopts_result sentinel = {
    CLOPTS_PARSE_OUT_OF_MEMORY,
    NULL,
    { CLOPTS_USAGE_ERROR_NONE, NULL, NULL, NULL, -1, 0, '\0' },
    NULL
};

clopts_result_t *clopts_result_sentinel(void)
{
    /* The cast drops const only for the handle type; nothing ever writes through it. */
    return (clopts_result_t *)&sentinel;
}

clopts_result_t *clopts_result_create(clopts_parse_status_t status, const clopts_option_t *terminal,
                                      const clopts_usage_error_t *error, clopts_matches_t *matches)
{
    clopts_result_t *result;

    assert((status != CLOPTS_PARSE_TERMINATED) || (terminal != NULL));
    assert((status != CLOPTS_PARSE_USAGE_ERROR) || (error != NULL));

    if ((status == CLOPTS_PARSE_OUT_OF_MEMORY) || (matches == NULL))
    {
        clopts_matches_destroy(matches);
        return clopts_result_sentinel();
    }
    result = (clopts_result_t *)clopts_malloc(sizeof(*result));
    if (result == NULL)
    {
        clopts_matches_destroy(matches);
        return clopts_result_sentinel();
    }
    result->status = status;
    result->terminal = (status == CLOPTS_PARSE_TERMINATED) ? terminal : NULL;
    if (status == CLOPTS_PARSE_USAGE_ERROR)
    {
        result->error = *error;
    }
    else
    {
        clopts_usage_error_init(&result->error, CLOPTS_USAGE_ERROR_NONE);
    }
    result->matches = matches;
    return result;
}

/* ---- public accessors ---- */

/* Returns result, or the sentinel when result is NULL. */
static const clopts_result_t *or_sentinel(const clopts_result_t *result)
{
    return (result == NULL) ? &sentinel : result;
}

/* True when the value accessors of result are meaningful (status OK, TERMINATED or NO_COMMAND). */
static bool has_values(const clopts_result_t *result)
{
    return (result->matches != NULL) &&
           ((result->status == CLOPTS_PARSE_OK) || (result->status == CLOPTS_PARSE_TERMINATED) ||
            (result->status == CLOPTS_PARSE_NO_COMMAND));
}

/*
 * The slot of option in result, or NULL when the accessors return the unset
 * value: a NULL result or the sentinel, a NULL option, a status other than
 * OK / TERMINATED / NO_COMMAND, or an option that is attached neither to the
 * program nor to the selected command (with NO_COMMAND no command was
 * selected, so only the global options have slots).
 */
static const clopts_slot_t *find_slot(const clopts_result_t *result, const clopts_option_t *option)
{
    result = or_sentinel(result);
    if (!has_values(result))
    {
        return NULL;
    }
    return clopts_matches_find_slot(result->matches, option);
}

clopts_parse_status_t clopts_result_get_status(const clopts_result_t *result)
{
    return or_sentinel(result)->status;
}

const clopts_command_t *clopts_result_get_command(const clopts_result_t *result)
{
    result = or_sentinel(result);
    return (result->matches != NULL) ? clopts_matches_get_command(result->matches) : NULL;
}

const clopts_option_t *clopts_result_get_terminal_option(const clopts_result_t *result)
{
    return or_sentinel(result)->terminal;
}

const clopts_usage_error_t *clopts_result_get_usage_error(const clopts_result_t *result)
{
    result = or_sentinel(result);
    return (result->status == CLOPTS_PARSE_USAGE_ERROR) ? &result->error : NULL;
}

bool clopts_result_get_flag(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->flag : false;
}

unsigned clopts_result_get_count(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->count : 0u;
}

const char *clopts_result_get_value(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->value : NULL;
}

size_t clopts_result_get_list_count(const clopts_result_t *result, const clopts_option_t *option)
{
    const clopts_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->items.len : 0u;
}

const char *clopts_result_get_list_at(const clopts_result_t *result, const clopts_option_t *option, size_t index)
{
    const clopts_slot_t *slot = find_slot(result, option);
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
    const clopts_slot_t *slot = find_slot(result, option);

    return (slot != NULL) ? slot->source : CLOPTS_SOURCE_NONE;
}

size_t clopts_result_get_positional_count(const clopts_result_t *result)
{
    result = or_sentinel(result);
    return has_values(result) ? clopts_matches_get_positional_count(result->matches) : 0;
}

const char *clopts_result_get_positional_at(const clopts_result_t *result, size_t index)
{
    size_t count = clopts_result_get_positional_count(result);

    assert((index < count) && "index must be less than clopts_result_get_positional_count()");
    if (index >= count)
    {
        return NULL;
    }
    return clopts_matches_get_positional_at(result->matches, index);
}

void clopts_result_destroy(clopts_result_t *result)
{
    if ((result == NULL) || (result == &sentinel))
    {
        return;
    }
    clopts_matches_destroy(result->matches);
    clopts_free(result);
}
