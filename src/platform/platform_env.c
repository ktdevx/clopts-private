/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Environment lookup contexts: creation and release, the failure record and
 * its accessors, and the value buffers; and the default lookup of the parser.
 *
 * getenv is called only from clopts_platform_getenv_default and from
 * clopts_env_lookup elsewhere than Windows (platform_posix.c); MSVC's
 * deprecation warning for it is silenced by the PRIVATE
 * _CRT_SECURE_NO_WARNINGS definition of the target.
 */

#include "platform/platform_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

clopts_env_status_t clopts_platform_getenv_default(void *ctx, const char *name, const char **value)
{
    const char *found;

    (void)ctx;
    assert(name != NULL);
    assert(value != NULL);

    found = getenv(name);
    if (found == NULL)
    {
        return CLOPTS_ENV_UNSET;
    }
    *value = found;
    return CLOPTS_ENV_FOUND;
}

/* Releases the recorded name, if any. */
static void platform_env_drop_name(clopts_env_t *env)
{
    clopts_core_free(env->error_name);
    env->error_name = NULL;
}

clopts_env_t *clopts_env_create(void)
{
    clopts_env_t *env;

    env = (clopts_env_t *)clopts_core_malloc(sizeof(*env));
    if (env == NULL)
    {
        return NULL;
    }
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
        copy = (char *)clopts_core_malloc(bytes);
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
    grown = (char *)clopts_core_malloc(bytes);
    if (grown == NULL)
    {
        return -1;
    }
    clopts_core_free(env->value);
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
    grown = clopts_core_malloc_array(units, sizeof(clopts_platform_u16_t));
    if (grown == NULL)
    {
        return -1;
    }
    clopts_core_free(env->wide);
    env->wide = grown;
    env->wide_cap = units;
    return 0;
}

void clopts_env_destroy(clopts_env_t *env)
{
    if (env == NULL)
    {
        return;
    }
    clopts_core_free(env->error_name);
    clopts_core_free(env->value);
    clopts_core_free(env->wide);
    clopts_core_free(env);
}

clopts_env_error_t clopts_env_get_error(const clopts_env_t *env)
{
    return (env == NULL) ? CLOPTS_ENV_ERROR_OUT_OF_MEMORY : env->error;
}

const char *clopts_env_get_error_name(const clopts_env_t *env)
{
    return (env == NULL) ? NULL : env->error_name;
}
