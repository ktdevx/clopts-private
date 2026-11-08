/*
 * argv and environment lookup elsewhere than Windows: argv is passed through
 * and variables are read with getenv().
 */

#include "platform/platform_internal.h"

clopts_argv_t *clopts_argv_get(int argc, char *const *argv)
{
    (void)argc;
    (void)argv;
    return clopts_platform_argv_sentinel();
}

clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_FAILED;
}
