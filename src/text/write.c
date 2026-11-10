/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * The public entry points of the text module (clopts/text/write.h): each one
 * checks its arguments, decides whether there is anything to write, makes
 * the output on the callback and returns how the writing went. The text
 * itself is made by help.c and message.c.
 */

#include "clopts/text/write.h"

#include "text/help.h"
#include "text/message.h"
#include "text/out.h"

#include <assert.h>
#include <stdbool.h>

#ifndef NDEBUG
/* True when command is a subcommand of program (read by the asserts only). */
static bool has_command(const clopts_program_t *program, const clopts_command_t *command)
{
    size_t count = clopts_program_get_command_count(program);
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (clopts_program_get_command_at(program, i) == command)
        {
            return true;
        }
    }
    return false;
}
#endif

clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                        clopts_write_fn write_fn, void *ctx)
{
    clopts_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || has_command(program, command));

    clopts_out_init(&out, write_fn, ctx);
    clopts_help_write(&out, program, command);
    return clopts_out_status(&out);
}

clopts_write_status_t clopts_write_usage(const clopts_program_t *program, const clopts_command_t *command,
                                         clopts_write_fn write_fn, void *ctx)
{
    clopts_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || has_command(program, command));

    clopts_out_init(&out, write_fn, ctx);
    clopts_help_write_usage(&out, program, command);
    return clopts_out_status(&out);
}

clopts_write_status_t clopts_write_version(const clopts_program_t *program, const char *version,
                                           clopts_write_fn write_fn, void *ctx)
{
    clopts_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);

    if (version == NULL)
    {
        return CLOPTS_WRITE_UNAVAILABLE;
    }
    clopts_out_init(&out, write_fn, ctx);
    clopts_message_write_version(&out, program, version);
    return clopts_out_status(&out);
}

clopts_write_status_t clopts_write_usage_hint(const clopts_program_t *program, const clopts_command_t *command,
                                              clopts_write_fn write_fn, void *ctx)
{
    clopts_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || has_command(program, command));

    clopts_out_init(&out, write_fn, ctx);
    clopts_message_write_usage_hint(&out, program, command);
    return clopts_out_status(&out);
}

clopts_write_status_t clopts_write_error(const clopts_program_t *program, const clopts_result_t *result,
                                         unsigned flags, clopts_write_fn write_fn, void *ctx)
{
    clopts_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((clopts_result_get_command(result) == NULL) || has_command(program, clopts_result_get_command(result)));

    switch (clopts_result_get_status(result))
    {
    case CLOPTS_PARSE_USAGE_ERROR:
        if (clopts_message_usage_text(clopts_usage_error_get_kind(clopts_result_get_usage_error(result))) == NULL)
        {
            return CLOPTS_WRITE_UNAVAILABLE;
        }
        break;
    case CLOPTS_PARSE_OUT_OF_MEMORY:
        break;
    default:
        /* OK / TERMINATED / NO_COMMAND: no error to describe. */
        return CLOPTS_WRITE_UNAVAILABLE;
    }
    clopts_out_init(&out, write_fn, ctx);
    clopts_message_write_error(&out, program, result, flags);
    return clopts_out_status(&out);
}
