/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_BASE_SIZE_H
#define CLOPTS_SRC_BASE_SIZE_H

#include <stddef.h>

/*
 * Overflow-checked size arithmetic.
 *
 * Both functions return 0 and store the result in *out on success. On
 * overflow they return -1 and leave *out unchanged. out must not be NULL.
 */
int clopts_size_mul(size_t a, size_t b, size_t *out);
int clopts_size_add(size_t a, size_t b, size_t *out);

#endif /* CLOPTS_SRC_BASE_SIZE_H */
