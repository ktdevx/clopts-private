/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite split: command line splitting with the rules of the current MSVC
 * CRT (clopts_process_split), on every OS.
 *
 * It also checks the comparison helper of test_util, which the table of
 * this suite relies on.
 */

#include "test_util.h"

#include "process/process_internal.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

NAOS_TEST(split_mismatch_accepts_equal_lists)
{
    process_test_u16_t a;
    process_test_u16_t b;
    process_test_u16_t empty;
    clopts_process_u16_t *args[3];
    static const char *const expected[] = {"prog", "b c", "", NULL};
    static const char *const none[] = {NULL};

    args[0] = process_test_u16_from_ascii(&a, "prog");
    args[1] = process_test_u16_from_ascii(&b, "b c");
    args[2] = process_test_u16_from_ascii(&empty, "");

    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 3, expected), -1);
    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 0, none), -1);
}

NAOS_TEST(split_mismatch_reports_the_first_difference)
{
    process_test_u16_t a;
    process_test_u16_t b;
    clopts_process_u16_t *args[2];
    static const char *const second_differs[] = {"prog", "bx", NULL};
    static const char *const both_differ[] = {"pro", "c", NULL};
    static const char *const longer[] = {"prog", "b", "c", NULL};
    static const char *const shorter[] = {"prog", NULL};

    args[0] = process_test_u16_from_ascii(&a, "prog");
    args[1] = process_test_u16_from_ascii(&b, "b");

    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 2, second_differs), 1);
    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 2, both_differ), 0);
    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 2, longer), 2);
    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 2, shorter), 1);
}

NAOS_TEST(split_mismatch_compares_non_ascii_units_as_different)
{
    static const uint16_t units[] = {0x30C6, 0x0061};
    process_test_u16_t a;
    clopts_process_u16_t *args[1];
    static const char *const expected[] = {"\x30" "a", NULL};

    args[0] = process_test_u16_from_units(&a, units, 2);
    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 1, expected), 0);
}

/* Room for the results of every row; checked with NAOS_REQUIRE before the fill pass. */
#define SPLIT_TEST_ARGS 16
#define SPLIT_TEST_UNITS 128
#define SPLIT_TEST_CANARY 0xBEEFu

/*
 * Runs both passes of clopts_process_split on cmdline and checks the
 * contract that does not depend on the expected elements: argc >= 1, the
 * fill pass reports the same argc and units as the counting pass, the
 * elements are laid out one after the other in buf with one NUL each and
 * use exactly *units code units, and nothing past them is written.
 * Returns the element count; args[0..argc-1] point into buf.
 */
static size_t split_run(const clopts_process_u16_t *cmdline, clopts_process_u16_t *buf,
                        clopts_process_u16_t **args)
{
    size_t count_argc = 0;
    size_t count_units = 0;
    size_t fill_argc = 0;
    size_t fill_units = 0;
    size_t used = 0;
    size_t i;

    clopts_process_split(cmdline, NULL, NULL, &count_argc, &count_units);
    NAOS_REQUIRE_UINT_GE(count_argc, 1);
    NAOS_REQUIRE_UINT_LE(count_argc, SPLIT_TEST_ARGS);
    NAOS_REQUIRE_UINT_GE(count_units, count_argc);
    NAOS_REQUIRE_UINT_LE(count_units, SPLIT_TEST_UNITS);

    for (i = 0; i <= SPLIT_TEST_UNITS; i++)
    {
        buf[i] = (clopts_process_u16_t)SPLIT_TEST_CANARY;
    }
    for (i = 0; i <= SPLIT_TEST_ARGS; i++)
    {
        args[i] = NULL;
    }

    clopts_process_split(cmdline, buf, args, &fill_argc, &fill_units);
    NAOS_REQUIRE_UINT_EQ(fill_argc, count_argc);
    NAOS_REQUIRE_UINT_EQ(fill_units, count_units);

    for (i = 0; i < fill_argc; i++)
    {
        size_t len = 0;

        NAOS_REQUIRE_PTR_EQ(args[i], buf + used);
        while (used + len < fill_units && args[i][len] != 0)
        {
            len++;
        }
        NAOS_REQUIRE_UINT_LT(used + len, fill_units);
        used += len + 1;
    }
    NAOS_CHECK_UINT_EQ(used, fill_units);
    NAOS_CHECK_UINT_EQ(buf[fill_units], SPLIT_TEST_CANARY);
    NAOS_CHECK_NULL(args[fill_argc]);
    return fill_argc;
}

/* One row of the table: an ASCII command line and the elements it splits into. */
typedef struct split_row
{
    const char *cmdline;
    const char *expected[SPLIT_TEST_ARGS + 1];   /* NULL-terminated */
} split_row_t;

/*
 * The raw command line of each row is given in the comment before it, as
 * the CRT sees it (no C escaping); the expected elements follow the rule
 * IDs R0-R9 of the design.
 */
static const split_row_t split_rows[] = {
    /* (empty): argc 1, argv[0] empty (R1) */
    { "", { "" } },
    /* " " : leading space ends argv[0] empty, the trailing spaces make nothing (R1, R2) */
    { " ", { "" } },
    /* " a" : leading space, argv[0] empty (R1) */
    { " a", { "", "a" } },
    /* "\tp a" : a leading tab works like a space (R1) */
    { "\tp a", { "", "p", "a" } },
    /* p a b */
    { "p a b", { "p", "a", "b" } },
    /* p  a   b : runs of spaces separate once (R2, R7) */
    { "p  a   b", { "p", "a", "b" } },
    /* p a<sp><sp> : trailing spaces make no element (R2) */
    { "p a  ", { "p", "a" } },
    /* p<sp><sp><sp> */
    { "p   ", { "p" } },
    /* p<tab>a<tab>b<sp><tab>c : tabs separate (R2, R7) */
    { "p\ta\tb \tc", { "p", "a", "b", "c" } },
    /* p a<LF>b and p<LF>q r : a newline is an ordinary character (R8) */
    { "p a\nb", { "p", "a\nb" } },
    { "p\nq r", { "p\nq", "r" } },
    /* p a<CR>b<VT>c<FF>d : no other control character separates (R8) */
    { "p a\rb\vc\fd", { "p", "a\rb\vc\fd" } },
    /* p * ? *.c a?b : wildcards are kept (R8) */
    { "p * ? *.c a?b", { "p", "*", "?", "*.c", "a?b" } },

    /* argv[0] (R0) */
    /* "C:\Program Files\x"y z */
    { "\"C:\\Program Files\\x\"y z", { "C:\\Program Files\\xy", "z" } },
    /* a\"b c : the backslash stays, the quote toggles quote mode -> a\b c */
    { "a\\\"b c", { "a\\b c" } },
    /* a\"b c" d : quote mode closes again -> a\b c, d */
    { "a\\\"b c\" d", { "a\\b c", "d" } },
    /* "a b" c */
    { "\"a b\" c", { "a b", "c" } },
    /* a"b"c d : quotes vanish in the middle of argv[0] */
    { "a\"b\"c d", { "abc", "d" } },
    /* "" a : argv[0] made of quotes only is empty */
    { "\"\" a", { "", "a" } },
    /* """" a : no doubled-quote rule in argv[0]: four toggles -> empty */
    { "\"\"\"\" a", { "", "a" } },
    /* "a b : unterminated quote in argv[0] runs to the end */
    { "\"a b", { "a b" } },
    /* C:\dir\ a : trailing backslash of argv[0] stays */
    { "C:\\dir\\ a", { "C:\\dir\\", "a" } },
    /* p\\"x y : backslashes never escape in argv[0] -> p\\x y */
    { "p\\\\\"x y", { "p\\\\x y" } },

    /* backslashes and quotes after argv[0] (R3-R6) */
    /* p "foo""bar" -> foo"bar (R5) */
    { "p \"foo\"\"bar\"", { "p", "foo\"bar" } },
    /* p a\"b (1 backslash) -> a"b (R4) */
    { "p a\\\"b", { "p", "a\"b" } },
    /* p a\\\"b (3 backslashes) -> a\"b (R4) */
    { "p a\\\\\\\"b", { "p", "a\\\"b" } },
    /* p a\\"b c" (2 backslashes) -> a\b c (R3) */
    { "p a\\\\\"b c\"", { "p", "a\\b c" } },
    /* p a\\\\"b c" (4 backslashes) -> a\\b c (R3) */
    { "p a\\\\\\\\\"b c\"", { "p", "a\\\\b c" } },
    /* p a\ : trailing backslash stays (R6) */
    { "p a\\", { "p", "a\\" } },
    /* p a\\ b : backslashes not followed by a quote stay (R6) */
    { "p a\\\\ b", { "p", "a\\\\", "b" } },
    /* p a\b\\c */
    { "p a\\b\\\\c", { "p", "a\\b\\\\c" } },
    /* p "a\\" (2 backslashes) -> a\ (R3) */
    { "p \"a\\\\\"", { "p", "a\\" } },
    /* p "a\\" b : quote mode ends at the quote after the even run */
    { "p \"a\\\\\" b", { "p", "a\\", "b" } },
    /* p "a\"b" : odd run inside quotes emits a quote, mode kept (R4) */
    { "p \"a\\\"b\"", { "p", "a\"b" } },
    /* p \" -> " */
    { "p \\\"", { "p", "\"" } },
    /* p \\\\" (4 backslashes, quote opens an unterminated quote) -> \\ (R3, R9) */
    { "p \\\\\\\\\"", { "p", "\\\\" } },
    /* p "a\ b" : backslash without a quote inside quotes stays (R6) */
    { "p \"a\\ b\"", { "p", "a\\ b" } },

    /* quote mode (R5, R7, R9) */
    /* p "" -> empty element (R7) */
    { "p \"\"", { "p", "" } },
    /* p "" "" x */
    { "p \"\" \"\" x", { "p", "", "", "x" } },
    /* p a"b c"d -> ab cd */
    { "p a\"b c\"d", { "p", "ab cd" } },
    /* p "a" "b" : quote mode does not carry over elements */
    { "p \"a\" \"b\"", { "p", "a", "b" } },
    /* p "a b : unterminated quote (R9) */
    { "p \"a b", { "p", "a b" } },
    /* p "a b<sp><sp> : unterminated quote keeps the trailing spaces (R9) */
    { "p \"a b  ", { "p", "a b  " } },
    /* p " : a lone quote makes an empty element */
    { "p \"", { "p", "" } },
    /* p """ -> " (doubled quote inside quotes, then the end) */
    { "p \"\"\"", { "p", "\"" } },
    /* p """" -> " */
    { "p \"\"\"\"", { "p", "\"" } },
    /* p """ a -> " a (quote mode kept after the doubled quote) */
    { "p \"\"\" a", { "p", "\" a" } },
    /* p "a"" b" -> a" b */
    { "p \"a\"\" b\"", { "p", "a\" b" } },
    /* p a""b : outside quotes "" opens and closes -> ab */
    { "p a\"\"b", { "p", "ab" } },
    /* p a"""b : open, doubled quote -> a"b, unterminated */
    { "p a\"\"\"b", { "p", "a\"b" } },
    /* p "a<tab>b" c : tab inside quotes is kept */
    { "p \"a\tb\" c", { "p", "a\tb", "c" } },
};

NAOS_TEST(split_table)
{
    size_t row;

    for (row = 0; row < sizeof split_rows / sizeof split_rows[0]; row++)
    {
        process_test_u16_t cmdline;
        clopts_process_u16_t buf[SPLIT_TEST_UNITS + 1];
        clopts_process_u16_t *args[SPLIT_TEST_ARGS + 1];
        size_t argc;

        NAOS_TRACE_PUSH("row %u: cmdline \"%s\"", (unsigned)row, split_rows[row].cmdline);
        argc = split_run(process_test_u16_from_ascii(&cmdline, split_rows[row].cmdline), buf, args);
        NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, argc, split_rows[row].expected), -1);
        NAOS_TRACE_POP();
    }
}

/* True when the NUL-terminated s has exactly units[0..count-1]. */
static bool split_units_eq(const clopts_process_u16_t *s, const uint16_t *units, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if ((uint16_t)s[i] != units[i])
        {
            return false;
        }
    }
    return s[count] == 0;
}

NAOS_TEST(split_passes_non_ascii_and_surrogates_through)
{
    /* <U+30C6> <U+1F600 as a pair> "<lone low> <lone high>" <U+00A0>a<U+3000>b */
    static const uint16_t cmdline_units[] = {
        0x30C6, 0x0020, 0xD83D, 0xDE00, 0x0020, 0x0022, 0xDC00, 0x0020, 0xD800, 0x0022, 0x0020,
        0x00A0, 0x0061, 0x3000, 0x0062,
    };
    static const uint16_t arg0[] = {0x30C6};
    static const uint16_t arg1[] = {0xD83D, 0xDE00};
    static const uint16_t arg2[] = {0xDC00, 0x0020, 0xD800};
    static const uint16_t arg3[] = {0x00A0, 0x0061, 0x3000, 0x0062};
    process_test_u16_t cmdline;
    clopts_process_u16_t buf[SPLIT_TEST_UNITS + 1];
    clopts_process_u16_t *args[SPLIT_TEST_ARGS + 1];
    size_t argc;

    argc = split_run(process_test_u16_from_units(&cmdline, cmdline_units,
                                                  sizeof cmdline_units / sizeof cmdline_units[0]),
                     buf, args);
    NAOS_REQUIRE_UINT_EQ(argc, 4);
    NAOS_CHECK_TRUE(split_units_eq(args[0], arg0, 1));
    NAOS_CHECK_TRUE(split_units_eq(args[1], arg1, 2));
    NAOS_CHECK_TRUE(split_units_eq(args[2], arg2, 3));
    NAOS_CHECK_TRUE(split_units_eq(args[3], arg3, 4));
}

NAOS_TEST(split_non_ascii_in_argv0_with_quotes)
{
    /* "<U+30C6> <lone high>"\x y : argv[0] rules with non-ASCII units */
    static const uint16_t cmdline_units[] = {
        0x0022, 0x30C6, 0x0020, 0xD800, 0x0022, 0x005C, 0x0078, 0x0020, 0x0079,
    };
    static const uint16_t arg0[] = {0x30C6, 0x0020, 0xD800, 0x005C, 0x0078};
    static const uint16_t arg1[] = {0x0079};
    process_test_u16_t cmdline;
    clopts_process_u16_t buf[SPLIT_TEST_UNITS + 1];
    clopts_process_u16_t *args[SPLIT_TEST_ARGS + 1];
    size_t argc;

    argc = split_run(process_test_u16_from_units(&cmdline, cmdline_units,
                                                  sizeof cmdline_units / sizeof cmdline_units[0]),
                     buf, args);
    NAOS_REQUIRE_UINT_EQ(argc, 2);
    NAOS_CHECK_TRUE(split_units_eq(args[0], arg0, 5));
    NAOS_CHECK_TRUE(split_units_eq(args[1], arg1, 1));
}

NAOS_SUITE(split,
    NAOS_ADD_TEST(split_mismatch_accepts_equal_lists),
    NAOS_ADD_TEST(split_mismatch_reports_the_first_difference),
    NAOS_ADD_TEST(split_mismatch_compares_non_ascii_units_as_different),
    NAOS_ADD_TEST(split_table),
    NAOS_ADD_TEST(split_passes_non_ascii_and_surrogates_through),
    NAOS_ADD_TEST(split_non_ascii_in_argv0_with_quotes)
)
