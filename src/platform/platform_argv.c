/*
 * argv handles: the out-of-memory sentinel, failure and passthrough handles,
 * the public accessors and the release function.
 */

#include "platform/platform_internal.h"

/* The values of every handle without elements: { NULL }. */
static char *const platform_argv_empty[1] = { NULL };

/* Shared out-of-memory sentinel; never written, so it may be shared by threads. */
static const clopts_argv_t platform_argv_oom = { CLOPTS_ARGV_OUT_OF_MEMORY, 0, -1, platform_argv_empty };

clopts_argv_t *clopts_platform_argv_sentinel(void)
{
    return (clopts_argv_t *)&platform_argv_oom;
}

clopts_argv_t *clopts_platform_argv_failure(clopts_platform_probe_t *probe, clopts_argv_status_t status,
                                            int error_index)
{
    (void)probe;
    (void)status;
    (void)error_index;
    return clopts_platform_argv_sentinel();
}

clopts_argv_t *clopts_platform_argv_passthrough(clopts_platform_probe_t *probe, int argc, char *const *argv)
{
    (void)probe;
    (void)argc;
    (void)argv;
    return clopts_platform_argv_sentinel();
}

void clopts_platform_argv_free(clopts_platform_probe_t *probe, clopts_argv_t *args)
{
    (void)probe;
    (void)args;
}

clopts_argv_status_t clopts_argv_status(const clopts_argv_t *args)
{
    (void)args;
    return CLOPTS_ARGV_OUT_OF_MEMORY;
}

int clopts_argv_count(const clopts_argv_t *args)
{
    (void)args;
    return 0;
}

char *const *clopts_argv_values(const clopts_argv_t *args)
{
    (void)args;
    return platform_argv_empty;
}

int clopts_argv_error_index(const clopts_argv_t *args)
{
    (void)args;
    return -1;
}

void clopts_argv_free(clopts_argv_t *args)
{
    clopts_platform_argv_free(NULL, args);
}
