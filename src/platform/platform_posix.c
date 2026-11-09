/*
 * argv and environment lookup elsewhere than Windows: argv is passed through
 * and variables are read with getenv().
 */

#include "platform/platform_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

clopts_argv_t *clopts_argv_get(int argc, char *const *argv)
{
    assert(argc >= 0);
    assert(argc == 0 || argv != NULL);
    /* Neither the array nor the strings are copied or converted. */
    return clopts_platform_argv_passthrough(argc, argv);
}

clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value)
{
    const char *found;

    /* A NULL context fails without reading name or value. */
    if (ctx == NULL)
    {
        return CLOPTS_ENV_FAILED;
    }
    assert(name != NULL);
    assert(value != NULL);
    clopts_platform_env_clear((clopts_env_t *)ctx);
    found = getenv(name);
    if (found == NULL)
    {
        return CLOPTS_ENV_UNSET;
    }
    /*
     * Borrowed from the process environment; an empty string is FOUND too.
     * Nothing is allocated, so the lookup never fails here.
     */
    *value = found;
    return CLOPTS_ENV_FOUND;
}
