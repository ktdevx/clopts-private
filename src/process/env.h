/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PROCESS_ENV_H
#define CLOPTS_SRC_PROCESS_ENV_H

#include "clopts/process/env.h"

#include <stddef.h>

/*
 * The process environment as a source of environment values: the lookup
 * clopts_parse() uses (env_posix.c, env_win32.c) and the buffers of the
 * values it lends (env.c). How the lookup grows the buffers is in
 * process/env_impl.h.
 */

/*
 * Buffers of the values one lookup context lends. One parse owns one (on its
 * stack) for the duration of the resolution, so parses never share one.
 * Nothing is allocated until a lookup needs it, and nothing at all elsewhere
 * than Windows.
 */
typedef struct clopts_env
{
    char *value;                      /* UTF-8 value buffer (Windows) */
    size_t value_cap;
    void *wide;                       /* UTF-16 work buffer (Windows): name and value */
    size_t wide_cap;                  /* in clopts_utf16_t units */
} clopts_env_t;

/* Makes env empty: no buffer. */
void clopts_env_init(clopts_env_t *env);
/* Releases the buffers of env, leaving it empty. Every value it lent becomes invalid. */
void clopts_env_release(clopts_env_t *env);

/*
 * The lookup in the process environment, a clopts_getenv_fn whose ctx is a
 * clopts_env_t *. Stores the UTF-8 value of name in *value: on Windows read
 * with GetEnvironmentVariableW() and converted into env->value (valid until
 * the next lookup with env or its release); elsewhere getenv()'s pointer as
 * is. Returns:
 *   - CLOPTS_ENV_FOUND, an empty value included;
 *   - CLOPTS_ENV_UNSET, also for a name that cannot be converted to UTF-16
 *     (no variable can have it; the builder rejects such names anyway);
 *   - CLOPTS_ENV_INVALID: Windows, the value is not valid UTF-16;
 *   - CLOPTS_ENV_FAILED: an allocation or the system call failed.
 */
clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value);

#endif /* CLOPTS_SRC_PROCESS_ENV_H */
