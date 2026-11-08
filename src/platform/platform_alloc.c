/*
 * Allocation gateway and allocation probe of the platform module.
 */

#include "platform/platform_internal.h"

#include <stdbool.h>
#include <stdlib.h>

/*
 * Passes one fault point. Returns true when the allocation may go ahead and
 * false when the probe makes it fail. Always true without a probe.
 */
static bool platform_alloc_pass(clopts_platform_probe_t *probe)
{
    if (probe == NULL)
    {
        return true;
    }

    probe->calls++;
    if ((probe->fail_at != 0) && (probe->calls >= probe->fail_at))
    {
        return false;
    }
    return true;
}

/* Allocates bytes (0 as 1 byte) and counts the block as live when it is not NULL. */
static void *platform_alloc_bytes(clopts_platform_probe_t *probe, size_t bytes)
{
    void *ptr;

    ptr = malloc((bytes == 0) ? 1 : bytes);
    if ((probe != NULL) && (ptr != NULL))
    {
        probe->live++;
    }
    return ptr;
}

void *clopts_platform_malloc(clopts_platform_probe_t *probe, size_t size)
{
    if (!platform_alloc_pass(probe))
    {
        return NULL;
    }
    return platform_alloc_bytes(probe, size);
}

/*
 * The attempt passes the fault point before the overflow check, as in the
 * parser's gateway, so an overflow counts as one attempt in calls.
 */
void *clopts_platform_malloc_array(clopts_platform_probe_t *probe, size_t count, size_t size)
{
    size_t bytes;

    if (!platform_alloc_pass(probe))
    {
        return NULL;
    }
    if (clopts_core_size_mul(count, size, &bytes) != 0)
    {
        return NULL;
    }
    return platform_alloc_bytes(probe, bytes);
}

void clopts_platform_free(clopts_platform_probe_t *probe, void *ptr)
{
    if ((probe != NULL) && (ptr != NULL))
    {
        probe->live--;
    }
    free(ptr);
}
