/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PROCESS_ENV_IMPL_H
#define CLOPTS_SRC_PROCESS_ENV_IMPL_H

#include "process/env.h"

#include <stddef.h>

/*
 * The implementation of the process environment lookup for the files that
 * implement it: env.c holds the buffers, env_win32.c grows them while it
 * converts a value. Only files of the process module include it
 * (docs/dev/design.md, chapter 3).
 */

/* Grows env->value to at least bytes (never shrinks). 0 on success, -1 on overflow or allocation failure. */
int clopts_env_reserve_value(clopts_env_t *env, size_t bytes);
/* Grows env->wide to at least units code units. Same return values. */
int clopts_env_reserve_wide(clopts_env_t *env, size_t units);

#endif /* CLOPTS_SRC_PROCESS_ENV_IMPL_H */
