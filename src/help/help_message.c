/*
 * HelpMessage: error line of a result, its prefix, and the message tables.
 *
 * Skeleton: clopts_write_error returns CLOPTS_WRITE_UNAVAILABLE without
 * calling write_fn, and the tables have no entries yet.
 */

#include "help/help_internal.h"

clopts_write_status_t clopts_write_error(const clopts_program_t *program, const clopts_result_t *result,
                                         unsigned flags, clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)result;
    (void)flags;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}

const char *clopts_help_usage_text(clopts_usage_error_t kind)
{
    (void)kind;
    return NULL;
}

const char *clopts_help_decl_reason(clopts_decl_error_t kind)
{
    (void)kind;
    return NULL;
}
