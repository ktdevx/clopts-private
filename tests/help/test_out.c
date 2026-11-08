/*
 * Suite out: the output sink of the help module (write and measure modes,
 * no call after a failure) and the UTF-8 display width.
 *
 * It also checks the recording callback of the shared fixtures, which the
 * byte comparisons of every suite rely on.
 */

#include "test_fixtures.h"

#include "help/help_internal.h"

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

/* ---- clopts_help_width -------------------------------------------------- */

/* Width of a string literal, embedded NULs included. */
#define WIDTH_OF(lit) clopts_help_width((lit), sizeof(lit) - 1)

NAOS_TEST(width_counts_ascii_bytes)
{
    NAOS_CHECK_UINT_EQ(clopts_help_width("", 0), 0);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("a"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("--verbose"), 9);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("a\0b"), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\x7F"), 1);
    /* only len bytes are looked at */
    NAOS_CHECK_UINT_EQ(clopts_help_width("abcdef", 2), 2);
}

NAOS_TEST(width_counts_a_valid_sequence_as_one)
{
    /* two bytes: U+0080, U+00EF, U+07FF */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xC2\x80"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xC3\xAF"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xDF\xBF"), 1);
    /* three bytes: U+0800, U+65E5, U+D7FF, U+E000, U+FFFF */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE0\xA0\x80"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE6\x97\xA5"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xED\x9F\xBF"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xEE\x80\x80"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xEF\xBF\xBF"), 1);
    /* four bytes: U+10000, U+1F600, U+10FFFF */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF0\x90\x80\x80"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF0\x9F\x98\x80"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF4\x8F\xBF\xBF"), 1);
    /* mixed with ASCII */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("a\xC3\xAF\xE6\x97\xA5\xF0\x9F\x98\x80" "z"), 5);
    NAOS_CHECK_UINT_EQ(WIDTH_OF(HELP_FIXTURE_NAIVE), 5);
    NAOS_CHECK_UINT_EQ(WIDTH_OF(HELP_FIXTURE_NIHONGO), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF(HELP_FIXTURE_ATAI), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF(HELP_FIXTURE_FILE), 4);
}

NAOS_TEST(width_counts_each_byte_of_an_overlong_form)
{
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xC0\x80"), 2);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xC0\xAF"), 2);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xC1\xBF"), 2);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE0\x80\xAF"), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE0\x9F\xBF"), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF0\x80\x80\xAF"), 4);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF0\x8F\xBF\xBF"), 4);
}

NAOS_TEST(width_counts_each_byte_of_a_surrogate)
{
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xED\xA0\x80"), 3); /* U+D800 */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xED\xAF\xBF"), 3); /* U+DBFF */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xED\xB0\x80"), 3); /* U+DC00 */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xED\xBF\xBF"), 3); /* U+DFFF */
}

NAOS_TEST(width_counts_each_byte_above_u10ffff)
{
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF4\x90\x80\x80"), 4); /* U+110000 */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF5\x80\x80\x80"), 4);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF7\xBF\xBF\xBF"), 4);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF8\x88\x80\x80\x80"), 5);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xFE"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xFF"), 1);
}

NAOS_TEST(width_counts_each_byte_of_a_truncated_sequence)
{
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xC3"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE6\x97"), 2);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF0\x9F\x98"), 3);
    /* cut by len, not by a NUL */
    NAOS_CHECK_UINT_EQ(clopts_help_width("\xE6\x97\xA5", 2), 2);
    /* interrupted by an ASCII byte or by the lead byte of a valid sequence */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE6\x97" "E"), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF0\x9F" "a"), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE6\xC3\xAF"), 2);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xF0\x9F\x98\xE6\x97\xA5"), 4);
}

NAOS_TEST(width_counts_each_lone_continuation_byte)
{
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\x80"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xBF"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\x80\x80\x80"), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("a\x80" "b"), 3);
    /* one too many after a valid sequence */
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xC3\xAF\xAF"), 2);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\xE6\x97\xA5\x97\xA5"), 3);
}

NAOS_TEST(width_of_the_fixture_raw_value_name_is_seven)
{
    NAOS_CHECK_UINT_EQ(WIDTH_OF(HELP_FIXTURE_RAW_VALUE_NAME), 7);
}

/* ---- write mode --------------------------------------------------------- */

#define RAW_LEN (sizeof(HELP_FIXTURE_RAW_VALUE_NAME) - 1)
#define WRITTEN_IN_ORDER "ab\0" HELP_FIXTURE_NIHONGO "=   " HELP_FIXTURE_RAW_VALUE_NAME

NAOS_TEST(write_mode_passes_every_output_to_the_callback_in_order)
{
    help_recorder_t rec;
    clopts_help_out_t out;

    help_recorder_init(&rec);
    clopts_help_out_init(&out, help_recorder_write, &rec);

    clopts_help_out_bytes(&out, "ab\0", 3);
    clopts_help_out_str(&out, HELP_FIXTURE_NIHONGO);
    clopts_help_out_char(&out, '=');
    clopts_help_out_spaces(&out, 3);
    clopts_help_out_bytes(&out, HELP_FIXTURE_RAW_VALUE_NAME, RAW_LEN);

    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(rec.calls, 5);
    NAOS_CHECK_FALSE(rec.overflow);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, WRITTEN_IN_ORDER, sizeof(WRITTEN_IN_ORDER) - 1);
}

NAOS_TEST(write_mode_does_not_call_the_callback_for_zero_bytes)
{
    help_recorder_t rec;
    clopts_help_out_t out;

    help_recorder_init(&rec);
    clopts_help_out_init(&out, help_recorder_write, &rec);

    clopts_help_out_bytes(&out, "abc", 0);
    clopts_help_out_bytes(&out, NULL, 0);
    clopts_help_out_str(&out, "");
    clopts_help_out_spaces(&out, 0);

    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
}

NAOS_TEST(write_mode_writes_many_spaces)
{
    static char expected[200];
    help_recorder_t rec;
    clopts_help_out_t out;

    memset(expected, ' ', sizeof(expected));
    help_recorder_init(&rec);
    clopts_help_out_init(&out, help_recorder_write, &rec);

    clopts_help_out_spaces(&out, sizeof(expected));

    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, sizeof(expected));
}

NAOS_TEST(write_mode_stops_calling_after_the_first_failure)
{
    help_failer_t failer;
    clopts_help_out_t out;

    help_failer_init(&failer, 3);
    clopts_help_out_init(&out, help_failer_write, &failer);

    clopts_help_out_str(&out, "Usage:");
    clopts_help_out_char(&out, ' ');
    NAOS_CHECK_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);

    clopts_help_out_str(&out, "tool"); /* the 3rd call fails */
    clopts_help_out_spaces(&out, 2);
    clopts_help_out_bytes(&out, "[OPTIONS]", 9);
    clopts_help_out_char(&out, '\n');
    clopts_help_out_str(&out, "more");

    NAOS_CHECK_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_FAILED);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 3);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "Usage: ", (size_t)7);
}

NAOS_TEST(write_mode_keeps_a_failure_on_the_first_call)
{
    help_failer_t failer;
    clopts_help_out_t out;

    help_failer_init(&failer, 1);
    clopts_help_out_init(&out, help_failer_write, &failer);

    clopts_help_out_str(&out, "a");
    clopts_help_out_str(&out, "b");
    clopts_help_out_spaces(&out, 100);

    NAOS_CHECK_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_FAILED);
    NAOS_CHECK_UINT_EQ(failer.calls, 1);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_UINT_EQ(failer.written.len, 0);
}

NAOS_TEST(write_mode_with_a_callback_that_never_fails_is_ok)
{
    help_failer_t failer;
    clopts_help_out_t out;

    help_failer_init(&failer, 0);
    clopts_help_out_init(&out, help_failer_write, &failer);

    clopts_help_out_str(&out, "a");
    clopts_help_out_str(&out, "b");
    clopts_help_out_char(&out, 'c');

    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(failer.calls, 3);
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "abc", (size_t)3);
}

/* ---- measure mode ------------------------------------------------------- */

NAOS_TEST(measure_mode_adds_widths_without_writing)
{
    clopts_help_out_t out;

    clopts_help_out_init(&out, NULL, NULL);
    NAOS_CHECK_UINT_EQ(out.width, 0);

    clopts_help_out_str(&out, "--");                                /* 2 */
    clopts_help_out_str(&out, HELP_FIXTURE_NIHONGO);                /* 3 */
    clopts_help_out_char(&out, ' ');                                /* 1 */
    clopts_help_out_spaces(&out, 40);                               /* 40 */
    clopts_help_out_bytes(&out, HELP_FIXTURE_RAW_VALUE_NAME, RAW_LEN); /* 7 */
    clopts_help_out_bytes(&out, HELP_FIXTURE_NAIVE, 3);             /* "na" and a lone C3: 3 */
    clopts_help_out_str(&out, "");                                  /* 0 */
    clopts_help_out_spaces(&out, 0);                                /* 0 */

    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(out.width, 56);
}

NAOS_TEST(measure_mode_does_not_call_the_callback)
{
    help_recorder_t rec;
    clopts_help_out_t out;

    help_recorder_init(&rec);
    /* ctx is given, but without fn nothing is written to it */
    clopts_help_out_init(&out, NULL, &rec);

    clopts_help_out_str(&out, "abc");
    clopts_help_out_char(&out, 'd');
    clopts_help_out_spaces(&out, 5);

    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(out.width, 9);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
}

NAOS_TEST(init_starts_over)
{
    help_failer_t failer;
    help_recorder_t rec;
    clopts_help_out_t out;

    help_failer_init(&failer, 1);
    clopts_help_out_init(&out, help_failer_write, &failer);
    clopts_help_out_str(&out, "x");
    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_FAILED);

    clopts_help_out_init(&out, NULL, NULL);
    clopts_help_out_str(&out, "xy");
    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(out.width, 2);

    help_recorder_init(&rec);
    clopts_help_out_init(&out, help_recorder_write, &rec);
    NAOS_CHECK_UINT_EQ(out.width, 0);
    clopts_help_out_str(&out, "z");
    NAOS_REQUIRE_INT_EQ(clopts_help_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "z", (size_t)1);
}

NAOS_SUITE(out,
    NAOS_ADD_TEST(recorder_concatenates_bytes_and_counts_calls),
    NAOS_ADD_TEST(recorder_rejects_a_write_that_does_not_fit),
    NAOS_ADD_TEST(recorder_works_as_a_write_fn),
    NAOS_ADD_TEST(width_counts_ascii_bytes),
    NAOS_ADD_TEST(width_counts_a_valid_sequence_as_one),
    NAOS_ADD_TEST(width_counts_each_byte_of_an_overlong_form),
    NAOS_ADD_TEST(width_counts_each_byte_of_a_surrogate),
    NAOS_ADD_TEST(width_counts_each_byte_above_u10ffff),
    NAOS_ADD_TEST(width_counts_each_byte_of_a_truncated_sequence),
    NAOS_ADD_TEST(width_counts_each_lone_continuation_byte),
    NAOS_ADD_TEST(width_of_the_fixture_raw_value_name_is_seven),
    NAOS_ADD_TEST(write_mode_passes_every_output_to_the_callback_in_order),
    NAOS_ADD_TEST(write_mode_does_not_call_the_callback_for_zero_bytes),
    NAOS_ADD_TEST(write_mode_writes_many_spaces),
    NAOS_ADD_TEST(write_mode_stops_calling_after_the_first_failure),
    NAOS_ADD_TEST(write_mode_keeps_a_failure_on_the_first_call),
    NAOS_ADD_TEST(write_mode_with_a_callback_that_never_fails_is_ok),
    NAOS_ADD_TEST(measure_mode_adds_widths_without_writing),
    NAOS_ADD_TEST(measure_mode_does_not_call_the_callback),
    NAOS_ADD_TEST(init_starts_over)
)
