/*
 * Shared helpers for the platform tests: NUL-terminated arrays of
 * clopts_platform_u16_t built from an ASCII string or from a sequence of
 * code units, and the comparison of split results with an expected list of
 * ASCII strings.
 *
 * The arrays live in a fixed buffer owned by the caller (usually a local
 * platform_test_u16_t); nothing is allocated and nothing has to be freed.
 * Contract violations (non-ASCII input, input that does not fit) are
 * asserted.
 */

#ifndef CLOPTS_TESTS_PLATFORM_TEST_UTIL_H
#define CLOPTS_TESTS_PLATFORM_TEST_UTIL_H

#include "platform/platform_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Capacity of a platform_test_u16_t in code units, the terminating NUL included. */
#define PLATFORM_TEST_U16_CAP 1024

/* A NUL-terminated array of code units: units[len] == 0. */
typedef struct platform_test_u16
{
    clopts_platform_u16_t units[PLATFORM_TEST_U16_CAP];
    size_t len;   /* code units before the terminating NUL */
} platform_test_u16_t;

/*
 * Fills out with the bytes of ascii (each < 0x80), one code unit each, and
 * a terminating NUL. Returns out->units.
 */
clopts_platform_u16_t *platform_test_u16_from_ascii(platform_test_u16_t *out, const char *ascii);

/*
 * Fills out with units[0..count-1] (any values, 0 included) and a
 * terminating NUL. units may be NULL when count is 0. Returns out->units.
 */
clopts_platform_u16_t *platform_test_u16_from_units(platform_test_u16_t *out, const uint16_t *units,
                                                    size_t count);

/* True when the NUL-terminated s has exactly the bytes of ascii, one code unit each. */
bool platform_test_u16_eq_ascii(const clopts_platform_u16_t *s, const char *ascii);

/*
 * Compares the split result args[0..argc-1] (each NUL-terminated) with
 * expected, a NULL-terminated list of ASCII strings. Returns -1 when they
 * have the same count and every element is equal; otherwise the index of
 * the first element that differs, or the shorter count when one list is a
 * prefix of the other.
 */
long platform_test_split_mismatch(clopts_platform_u16_t *const *args, size_t argc,
                                  const char *const *expected);

#endif /* CLOPTS_TESTS_PLATFORM_TEST_UTIL_H */
