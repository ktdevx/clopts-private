/*
 * Result handle: allocation, the out-of-memory sentinel, recording into
 * slots, the accessors, run and release.
 *
 * Implemented: the sentinel; clopts_parser_result_new() for the result
 * itself (no slots yet); clopts_result_status() and the declaration error
 * accessors; clopts_result_free() ignoring NULL and the sentinel and
 * releasing the result itself. Every accessor reads a NULL result like the
 * sentinel. Skeleton: the other functions are placeholders that return fixed
 * values (the sentinel's values for the accessors). Task 6.2 implements them,
 * together with the slots of clopts_parser_result_new() and their release.
 */

#include "parser/parser_internal.h"

#include <assert.h>

/*
 * The out-of-memory sentinel. Never written to; shared between threads
 * safely because it is a const static object.
 */
static const struct clopts_result parser_result_sentinel = {
    .status = CLOPTS_OUT_OF_MEMORY,
    .program = NULL,
    .command = NULL,
    .error = {
        .usage = CLOPTS_USAGE_NONE,
        .decl = CLOPTS_DECL_NONE,
        .command = NULL,
        .option = NULL,
        .positional = NULL,
        .token = NULL,
        .argv_index = -1,
        .name_len = 0,
        .short_name = '\0'
    },
    .parsed = false,
    .global_slots = NULL,
    .global_count = 0,
    .command_slots = NULL,
    .command_count = 0,
    .positionals = { .data = NULL, .len = 0, .cap = 0, .elem_size = 0 },
    .probe = NULL
};

/* ---- internal API ---- */

clopts_result_t *clopts_parser_result_new(const clopts_program_t *program)
{
    clopts_result_t *result;

    assert(program != NULL);

    result = (clopts_result_t *)clopts_parser_calloc(program->probe, 1, sizeof(*result));
    if (result == NULL)
    {
        return NULL;
    }
    result->status = CLOPTS_OK;
    result->program = program;
    result->command = NULL;
    result->error.usage = CLOPTS_USAGE_NONE;
    result->error.decl = CLOPTS_DECL_NONE;
    result->error.command = NULL;
    result->error.option = NULL;
    result->error.positional = NULL;
    result->error.token = NULL;
    result->error.argv_index = -1;
    result->error.name_len = 0;
    result->error.short_name = '\0';
    result->parsed = false;
    result->global_slots = NULL;
    result->global_count = 0;
    result->command_slots = NULL;
    result->command_count = 0;
    clopts_core_vec_init(&result->positionals, sizeof(const char *));
    result->probe = program->probe;
    return result;
}

clopts_result_t *clopts_parser_result_oom(void)
{
    return (clopts_result_t *)&parser_result_sentinel;
}

int clopts_parser_result_select(clopts_result_t *result, const clopts_command_t *command)
{
    (void)result;
    (void)command;
    return -1;
}

clopts_parser_slot_t *clopts_parser_result_slot(clopts_result_t *result, const clopts_parser_ref_t *ref)
{
    (void)result;
    (void)ref;
    return NULL;
}

void clopts_parser_slot_set_flag(clopts_parser_slot_t *slot, bool value, clopts_source_t source)
{
    (void)slot;
    (void)value;
    (void)source;
}

void clopts_parser_slot_add_count(clopts_parser_slot_t *slot, unsigned count_max)
{
    (void)slot;
    (void)count_max;
}

void clopts_parser_slot_set_value(clopts_parser_probe_t *probe, clopts_parser_slot_t *slot, const char *value,
                                  clopts_source_t source)
{
    (void)probe;
    (void)slot;
    (void)value;
    (void)source;
}

int clopts_parser_slot_append_list(clopts_parser_probe_t *probe, clopts_parser_slot_t *slot, const char *value,
                                   char delimiter, clopts_source_t source)
{
    (void)probe;
    (void)slot;
    (void)value;
    (void)delimiter;
    (void)source;
    return -1;
}

void clopts_parser_slot_own(clopts_parser_probe_t *probe, clopts_parser_slot_t *slot, char *owned)
{
    (void)probe;
    (void)slot;
    (void)owned;
}

int clopts_parser_result_add_positional(clopts_result_t *result, const char *token)
{
    (void)result;
    (void)token;
    return -1;
}

/* ---- public accessors ---- */

/* Returns true when result carries an error (status USAGE_ERROR or DECL_ERROR). */
static bool result_has_error(const clopts_result_t *result)
{
    return ((result != NULL) &&
            ((result->status == CLOPTS_USAGE_ERROR) || (result->status == CLOPTS_DECL_ERROR)));
}

clopts_status_t clopts_result_status(const clopts_result_t *result)
{
    if (result == NULL)
    {
        return parser_result_sentinel.status;
    }
    return result->status;
}

const clopts_command_t *clopts_result_command(const clopts_result_t *result)
{
    (void)result;
    return NULL;
}

clopts_usage_error_t clopts_result_usage_error(const clopts_result_t *result)
{
    (void)result;
    return CLOPTS_USAGE_NONE;
}

clopts_decl_error_t clopts_result_decl_error(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return CLOPTS_DECL_NONE;
    }
    return result->error.decl;
}

const clopts_command_t *clopts_result_error_command(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return NULL;
    }
    return result->error.command;
}

const clopts_option_t *clopts_result_error_option(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return NULL;
    }
    return result->error.option;
}

const clopts_positional_t *clopts_result_error_positional(const clopts_result_t *result)
{
    if (!result_has_error(result))
    {
        return NULL;
    }
    return result->error.positional;
}

const char *clopts_result_error_token(const clopts_result_t *result)
{
    (void)result;
    return NULL;
}

int clopts_result_error_argv_index(const clopts_result_t *result)
{
    (void)result;
    return -1;
}

size_t clopts_result_error_name_len(const clopts_result_t *result)
{
    (void)result;
    return 0;
}

char clopts_result_error_short_name(const clopts_result_t *result)
{
    (void)result;
    return '\0';
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

const char *clopts_result_list_at(const clopts_result_t *result, const clopts_option_t *option, size_t index)
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

void clopts_result_free(clopts_result_t *result)
{
    if ((result == NULL) || (result == &parser_result_sentinel))
    {
        return;
    }
    clopts_parser_free(result->probe, result);
}
