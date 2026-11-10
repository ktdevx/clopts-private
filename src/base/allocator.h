/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_BASE_ALLOCATOR_H
#define CLOPTS_SRC_BASE_ALLOCATOR_H

#include "clopts/base/allocator.h"

/*
 * The allocator in use: the one set with clopts_set_allocator(), or the
 * standard functions. Never NULL. Only the allocation gateway (mem.c) calls
 * its functions.
 */
const clopts_allocator_t *clopts_allocator_get(void);

#endif /* CLOPTS_SRC_BASE_ALLOCATOR_H */
