/*
 * Suite out: the output sink of the help module (write and measure modes,
 * no call after a failure) and the UTF-8 display width.
 *
 * It also checks the recording callback of the shared fixtures, which the
 * byte comparisons of every suite rely on.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <string.h>

NAOS_TEST(recorder_concatenates_bytes_and_counts_calls)
{
    help_recorder_t rec;

    help_recorder_init(&rec);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_FALSE(rec.overflow);
    NAOS_CHECK_STR_EQ(rec.buf, "");

    NAOS_CHECK_INT_EQ(help_recorder_write(&rec, "ab", 2), 0);
    NAOS_CHECK_INT_EQ(help_recorder_write(&rec, "xyz", 0), 0);
    NAOS_CHECK_INT_EQ(help_recorder_write(&rec, "c\0d", 3), 0);

    NAOS_CHECK_UINT_EQ(rec.calls, 3);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "abc\0d", (size_t)5);
    NAOS_CHECK_INT_EQ(rec.buf[rec.len], '\0');
    NAOS_CHECK_FALSE(rec.overflow);
}

NAOS_TEST(recorder_rejects_a_write_that_does_not_fit)
{
    static char big[HELP_RECORDER_SIZE];
    help_recorder_t rec;

    memset(big, 'x', sizeof(big));
    help_recorder_init(&rec);

    NAOS_CHECK_INT_EQ(help_recorder_write(&rec, big, sizeof(big) - 1), 0);
    NAOS_CHECK_INT_EQ(help_recorder_write(&rec, "yz", 2), 1);
    NAOS_CHECK_TRUE(rec.overflow);
    NAOS_CHECK_UINT_EQ(rec.len, sizeof(big) - 1);
    NAOS_CHECK_INT_EQ(help_recorder_write(&rec, "y", 1), 0);
    NAOS_CHECK_UINT_EQ(rec.len, sizeof(big));
    NAOS_CHECK_UINT_EQ(rec.calls, 3);
    NAOS_CHECK_INT_EQ(rec.buf[rec.len - 1], 'y');
    NAOS_CHECK_INT_EQ(rec.buf[rec.len], '\0');

    /* init empties it again */
    help_recorder_init(&rec);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_FALSE(rec.overflow);
}

NAOS_TEST(recorder_works_as_a_write_fn)
{
    clopts_write_fn fn = help_recorder_write;
    help_recorder_t rec;

    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(fn(&rec, "hello\n", 6), 0);
    NAOS_CHECK_UINT_EQ(rec.calls, 1);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "hello\n", (size_t)6);
}

NAOS_SUITE(out,
    NAOS_ADD_TEST(recorder_concatenates_bytes_and_counts_calls),
    NAOS_ADD_TEST(recorder_rejects_a_write_that_does_not_fit),
    NAOS_ADD_TEST(recorder_works_as_a_write_fn)
)
