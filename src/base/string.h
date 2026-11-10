/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_BASE_STRING_H
#define CLOPTS_SRC_BASE_STRING_H

#include <stddef.h>

/*
 * Owned strings.
 *
 * The copies are allocated through the allocation gateway (base/memory.h)
 * and released with clopts_free().
 */

/* Returns a copy of s, or NULL on failure. s == NULL gives NULL without allocating. */
char *clopts_strdup(const char *s);

/*
 * Returns a NUL-terminated copy of the len bytes at s, or NULL. s must not be
 * NULL and must point to at least len bytes.
 */
char *clopts_strndup(const char *s, size_t len);

/*
 * Replaces the owned string *field with a copy of value (NULL resets it to
 * NULL), releasing the old one. Returns 0, or -1 when the copy cannot be
 * allocated, leaving *field unchanged.
 */
int clopts_strrep(char **field, const char *value);

#endif /* CLOPTS_SRC_BASE_STRING_H */
