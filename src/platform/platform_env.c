/*
 * Environment lookup contexts: creation and release, the failure record and
 * its accessors, and the value buffers.
 */

#include "platform/platform_internal.h"

clopts_env_t *clopts_platform_env_new(clopts_platform_probe_t *probe)
{
    (void)probe;
    return NULL;
}

void clopts_platform_env_clear(clopts_env_t *env)
{
    (void)env;
}

void clopts_platform_env_fail(clopts_env_t *env, clopts_env_error_t kind, const char *name)
{
    (void)env;
    (void)kind;
    (void)name;
}

int clopts_platform_env_reserve_value(clopts_env_t *env, size_t bytes)
{
    (void)env;
    (void)bytes;
    return -1;
}

int clopts_platform_env_reserve_wide(clopts_env_t *env, size_t units)
{
    (void)env;
    (void)units;
    return -1;
}

clopts_env_t *clopts_env_new(void)
{
    return clopts_platform_env_new(NULL);
}

void clopts_env_free(clopts_env_t *env)
{
    (void)env;
}

clopts_env_error_t clopts_env_error(const clopts_env_t *env)
{
    (void)env;
    return CLOPTS_ENV_ERROR_OUT_OF_MEMORY;
}

const char *clopts_env_error_name(const clopts_env_t *env)
{
    (void)env;
    return NULL;
}
