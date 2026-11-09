/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite convert (Windows only; built only on Windows): clopts_argv_create_from_wide,
 * conversion from a command line, non-ASCII elements and the index of the
 * first invalid UTF-16 element. The allocations are counted with the test
 * allocator (tests/support/test_alloc.h).
 *
 * It also checks that the helpers of test_util give the same code units as
 * wide string literals, which the inputs of this suite mix.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include "test_alloc.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

/* "テスト.txt" in UTF-16 and in UTF-8. */
static const uint16_t convert_test_txt_u16[] = {0x30C6, 0x30B9, 0x30C8, 0x002E, 0x0074, 0x0078, 0x0074};
static const char convert_test_txt_u8[] = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt";

/* Number of elements of a static array. */
#define CONVERT_COUNT(a) (sizeof(a) / sizeof((a)[0]))

NAOS_TEST(u16_helpers_match_wide_literals)
{
    platform_test_u16_t s;

    platform_test_u16_from_ascii(&s, "a \"b\"");
    NAOS_CHECK_UINT_EQ(s.len, wcslen(L"a \"b\""));
    NAOS_CHECK_INT_EQ(wcscmp(s.units, L"a \"b\""), 0);

    platform_test_u16_from_units(&s, convert_test_txt_u16, CONVERT_COUNT(convert_test_txt_u16));
    NAOS_CHECK_UINT_EQ(s.len, wcslen(L"\u30c6\u30b9\u30c8.txt"));
    NAOS_CHECK_INT_EQ(wcscmp(s.units, L"\u30c6\u30b9\u30c8.txt"), 0);
}

/* Checks the accessors of a successful handle against expected (a NULL-terminated list of UTF-8 strings). */
static void convert_check_ok(const clopts_argv_t *args, const char *const *expected)
{
    char *const *values;
    int count = 0;
    int i;

    while (expected[count] != NULL)
    {
        ++count;
    }
    NAOS_REQUIRE_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_get_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_get_error_index(args), -1);
    NAOS_REQUIRE_INT_EQ(clopts_argv_get_count(args), count);
    values = clopts_argv_get_values(args);
    NAOS_REQUIRE_NOT_NULL(values);
    for (i = 0; i < count; ++i)
    {
        NAOS_REQUIRE_NOT_NULL(values[i]);
        /* exact bytes, the terminating NUL included */
        NAOS_CHECK_MEM_EQ(values[i], strlen(values[i]) + 1, expected[i], strlen(expected[i]) + 1);
    }
    NAOS_CHECK_NULL(values[count]);
}

/* Checks the accessors of an INVALID_UTF16 handle. */
static void convert_check_invalid(const clopts_argv_t *args, int index)
{
    char *const *values;

    NAOS_REQUIRE_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_get_status(args), CLOPTS_ARGV_INVALID_UTF16);
    NAOS_CHECK_INT_EQ(clopts_argv_get_error_index(args), index);
    NAOS_CHECK_INT_EQ(clopts_argv_get_count(args), 0);
    values = clopts_argv_get_values(args);
    NAOS_REQUIRE_NOT_NULL(values);
    NAOS_CHECK_NULL(values[0]);
}

/* ---- clopts_argv_create_from_wide / clopts_platform_argv_convert ---------------- */

NAOS_TEST(from_wide_converts_japanese_to_utf8)
{
    static const uint16_t emoji[] = {0x0078, 0xD83D, 0xDE00};   /* "x" U+1F600 (a surrogate pair) */
    platform_test_u16_t a0;
    platform_test_u16_t a1;
    platform_test_u16_t a2;
    wchar_t *wargv[4];
    static const char *const expected[] = {"tool", convert_test_txt_u8, "x\xf0\x9f\x98\x80", NULL};
    clopts_argv_t *args;

    wargv[0] = platform_test_u16_from_ascii(&a0, "tool");
    wargv[1] = platform_test_u16_from_units(&a1, convert_test_txt_u16, CONVERT_COUNT(convert_test_txt_u16));
    wargv[2] = platform_test_u16_from_units(&a2, emoji, CONVERT_COUNT(emoji));
    wargv[3] = NULL;
    test_alloc_begin(0);
    args = clopts_argv_create_from_wide(3, wargv);
    convert_check_ok(args, expected);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
    /* wargv is not written */
    NAOS_CHECK_INT_EQ(wcscmp(wargv[1], L"\u30c6\u30b9\u30c8.txt"), 0);
}

NAOS_TEST(from_wide_keeps_empty_elements)
{
    wchar_t e0[] = L"";
    wchar_t e1[] = L"tool";
    wchar_t e2[] = L"";
    wchar_t *wargv[4];
    static const char *const expected[] = {"", "tool", "", NULL};
    clopts_argv_t *args;

    wargv[0] = e0;
    wargv[1] = e1;
    wargv[2] = e2;
    wargv[3] = NULL;
    test_alloc_begin(0);
    args = clopts_argv_create_from_wide(3, wargv);
    convert_check_ok(args, expected);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(from_wide_with_argc_zero)
{
    wchar_t *wargv[1] = { NULL };
    static const char *const expected[] = {NULL};
    clopts_argv_t *args;

    test_alloc_begin(0);
    args = clopts_argv_create_from_wide(0, wargv);
    convert_check_ok(args, expected);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* The handle, the pointer array and the strings are one allocation. */
NAOS_TEST(convert_allocates_one_block)
{
    platform_test_u16_t a0;
    platform_test_u16_t a1;
    const wchar_t *wargs[2];
    static const char *const expected[] = {"tool", convert_test_txt_u8, NULL};
    clopts_argv_t *args;

    wargs[0] = platform_test_u16_from_ascii(&a0, "tool");
    wargs[1] = platform_test_u16_from_units(&a1, convert_test_txt_u16, CONVERT_COUNT(convert_test_txt_u16));
    test_alloc_begin(0);
    args = clopts_platform_argv_convert(2, wargs);
    convert_check_ok(args, expected);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Converts the single element units[0..count-1] (after "tool") and expects
 * INVALID_UTF16 at index 1. The failure handle is the only allocation: the
 * invalid element is found before the block is allocated.
 */
static void convert_check_single_invalid(const uint16_t *units, size_t count)
{
    platform_test_u16_t a0;
    platform_test_u16_t a1;
    const wchar_t *wargs[2];
    clopts_argv_t *args;

    wargs[0] = platform_test_u16_from_ascii(&a0, "tool");
    wargs[1] = platform_test_u16_from_units(&a1, units, count);
    test_alloc_begin(0);
    args = clopts_platform_argv_convert(2, wargs);
    convert_check_invalid(args, 1);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(lone_high_surrogate_is_invalid)
{
    static const uint16_t alone[] = {0xD800};
    static const uint16_t at_end[] = {0x0061, 0xD83D};
    static const uint16_t before_ascii[] = {0xDBFF, 0x0061};

    convert_check_single_invalid(alone, CONVERT_COUNT(alone));
    convert_check_single_invalid(at_end, CONVERT_COUNT(at_end));
    convert_check_single_invalid(before_ascii, CONVERT_COUNT(before_ascii));
}

NAOS_TEST(lone_low_surrogate_is_invalid)
{
    static const uint16_t alone[] = {0xDC00};
    static const uint16_t in_middle[] = {0x0061, 0xDFFF, 0x0062};
    static const uint16_t reversed_pair[] = {0xDE00, 0xD83D};

    convert_check_single_invalid(alone, CONVERT_COUNT(alone));
    convert_check_single_invalid(in_middle, CONVERT_COUNT(in_middle));
    convert_check_single_invalid(reversed_pair, CONVERT_COUNT(reversed_pair));
}

/* Elements 1 and 3 are invalid: the smaller index is reported, and nothing is replaced. */
NAOS_TEST(smallest_invalid_index_is_reported)
{
    static const uint16_t high[] = {0x0061, 0xD800};
    static const uint16_t low[] = {0xDC00};
    platform_test_u16_t a[5];
    wchar_t *wargv[6];
    clopts_argv_t *args;

    wargv[0] = platform_test_u16_from_ascii(&a[0], "tool");
    wargv[1] = platform_test_u16_from_units(&a[1], high, CONVERT_COUNT(high));
    wargv[2] = platform_test_u16_from_units(&a[2], convert_test_txt_u16, CONVERT_COUNT(convert_test_txt_u16));
    wargv[3] = platform_test_u16_from_units(&a[3], low, CONVERT_COUNT(low));
    wargv[4] = platform_test_u16_from_ascii(&a[4], "");
    wargv[5] = NULL;
    test_alloc_begin(0);
    args = clopts_argv_create_from_wide(5, wargv);
    convert_check_invalid(args, 1);
    clopts_argv_destroy(args);

    /* Only the later one invalid: its index. */
    wargv[1] = platform_test_u16_from_ascii(&a[1], "ok");
    args = clopts_argv_create_from_wide(5, wargv);
    convert_check_invalid(args, 3);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(invalid_argv0_reports_index_zero)
{
    static const uint16_t high[] = {0xD800};
    platform_test_u16_t a0;
    wchar_t *wargv[2];
    clopts_argv_t *args;

    wargv[0] = platform_test_u16_from_units(&a0, high, CONVERT_COUNT(high));
    wargv[1] = NULL;
    test_alloc_begin(0);
    args = clopts_argv_create_from_wide(1, wargv);
    convert_check_invalid(args, 0);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_platform_argv_from_cmdline ------------------------------------- */

NAOS_TEST(from_cmdline_splits_and_converts_non_ascii)
{
    /* tool "テスト.txt x" -n テスト.txt */
    static const uint16_t cmdline_units[] = {
        0x0074, 0x006F, 0x006F, 0x006C, 0x0020, 0x0022, 0x30C6, 0x30B9, 0x30C8, 0x002E, 0x0074, 0x0078,
        0x0074, 0x0020, 0x0078, 0x0022, 0x0020, 0x002D, 0x006E, 0x0020, 0x30C6, 0x30B9, 0x30C8, 0x002E,
        0x0074, 0x0078, 0x0074};
    static const char *const expected[] = {
        "tool", "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt x", "-n", convert_test_txt_u8, NULL};
    platform_test_u16_t cmdline;
    clopts_argv_t *args;

    platform_test_u16_from_units(&cmdline, cmdline_units, CONVERT_COUNT(cmdline_units));
    test_alloc_begin(0);
    args = clopts_platform_argv_from_cmdline(cmdline.units);
    convert_check_ok(args, expected);
    /* the work block, then the handle block; the work block is released: only the handle stays */
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 2);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(from_cmdline_follows_the_crt_rules)
{
    static const char *const expected[] = {"C:\\Program Files\\x", "a\\\"b", "", "foo\"bar", NULL};
    platform_test_u16_t cmdline;
    clopts_argv_t *args;

    platform_test_u16_from_ascii(&cmdline, "\"C:\\Program Files\\x\" a\\\\\\\"b \"\" \"foo\"\"bar\"  ");
    test_alloc_begin(0);
    args = clopts_platform_argv_from_cmdline(cmdline.units);
    convert_check_ok(args, expected);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(from_cmdline_reports_the_invalid_element)
{
    /* tool a "b<U+D800>c" <U+DC00> */
    static const uint16_t cmdline_units[] = {
        0x0074, 0x006F, 0x006F, 0x006C, 0x0020, 0x0061, 0x0020, 0x0022, 0x0062, 0xD800, 0x0063, 0x0022,
        0x0020, 0xDC00};
    platform_test_u16_t cmdline;
    clopts_argv_t *args;

    platform_test_u16_from_units(&cmdline, cmdline_units, CONVERT_COUNT(cmdline_units));
    test_alloc_begin(0);
    args = clopts_platform_argv_from_cmdline(cmdline.units);
    convert_check_invalid(args, 2);
    /* the work block, then the failure handle; the handle stays, the work block is released */
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 2);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(from_cmdline_empty_gives_one_empty_element)
{
    static const char *const expected[] = {"", NULL};
    clopts_argv_t *args;

    test_alloc_begin(0);
    args = clopts_platform_argv_from_cmdline(L"");
    convert_check_ok(args, expected);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(from_cmdline_null_is_cmdline_unavailable)
{
    clopts_argv_t *args;
    char *const *values;

    test_alloc_begin(0);
    args = clopts_platform_argv_from_cmdline(NULL);
    NAOS_REQUIRE_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_get_status(args), CLOPTS_ARGV_CMDLINE_UNAVAILABLE);
    NAOS_CHECK_INT_EQ(clopts_argv_get_error_index(args), -1);
    NAOS_CHECK_INT_EQ(clopts_argv_get_count(args), 0);
    values = clopts_argv_get_values(args);
    NAOS_REQUIRE_NOT_NULL(values);
    NAOS_CHECK_NULL(values[0]);
    /* only the failure handle is allocated */
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
    NAOS_CHECK_INT_EQ(test_alloc.live, 1);
    clopts_argv_destroy(args);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_SUITE(convert,
    NAOS_ADD_TEST(u16_helpers_match_wide_literals),
    NAOS_ADD_TEST(from_wide_converts_japanese_to_utf8),
    NAOS_ADD_TEST(from_wide_keeps_empty_elements),
    NAOS_ADD_TEST(from_wide_with_argc_zero),
    NAOS_ADD_TEST(convert_allocates_one_block),
    NAOS_ADD_TEST(lone_high_surrogate_is_invalid),
    NAOS_ADD_TEST(lone_low_surrogate_is_invalid),
    NAOS_ADD_TEST(smallest_invalid_index_is_reported),
    NAOS_ADD_TEST(invalid_argv0_reports_index_zero),
    NAOS_ADD_TEST(from_cmdline_splits_and_converts_non_ascii),
    NAOS_ADD_TEST(from_cmdline_follows_the_crt_rules),
    NAOS_ADD_TEST(from_cmdline_reports_the_invalid_element),
    NAOS_ADD_TEST(from_cmdline_empty_gives_one_empty_element),
    NAOS_ADD_TEST(from_cmdline_null_is_cmdline_unavailable)
)
