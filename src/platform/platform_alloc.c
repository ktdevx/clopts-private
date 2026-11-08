/*
 * Allocation gateway and allocation probe of the platform module.
 */

#include "platform/platform_internal.h"

void *clopts_platform_malloc(clopts_platform_probe_t *probe, size_t size)
{
    (void)probe;
    (void)size;
    return NULL;
}

void *clopts_platform_malloc_array(clopts_platform_probe_t *probe, size_t count, size_t size)
{
    (void)probe;
    (void)count;
    (void)size;
    return NULL;
}

void clopts_platform_free(clopts_platform_probe_t *probe, void *ptr)
{
    (void)probe;
    (void)ptr;
}
