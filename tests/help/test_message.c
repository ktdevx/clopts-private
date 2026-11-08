/*
 * Suite message: the texts of every usage and declaration error kind, the
 * prefixes and the coverage of the text tables.
 *
 * It also checks the failing callback of the shared fixtures, which the
 * write failure tests rely on.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(failer_fails_on_the_nth_call_and_counts_later_calls)
{
    help_failer_t failer;

    help_failer_init(&failer, 3);
    NAOS_CHECK_UINT_EQ(failer.fail_at, 3);
    NAOS_CHECK_UINT_EQ(failer.calls, 0);
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "ab", 2), 0);
    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "c", 1), 0);
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "d", 1), 1);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 3);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);

    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "e", 1), 1);
    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "f", 0), 1);
    NAOS_CHECK_UINT_EQ(failer.calls, 5);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 2);

    /* only the bytes before the failing call were accepted */
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "abc", (size_t)3);
}

NAOS_TEST(failer_fails_on_the_first_call)
{
    help_failer_t failer;

    help_failer_init(&failer, 1);
    NAOS_CHECK_INT_EQ(help_failer_write(&failer, "a", 1), 1);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 1);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_UINT_EQ(failer.written.len, 0);
}

NAOS_TEST(failer_with_zero_never_fails)
{
    clopts_write_fn fn = help_failer_write;
    help_failer_t failer;
    int i;

    help_failer_init(&failer, 0);
    for (i = 0; i < 10; i++)
    {
        NAOS_CHECK_INT_EQ(fn(&failer, "x", 1), 0);
    }
    NAOS_CHECK_FALSE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 10);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "xxxxxxxxxx", (size_t)10);
}

NAOS_SUITE(message,
    NAOS_ADD_TEST(failer_fails_on_the_nth_call_and_counts_later_calls),
    NAOS_ADD_TEST(failer_fails_on_the_first_call),
    NAOS_ADD_TEST(failer_with_zero_never_fails)
)
