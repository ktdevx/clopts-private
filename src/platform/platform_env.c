/*
 * Environment lookup contexts: creation and release, the failure record and
 * its accessors, and the value buffers.
 */

#include "platform/platform_internal.h"

#include <assert.h>
#include <string.h>

/* Releases the recorded name, if any, through the context's probe. */
static void platform_env_drop_name(clopts_env_t *env)
{
    clopts_platform_free(env->probe, env->error_name);
    env->error_name = NULL;
}

clopts_env_t *clopts_platform_env_new(clopts_platform_probe_t *probe)
{
    clopts_env_t *env;

    env = (clopts_env_t *)clopts_platform_malloc(probe, sizeof(*env));
    if (env == NULL)
    {
        return NULL;
    }
    env->probe = probe;
    env->error = CLOPTS_ENV_ERROR_NONE;
    env->error_name = NULL;
    env->value = NULL;
    env->value_cap = 0;
    env->wide = NULL;
    env->wide_cap = 0;
    return env;
}

void clopts_platform_env_clear(clopts_env_t *env)
{
    assert(env != NULL);
    platform_env_drop_name(env);
    env->error = CLOPTS_ENV_ERROR_NONE;
}

void clopts_platform_env_fail(clopts_env_t *env, clopts_env_error_t kind, const char *name)
{
    size_t bytes;
    char *copy;

    assert(env != NULL);
    assert(kind != CLOPTS_ENV_ERROR_NONE);
    assert(name != NULL);

    platform_env_drop_name(env);
    copy = NULL;
    if (clopts_core_size_add(strlen(name), 1, &bytes) == 0)
    {
        copy = (char *)clopts_platform_malloc(env->probe, bytes);
    }
    if (copy == NULL)
    {
        env->error = CLOPTS_ENV_ERROR_OUT_OF_MEMORY;
        return;
    }
    memcpy(copy, name, bytes);
    env->error = kind;
    env->error_name = copy;
}

int clopts_platform_env_reserve_value(clopts_env_t *env, size_t bytes)
{
    char *grown;

    assert(env != NULL);
    if ((env->value != NULL) && (bytes <= env->value_cap))
    {
        return 0;
    }
    /* The old contents need not survive (7.7), so the buffer is replaced, not copied. */
    grown = (char *)clopts_platform_malloc(env->probe, bytes);
    if (grown == NULL)
    {
        return -1;
    }
    clopts_platform_free(env->probe, env->value);
    env->value = grown;
    env->value_cap = bytes;
    return 0;
}

int clopts_platform_env_reserve_wide(clopts_env_t *env, size_t units)
{
    void *grown;

    assert(env != NULL);
    if ((env->wide != NULL) && (units <= env->wide_cap))
    {
        return 0;
    }
    /* NULL both when units * sizeof(clopts_platform_u16_t) overflows and when the allocation fails. */
    grown = clopts_platform_malloc_array(env->probe, units, sizeof(clopts_platform_u16_t));
    if (grown == NULL)
    {
        return -1;
    }
    clopts_platform_free(env->probe, env->wide);
    env->wide = grown;
    env->wide_cap = units;
    return 0;
}

clopts_env_t *clopts_env_new(void)
{
    return clopts_platform_env_new(NULL);
}

void clopts_env_free(clopts_env_t *env)
{
    clopts_platform_probe_t *probe;

    if (env == NULL)
    {
        return;
    }
    probe = env->probe;
    clopts_platform_free(probe, env->error_name);
    clopts_platform_free(probe, env->value);
    clopts_platform_free(probe, env->wide);
    clopts_platform_free(probe, env);
}

clopts_env_error_t clopts_env_error(const clopts_env_t *env)
{
    return (env == NULL) ? CLOPTS_ENV_ERROR_OUT_OF_MEMORY : env->error;
}

const char *clopts_env_error_name(const clopts_env_t *env)
{
    return (env == NULL) ? NULL : env->error_name;
}
