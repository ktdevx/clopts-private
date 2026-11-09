/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * argv handles: the out-of-memory sentinel, failure and passthrough handles,
 * the public accessors and the release function.
 */

#include "platform/platform_internal.h"

#include <assert.h>

/* The values of every handle without elements: { NULL }. */
static char *const platform_argv_empty[1] = { NULL };

/* Shared out-of-memory sentinel; never written, so it may be shared by threads. */
static const clopts_argv_t platform_argv_oom = { CLOPTS_ARGV_OUT_OF_MEMORY, 0, -1, platform_argv_empty };

/* A NULL handle reads like the sentinel. */
static const clopts_argv_t *platform_argv_read(const clopts_argv_t *args)
{
    return (args == NULL) ? &platform_argv_oom : args;
}

/* Allocates a handle without elements of its own; NULL when the allocation fails. */
static clopts_argv_t *platform_argv_new(clopts_argv_status_t status, int argc,
                                        int error_index, char *const *argv)
{
    clopts_argv_t *args;

    args = (clopts_argv_t *)clopts_core_malloc(sizeof(*args));
    if (args == NULL)
    {
        return NULL;
    }
    args->status = status;
    args->argc = argc;
    args->error_index = error_index;
    args->argv = argv;
    return args;
}

clopts_argv_t *clopts_platform_argv_sentinel(void)
{
    /* The cast drops const only for the handle type; nothing ever writes through it. */
    return (clopts_argv_t *)&platform_argv_oom;
}

clopts_argv_t *clopts_platform_argv_failure(clopts_argv_status_t status, int error_index)
{
    clopts_argv_t *args;

    assert(status != CLOPTS_ARGV_OK);
    assert((status != CLOPTS_ARGV_INVALID_UTF16) || (error_index >= 0));
    if (status == CLOPTS_ARGV_OUT_OF_MEMORY)
    {
        return clopts_platform_argv_sentinel();
    }
    if (status != CLOPTS_ARGV_INVALID_UTF16)
    {
        error_index = -1;
    }
    args = platform_argv_new(status, 0, error_index, platform_argv_empty);
    return (args == NULL) ? clopts_platform_argv_sentinel() : args;
}

clopts_argv_t *clopts_platform_argv_passthrough(int argc, char *const *argv)
{
    clopts_argv_t *args;

    assert(argc >= 0);
    assert((argv != NULL) || (argc == 0));
    assert((argv == NULL) || (argv[argc] == NULL));
    args = platform_argv_new(CLOPTS_ARGV_OK, argc, -1, (argv == NULL) ? platform_argv_empty : argv);
    return (args == NULL) ? clopts_platform_argv_sentinel() : args;
}

clopts_argv_status_t clopts_argv_status(const clopts_argv_t *args)
{
    return platform_argv_read(args)->status;
}

int clopts_argv_count(const clopts_argv_t *args)
{
    return platform_argv_read(args)->argc;
}

char *const *clopts_argv_values(const clopts_argv_t *args)
{
    return platform_argv_read(args)->argv;
}

int clopts_argv_error_index(const clopts_argv_t *args)
{
    return platform_argv_read(args)->error_index;
}

void clopts_argv_free(clopts_argv_t *args)
{
    /* The handle, and on Windows its array and strings, are one block. */
    if ((args == NULL) || (args == &platform_argv_oom))
    {
        return;
    }
    clopts_core_free(args);
}
