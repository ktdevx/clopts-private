/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Process environment lookup contexts: their initialization, release and
 * value buffers. The lookup itself is in win32.c or posix.c.
 */

#include "process/process_internal.h"

#include <assert.h>

void clopts_process_env_init(clopts_process_env_ctx_t *env)
{
    assert(env != NULL);
    env->value = NULL;
    env->value_cap = 0;
    env->wide = NULL;
    env->wide_cap = 0;
}

void clopts_process_env_release(clopts_process_env_ctx_t *env)
{
    assert(env != NULL);
    clopts_mem_free(env->value);
    clopts_mem_free(env->wide);
    clopts_process_env_init(env);
}

int clopts_process_env_reserve_value(clopts_process_env_ctx_t *env, size_t bytes)
{
    char *grown;

    assert(env != NULL);
    if ((env->value != NULL) && (bytes <= env->value_cap))
    {
        return 0;
    }
    /* The old contents need not survive, so the buffer is replaced, not copied. */
    grown = (char *)clopts_mem_alloc(bytes);
    if (grown == NULL)
    {
        return -1;
    }
    clopts_mem_free(env->value);
    env->value = grown;
    env->value_cap = bytes;
    return 0;
}

int clopts_process_env_reserve_wide(clopts_process_env_ctx_t *env, size_t units)
{
    void *grown;

    assert(env != NULL);
    if ((env->wide != NULL) && (units <= env->wide_cap))
    {
        return 0;
    }
    /* NULL both when units * sizeof(clopts_process_u16_t) overflows and when the allocation fails. */
    grown = clopts_mem_alloc_array(units, sizeof(clopts_process_u16_t));
    if (grown == NULL)
    {
        return -1;
    }
    clopts_mem_free(env->wide);
    env->wide = grown;
    env->wide_cap = units;
    return 0;
}
