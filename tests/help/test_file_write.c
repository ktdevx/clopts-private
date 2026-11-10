/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the ready-made FILE* callback of clopts/writer.h (the static
 * inline clopts_file_write), through which the help module's output usually
 * reaches a stream.
 */

#include "clopts/writer.h"

#include <naos/naos.h>

#include <stdio.h>

/*
 * Creates the temporary file used by the FILE* callback tests. Kept in this
 * single function so that the creation method can be replaced (for example
 * by fopen() under the build directory) where tmpfile() does not work.
 */
static FILE *open_temp_file(void)
{
    return tmpfile();
}

/* Reads the whole stream from the start into buf; returns the byte count. */
static size_t read_back(FILE *stream, char *buf, size_t size)
{
    rewind(stream);
    return fread(buf, 1, size, stream);
}

/*
 * File used by the read-only stream test. It is created in the working
 * directory of the test run and removed by the test.
 */
static const char read_only_path[] = "clopts_help_tests_file_write_read_only.tmp";

NAOS_TEST(file_write_writes_all_bytes_in_order)
{
    static const char expected[] = { 'a', 'b', 'c', '\0', 'x', '\xFF', '\n', 'e', 'n', 'd' };
    clopts_write_fn fn = clopts_file_write;
    FILE *stream = open_temp_file();
    char buf[32];
    size_t n;

    NAOS_REQUIRE_NOT_NULL(stream);

    NAOS_CHECK_INT_EQ(clopts_file_write(stream, expected, 3), 0);
    NAOS_CHECK_INT_EQ(fn(stream, expected + 3, 4), 0);
    NAOS_CHECK_INT_EQ(fn(stream, expected + 7, 3), 0);

    n = read_back(stream, buf, sizeof(buf));
    NAOS_CHECK_MEM_EQ(buf, n, expected, sizeof(expected));

    fclose(stream);
}

NAOS_TEST(file_write_len_zero_writes_nothing)
{
    FILE *stream = open_temp_file();
    char buf[8];

    NAOS_REQUIRE_NOT_NULL(stream);

    NAOS_CHECK_INT_EQ(clopts_file_write(stream, "abc", 0), 0);
    NAOS_CHECK_INT_EQ(clopts_file_write(stream, NULL, 0), 0);

    NAOS_CHECK_UINT_EQ(read_back(stream, buf, sizeof(buf)), 0);

    fclose(stream);
}

NAOS_TEST(file_write_to_read_only_stream_fails)
{
    FILE *stream = fopen(read_only_path, "wb");

    NAOS_REQUIRE_NOT_NULL(stream);
    NAOS_REQUIRE_INT_EQ(fclose(stream), 0);

    stream = fopen(read_only_path, "rb");
    NAOS_REQUIRE_NOT_NULL(stream);

    NAOS_CHECK_INT_NE(clopts_file_write(stream, "abc", 3), 0);

    fclose(stream);
    NAOS_CHECK_INT_EQ(remove(read_only_path), 0);
}

NAOS_SUITE(file_write,
    NAOS_ADD_TEST(file_write_writes_all_bytes_in_order),
    NAOS_ADD_TEST(file_write_len_zero_writes_nothing),
    NAOS_ADD_TEST(file_write_to_read_only_stream_fails)
)
