/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_BASE_UTF8_H
#define CLOPTS_SRC_BASE_UTF8_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Strict UTF-8: shortest forms only, no surrogates (U+D800..U+DFFF) and
 * nothing above U+10FFFF.
 */

/*
 * Length in bytes of the valid sequence that starts at data (at most avail
 * bytes, avail > 0), or 0 when no valid sequence starts there.
 */
size_t clopts_utf8_sequence(const char *data, size_t avail);

/* True when the bytes of s up to its NUL are valid UTF-8. s must not be NULL. */
bool clopts_utf8_valid(const char *s);

/*
 * The number of code points of the len bytes at data. A valid sequence
 * counts as 1, and so does every byte that is not part of one.
 */
size_t clopts_utf8_count(const char *data, size_t len);

#endif /* CLOPTS_SRC_BASE_UTF8_H */
