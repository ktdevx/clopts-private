/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PROCESS_UTF16_H
#define CLOPTS_SRC_PROCESS_UTF16_H

#include <stddef.h>
#include <stdint.h>

/* One UTF-16 code unit. */
#ifdef _WIN32
typedef wchar_t clopts_utf16_t;   /* 16-bit on Windows */
#else
typedef uint16_t clopts_utf16_t;
#endif

#ifdef _WIN32
/*
 * Conversion from UTF-16 to UTF-8 on Windows (utf16_win32.c). Invalid UTF-16
 * (an unpaired surrogate) is never replaced: it is reported.
 */

/* Outcome of clopts_utf16_measure(). */
typedef enum clopts_utf16_status
{
    CLOPTS_UTF16_OK,
    CLOPTS_UTF16_INVALID,   /* not valid UTF-16 */
    CLOPTS_UTF16_FAILED     /* any other failure; treated as out of memory */
} clopts_utf16_status_t;

/* The length of s (NUL-terminated) in code units as an int; -1 when it does not fit. */
int clopts_utf16_length(const wchar_t *s);

/* Stores in *bytes the UTF-8 size of the len code units at s (len > 0), without a NUL. */
clopts_utf16_status_t clopts_utf16_measure(const wchar_t *s, int len, size_t *bytes);

/*
 * Converts the len code units at s (len > 0) to UTF-8 into out, which holds
 * room bytes (more than INT_MAX is used as INT_MAX). No NUL is added. Returns
 * the number of bytes written, or 0 when the conversion fails.
 */
int clopts_utf16_to_utf8(const wchar_t *s, int len, char *out, size_t room);
#endif

#endif /* CLOPTS_SRC_PROCESS_UTF16_H */
