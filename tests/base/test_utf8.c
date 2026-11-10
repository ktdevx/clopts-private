/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite utf8: the strict UTF-8 reader (clopts_utf8_sequence,
 * clopts_utf8_valid, clopts_utf8_count).
 */

#include "base/utf8.h"

#include <naos/naos.h>

#include <stddef.h>

NAOS_TEST(sequence_gives_the_length_of_a_valid_sequence)
{
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("a", 1), 1);
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xC3\xAF", 2), 2);
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xE6\x97\xA5", 3), 3);
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xF0\x9F\x98\x80", 4), 4);
    /* only the first sequence */
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xC3\xAF" "b", 3), 2);
}

NAOS_TEST(sequence_gives_zero_where_no_valid_sequence_starts)
{
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\x80", 1), 0);
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xC0\xAF", 2), 0);
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xED\xA0\x80", 3), 0);
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xF4\x90\x80\x80", 4), 0);
    /* cut by avail */
    NAOS_CHECK_UINT_EQ(clopts_utf8_sequence("\xE6\x97\xA5", 2), 0);
}

NAOS_TEST(valid_accepts_strict_utf8_only)
{
    NAOS_CHECK_TRUE(clopts_utf8_valid(""));
    NAOS_CHECK_TRUE(clopts_utf8_valid("PATH"));
    NAOS_CHECK_TRUE(clopts_utf8_valid("na\xC3\xAF" "ve"));
    NAOS_CHECK_TRUE(clopts_utf8_valid("\xF4\x8F\xBF\xBF"));
    NAOS_CHECK_FALSE(clopts_utf8_valid("\xFF"));
    NAOS_CHECK_FALSE(clopts_utf8_valid("a\xC0\xAF"));
    NAOS_CHECK_FALSE(clopts_utf8_valid("\xED\xB0\x80"));
    NAOS_CHECK_FALSE(clopts_utf8_valid("\xF4\x90\x80\x80"));
    /* a sequence cut by the NUL */
    NAOS_CHECK_FALSE(clopts_utf8_valid("\xE6\x97"));
}

/* Code points of a string literal, embedded NULs included. */
#define COUNT_OF(lit) clopts_utf8_count((lit), sizeof(lit) - 1)

NAOS_TEST(count_counts_ascii_bytes)
{
    NAOS_CHECK_UINT_EQ(clopts_utf8_count("", 0), 0);
    NAOS_CHECK_UINT_EQ(COUNT_OF("a"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("--verbose"), 9);
    NAOS_CHECK_UINT_EQ(COUNT_OF("a\0b"), 3);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\x7F"), 1);
    /* only len bytes are looked at */
    NAOS_CHECK_UINT_EQ(clopts_utf8_count("abcdef", 2), 2);
}

NAOS_TEST(count_counts_a_valid_sequence_as_one)
{
    /* two bytes: U+0080, U+00EF, U+07FF */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xC2\x80"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xC3\xAF"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xDF\xBF"), 1);
    /* three bytes: U+0800, U+65E5, U+D7FF, U+E000, U+FFFF */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE0\xA0\x80"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE6\x97\xA5"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xED\x9F\xBF"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xEE\x80\x80"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xEF\xBF\xBF"), 1);
    /* four bytes: U+10000, U+1F600, U+10FFFF */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF0\x90\x80\x80"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF0\x9F\x98\x80"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF4\x8F\xBF\xBF"), 1);
    /* mixed with ASCII */
    NAOS_CHECK_UINT_EQ(COUNT_OF("a\xC3\xAF\xE6\x97\xA5\xF0\x9F\x98\x80" "z"), 5);
    NAOS_CHECK_UINT_EQ(COUNT_OF("na\xC3\xAF" "ve"), 5);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"), 3);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE5\x80\xA4"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB"), 4);
}

NAOS_TEST(count_counts_each_byte_of_an_overlong_form)
{
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xC0\x80"), 2);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xC0\xAF"), 2);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xC1\xBF"), 2);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE0\x80\xAF"), 3);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE0\x9F\xBF"), 3);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF0\x80\x80\xAF"), 4);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF0\x8F\xBF\xBF"), 4);
}

NAOS_TEST(count_counts_each_byte_of_a_surrogate)
{
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xED\xA0\x80"), 3); /* U+D800 */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xED\xAF\xBF"), 3); /* U+DBFF */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xED\xB0\x80"), 3); /* U+DC00 */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xED\xBF\xBF"), 3); /* U+DFFF */
}

NAOS_TEST(count_counts_each_byte_above_u10ffff)
{
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF4\x90\x80\x80"), 4); /* U+110000 */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF5\x80\x80\x80"), 4);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF7\xBF\xBF\xBF"), 4);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF8\x88\x80\x80\x80"), 5);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xFE"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xFF"), 1);
}

NAOS_TEST(count_counts_each_byte_of_a_truncated_sequence)
{
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xC3"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE6\x97"), 2);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF0\x9F\x98"), 3);
    /* cut by len, not by a NUL */
    NAOS_CHECK_UINT_EQ(clopts_utf8_count("\xE6\x97\xA5", 2), 2);
    /* interrupted by an ASCII byte or by the lead byte of a valid sequence */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE6\x97" "E"), 3);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF0\x9F" "a"), 3);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE6\xC3\xAF"), 2);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xF0\x9F\x98\xE6\x97\xA5"), 4);
}

NAOS_TEST(count_counts_each_lone_continuation_byte)
{
    NAOS_CHECK_UINT_EQ(COUNT_OF("\x80"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xBF"), 1);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\x80\x80\x80"), 3);
    NAOS_CHECK_UINT_EQ(COUNT_OF("a\x80" "b"), 3);
    /* one too many after a valid sequence */
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xC3\xAF\xAF"), 2);
    NAOS_CHECK_UINT_EQ(COUNT_OF("\xE6\x97\xA5\x97\xA5"), 3);
}

NAOS_TEST(count_of_a_mix_of_invalid_bytes)
{
    NAOS_CHECK_UINT_EQ(COUNT_OF("B\xFF\xC0\xAF\xE6\x97" "E"), 7);
}

NAOS_SUITE(utf8,
    NAOS_ADD_TEST(sequence_gives_the_length_of_a_valid_sequence),
    NAOS_ADD_TEST(sequence_gives_zero_where_no_valid_sequence_starts),
    NAOS_ADD_TEST(valid_accepts_strict_utf8_only),
    NAOS_ADD_TEST(count_counts_ascii_bytes),
    NAOS_ADD_TEST(count_counts_a_valid_sequence_as_one),
    NAOS_ADD_TEST(count_counts_each_byte_of_an_overlong_form),
    NAOS_ADD_TEST(count_counts_each_byte_of_a_surrogate),
    NAOS_ADD_TEST(count_counts_each_byte_above_u10ffff),
    NAOS_ADD_TEST(count_counts_each_byte_of_a_truncated_sequence),
    NAOS_ADD_TEST(count_counts_each_lone_continuation_byte),
    NAOS_ADD_TEST(count_of_a_mix_of_invalid_bytes)
)
