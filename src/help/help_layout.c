/*
 * HelpLayout: help, Usage line, version line and usage hint.
 *
 * Skeleton: every function returns CLOPTS_WRITE_UNAVAILABLE without calling
 * write_fn.
 */

#include "help/help_internal.h"

clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                        clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)command;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}

clopts_write_status_t clopts_write_usage(const clopts_program_t *program, const clopts_command_t *command,
                                         clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)command;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}

clopts_write_status_t clopts_write_version(const clopts_program_t *program, clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}

clopts_write_status_t clopts_write_usage_hint(const clopts_program_t *program, const clopts_command_t *command,
                                              clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)command;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}
