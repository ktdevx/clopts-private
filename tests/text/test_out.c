/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite out: the output sink of the text module (write and measure modes,
 * no call after a failure) and the UTF-8 display width.
 *
 * It also checks the recording callback of the shared fixtures, which the
 * byte comparisons of every suite rely on.
 */

#include "test_fixtures.h"

#include "text/text_internal.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

NAOS_TEST(recorder_concatenates_bytes_and_counts_calls)
{
    text_recorder_t rec;

    text_recorder_init(&rec);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_FALSE(rec.overflow);
    NAOS_CHECK_STR_EQ(rec.buf, "");

    NAOS_CHECK_INT_EQ(text_recorder_write(&rec, "ab", 2), 0);
    NAOS_CHECK_INT_EQ(text_recorder_write(&rec, "xyz", 0), 0);
    NAOS_CHECK_INT_EQ(text_recorder_write(&rec, "c\0d", 3), 0);

    NAOS_CHECK_UINT_EQ(rec.calls, 3);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "abc\0d", (size_t)5);
    NAOS_CHECK_INT_EQ(rec.buf[rec.len], '\0');
    NAOS_CHECK_FALSE(rec.overflow);
}

NAOS_TEST(recorder_rejects_a_write_that_does_not_fit)
{
    static char big[TEXT_RECORDER_SIZE];
    text_recorder_t rec;

    memset(big, 'x', sizeof(big));
    text_recorder_init(&rec);

    NAOS_CHECK_INT_EQ(text_recorder_write(&rec, big, sizeof(big) - 1), 0);
    NAOS_CHECK_INT_EQ(text_recorder_write(&rec, "yz", 2), 1);
    NAOS_CHECK_TRUE(rec.overflow);
    NAOS_CHECK_UINT_EQ(rec.len, sizeof(big) - 1);
    NAOS_CHECK_INT_EQ(text_recorder_write(&rec, "y", 1), 0);
    NAOS_CHECK_UINT_EQ(rec.len, sizeof(big));
    NAOS_CHECK_UINT_EQ(rec.calls, 3);
    NAOS_CHECK_INT_EQ(rec.buf[rec.len - 1], 'y');
    NAOS_CHECK_INT_EQ(rec.buf[rec.len], '\0');

    /* init empties it again */
    text_recorder_init(&rec);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_FALSE(rec.overflow);
}

NAOS_TEST(recorder_works_as_a_write_fn)
{
    clopts_write_fn fn = text_recorder_write;
    text_recorder_t rec;

    text_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(fn(&rec, "hello\n", 6), 0);
    NAOS_CHECK_UINT_EQ(rec.calls, 1);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "hello\n", (size_t)6);
}

/* ---- clopts_text_width -------------------------------------------------- */

/* Width of a string literal, embedded NULs included. */
#define WIDTH_OF(lit) clopts_text_width((lit), sizeof(lit) - 1)

NAOS_TEST(width_counts_ascii_bytes)
{
    NAOS_CHECK_UINT_EQ(clopts_text_width("", 0), 0);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("a"), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("--verbose"), 9);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("a\0b"), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF("\x7F"), 1);
    /* only len bytes are looked at */
    NAOS_CHECK_UINT_EQ(clopts_text_width("abcdef", 2), 2);
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
    NAOS_CHECK_UINT_EQ(WIDTH_OF(TEXT_FIXTURE_NAIVE), 5);
    NAOS_CHECK_UINT_EQ(WIDTH_OF(TEXT_FIXTURE_NIHONGO), 3);
    NAOS_CHECK_UINT_EQ(WIDTH_OF(TEXT_FIXTURE_ATAI), 1);
    NAOS_CHECK_UINT_EQ(WIDTH_OF(TEXT_FIXTURE_FILE), 4);
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
    NAOS_CHECK_UINT_EQ(clopts_text_width("\xE6\x97\xA5", 2), 2);
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
    NAOS_CHECK_UINT_EQ(WIDTH_OF(TEXT_FIXTURE_RAW_VALUE_NAME), 7);
}

/* ---- write mode --------------------------------------------------------- */

#define RAW_LEN (sizeof(TEXT_FIXTURE_RAW_VALUE_NAME) - 1)
#define WRITTEN_IN_ORDER "ab\0" TEXT_FIXTURE_NIHONGO "=   " TEXT_FIXTURE_RAW_VALUE_NAME

NAOS_TEST(write_mode_passes_every_output_to_the_callback_in_order)
{
    text_recorder_t rec;
    clopts_text_out_t out;

    text_recorder_init(&rec);
    clopts_text_out_init(&out, text_recorder_write, &rec);

    clopts_text_out_bytes(&out, "ab\0", 3);
    clopts_text_out_str(&out, TEXT_FIXTURE_NIHONGO);
    clopts_text_out_char(&out, '=');
    clopts_text_out_spaces(&out, 3);
    clopts_text_out_bytes(&out, TEXT_FIXTURE_RAW_VALUE_NAME, RAW_LEN);

    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(rec.calls, 5);
    NAOS_CHECK_FALSE(rec.overflow);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, WRITTEN_IN_ORDER, sizeof(WRITTEN_IN_ORDER) - 1);
}

NAOS_TEST(write_mode_does_not_call_the_callback_for_zero_bytes)
{
    text_recorder_t rec;
    clopts_text_out_t out;

    text_recorder_init(&rec);
    clopts_text_out_init(&out, text_recorder_write, &rec);

    clopts_text_out_bytes(&out, "abc", 0);
    clopts_text_out_bytes(&out, NULL, 0);
    clopts_text_out_str(&out, "");
    clopts_text_out_spaces(&out, 0);

    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
}

NAOS_TEST(write_mode_writes_many_spaces)
{
    static char expected[200];
    text_recorder_t rec;
    clopts_text_out_t out;

    memset(expected, ' ', sizeof(expected));
    text_recorder_init(&rec);
    clopts_text_out_init(&out, text_recorder_write, &rec);

    clopts_text_out_spaces(&out, sizeof(expected));

    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, sizeof(expected));
}

NAOS_TEST(write_mode_stops_calling_after_the_first_failure)
{
    text_failer_t failer;
    clopts_text_out_t out;

    text_failer_init(&failer, 3);
    clopts_text_out_init(&out, text_failer_write, &failer);

    clopts_text_out_str(&out, "Usage:");
    clopts_text_out_char(&out, ' ');
    NAOS_CHECK_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);

    clopts_text_out_str(&out, "tool"); /* the 3rd call fails */
    clopts_text_out_spaces(&out, 2);
    clopts_text_out_bytes(&out, "[OPTIONS]", 9);
    clopts_text_out_char(&out, '\n');
    clopts_text_out_str(&out, "more");

    NAOS_CHECK_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_FAILED);
    NAOS_CHECK_TRUE(failer.failed);
    NAOS_CHECK_UINT_EQ(failer.calls, 3);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "Usage: ", (size_t)7);
}

NAOS_TEST(write_mode_keeps_a_failure_on_the_first_call)
{
    text_failer_t failer;
    clopts_text_out_t out;

    text_failer_init(&failer, 1);
    clopts_text_out_init(&out, text_failer_write, &failer);

    clopts_text_out_str(&out, "a");
    clopts_text_out_str(&out, "b");
    clopts_text_out_spaces(&out, 100);

    NAOS_CHECK_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_FAILED);
    NAOS_CHECK_UINT_EQ(failer.calls, 1);
    NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
    NAOS_CHECK_UINT_EQ(failer.written.len, 0);
}

NAOS_TEST(write_mode_with_a_callback_that_never_fails_is_ok)
{
    text_failer_t failer;
    clopts_text_out_t out;

    text_failer_init(&failer, 0);
    clopts_text_out_init(&out, text_failer_write, &failer);

    clopts_text_out_str(&out, "a");
    clopts_text_out_str(&out, "b");
    clopts_text_out_char(&out, 'c');

    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(failer.calls, 3);
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, "abc", (size_t)3);
}

/* ---- measure mode ------------------------------------------------------- */

NAOS_TEST(measure_mode_adds_widths_without_writing)
{
    clopts_text_out_t out;

    clopts_text_out_init(&out, NULL, NULL);
    NAOS_CHECK_UINT_EQ(out.width, 0);

    clopts_text_out_str(&out, "--");                                /* 2 */
    clopts_text_out_str(&out, TEXT_FIXTURE_NIHONGO);                /* 3 */
    clopts_text_out_char(&out, ' ');                                /* 1 */
    clopts_text_out_spaces(&out, 40);                               /* 40 */
    clopts_text_out_bytes(&out, TEXT_FIXTURE_RAW_VALUE_NAME, RAW_LEN); /* 7 */
    clopts_text_out_bytes(&out, TEXT_FIXTURE_NAIVE, 3);             /* "na" and a lone C3: 3 */
    clopts_text_out_str(&out, "");                                  /* 0 */
    clopts_text_out_spaces(&out, 0);                                /* 0 */

    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(out.width, 56);
}

NAOS_TEST(measure_mode_does_not_call_the_callback)
{
    text_recorder_t rec;
    clopts_text_out_t out;

    text_recorder_init(&rec);
    /* ctx is given, but without fn nothing is written to it */
    clopts_text_out_init(&out, NULL, &rec);

    clopts_text_out_str(&out, "abc");
    clopts_text_out_char(&out, 'd');
    clopts_text_out_spaces(&out, 5);

    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(out.width, 9);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
}

NAOS_TEST(init_starts_over)
{
    text_failer_t failer;
    text_recorder_t rec;
    clopts_text_out_t out;

    text_failer_init(&failer, 1);
    clopts_text_out_init(&out, text_failer_write, &failer);
    clopts_text_out_str(&out, "x");
    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_FAILED);

    clopts_text_out_init(&out, NULL, NULL);
    clopts_text_out_str(&out, "xy");
    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(out.width, 2);

    text_recorder_init(&rec);
    clopts_text_out_init(&out, text_recorder_write, &rec);
    NAOS_CHECK_UINT_EQ(out.width, 0);
    clopts_text_out_str(&out, "z");
    NAOS_REQUIRE_INT_EQ(clopts_text_out_status(&out), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "z", (size_t)1);
}

/* ---- public functions: write failure ----------------------------------- */

/* Version string passed to clopts_write_version() by an OUT_VERSION case. */
#define OUT_VERSION_STRING "1.2.3"

/* Public write function exercised by an out_case_t. */
typedef enum out_kind
{
    OUT_HELP,
    OUT_USAGE,
    OUT_VERSION,
    OUT_HINT,
    OUT_ERROR
} out_kind_t;

/* One call of a public write function, with everything but the callback. */
typedef struct out_case
{
    const char *name;                 /* shown when a check fails */
    out_kind_t kind;
    const clopts_program_t *program;
    const clopts_command_t *command;  /* OUT_HELP / OUT_USAGE / OUT_HINT */
    const clopts_result_t *result;    /* OUT_ERROR (NULL: out of memory) */
    unsigned flags;                   /* OUT_ERROR */
} out_case_t;

static clopts_write_status_t out_case_run(const out_case_t *c, clopts_write_fn fn, void *ctx)
{
    switch (c->kind)
    {
    case OUT_HELP:
        return clopts_write_help(c->program, c->command, fn, ctx);
    case OUT_USAGE:
        return clopts_write_usage(c->program, c->command, fn, ctx);
    case OUT_VERSION:
        return clopts_write_version(c->program, OUT_VERSION_STRING, fn, ctx);
    case OUT_HINT:
        return clopts_write_usage_hint(c->program, c->command, fn, ctx);
    case OUT_ERROR:
        return clopts_write_error(c->program, c->result, c->flags, fn, ctx);
    }
    return CLOPTS_WRITE_UNAVAILABLE;
}

/*
 * c succeeds with a callback that never fails, and for every k from 1 to the
 * number of calls it makes, a callback failing on its k-th call makes it
 * return FAILED after exactly k calls, with the bytes accepted before the
 * failure being a prefix of the full text.
 */
static void out_check_case(const out_case_t *c)
{
    static text_recorder_t rec;
    static text_failer_t failer;
    size_t k;

    text_recorder_init(&rec);
    NAOS_REQUIRE_INT_EQ(out_case_run(c, text_recorder_write, &rec), CLOPTS_WRITE_OK);
    NAOS_REQUIRE_FALSE(rec.overflow);
    NAOS_REQUIRE_TRUE(rec.calls > 0);

    text_failer_init(&failer, 0);
    NAOS_CHECK_INT_EQ(out_case_run(c, text_failer_write, &failer), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(failer.calls, rec.calls);
    NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, rec.buf, rec.len);

    for (k = 1; k <= rec.calls; k++)
    {
        NAOS_TRACE_PUSH("fail at call %u", (unsigned)k);
        text_failer_init(&failer, k);
        NAOS_CHECK_INT_EQ(out_case_run(c, text_failer_write, &failer), CLOPTS_WRITE_FAILED);
        NAOS_CHECK_TRUE(failer.failed);
        NAOS_CHECK_UINT_EQ(failer.calls, k);
        NAOS_CHECK_UINT_EQ(failer.calls_after_failure, 0);
        NAOS_REQUIRE_TRUE(failer.written.len < rec.len);
        NAOS_CHECK_MEM_EQ(failer.written.buf, failer.written.len, rec.buf, failer.written.len);
        NAOS_TRACE_POP();
    }
}

/* Environment lookup that finds no variable. */
static clopts_env_status_t out_env_unset(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_UNSET;
}

NAOS_TEST(public_help_functions_stop_after_a_write_failure)
{
    text_fixture_tool_t tool;
    text_fixture_filt_t filt;
    text_fixture_bare_t bare;
    text_fixture_wide_t wide;
    size_t i;

    text_fixture_tool_init(&tool);
    text_fixture_filt_init(&filt);
    text_fixture_bare_init(&bare);
    text_fixture_wide_init(&wide);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_REQUIRE_NOT_NULL(filt.program);
    NAOS_REQUIRE_NOT_NULL(bare.program);
    NAOS_REQUIRE_NOT_NULL(wide.program);
    {
        const out_case_t cases[] = {
            { "help tool", OUT_HELP, tool.program, NULL, NULL, 0u },
            { "help tool build", OUT_HELP, tool.program, tool.build, NULL, 0u },
            { "help tool copy", OUT_HELP, tool.program, tool.copy, NULL, 0u },
            { "help tool help", OUT_HELP, tool.program, tool.help_command, NULL, 0u },
            { "help filt", OUT_HELP, filt.program, NULL, NULL, 0u },
            { "help bare", OUT_HELP, bare.program, NULL, NULL, 0u },
            { "help bare run", OUT_HELP, bare.program, bare.run, NULL, 0u },
            { "help wide", OUT_HELP, wide.program, NULL, NULL, 0u },
            { "usage tool", OUT_USAGE, tool.program, NULL, NULL, 0u },
            { "usage tool build", OUT_USAGE, tool.program, tool.build, NULL, 0u },
            { "usage filt", OUT_USAGE, filt.program, NULL, NULL, 0u },
            { "version tool", OUT_VERSION, tool.program, NULL, NULL, 0u },
            { "version bare", OUT_VERSION, bare.program, NULL, NULL, 0u },
            { "version filt", OUT_VERSION, filt.program, NULL, NULL, 0u },
            { "hint tool", OUT_HINT, tool.program, NULL, NULL, 0u },
            { "hint tool build", OUT_HINT, tool.program, tool.build, NULL, 0u },
        };

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            NAOS_TRACE_PUSH("case %s", cases[i].name);
            out_check_case(&cases[i]);
            NAOS_TRACE_POP();
        }
    }
    text_fixture_wide_free(&wide);
    text_fixture_bare_free(&bare);
    text_fixture_filt_free(&filt);
    text_fixture_tool_free(&tool);
}

NAOS_TEST(write_error_stops_after_a_write_failure)
{
    char *argv[] = { "tool", "build", "--bad=1", NULL };
    text_fixture_tool_t tool;
    clopts_result_t *usage_result;
    size_t i;

    text_fixture_tool_init(&tool);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    usage_result = clopts_parse_with_env(tool.program, 3, argv, out_env_unset, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(usage_result), CLOPTS_PARSE_USAGE_ERROR);
    {
        const unsigned both = CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR;
        const out_case_t cases[] = {
            { "usage error", OUT_ERROR, tool.program, NULL, usage_result, 0u },
            { "usage error with prefixes", OUT_ERROR, tool.program, NULL, usage_result, both },
            { "out of memory", OUT_ERROR, tool.program, NULL, NULL, 0u },
            { "out of memory with prefixes", OUT_ERROR, tool.program, NULL, NULL, both },
        };

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            NAOS_TRACE_PUSH("case %s", cases[i].name);
            out_check_case(&cases[i]);
            NAOS_TRACE_POP();
        }
    }
    clopts_result_destroy(usage_result);
    text_fixture_tool_free(&tool);
}

/* ---- public functions: FILE* output ------------------------------------ */

/*
 * The help of program written with clopts_file_write to a temporary file
 * reads back as the same bytes as through the recorder. tmpfile() opens the
 * file in binary update mode ("wb+"), so no newline translation happens on
 * any platform.
 */
static void out_check_file_help(const clopts_program_t *program)
{
    static text_recorder_t rec;
    static char back[TEXT_RECORDER_SIZE + 1];
    FILE *fp;
    size_t n;

    text_recorder_init(&rec);
    NAOS_REQUIRE_INT_EQ(clopts_write_help(program, NULL, text_recorder_write, &rec), CLOPTS_WRITE_OK);
    NAOS_REQUIRE_FALSE(rec.overflow);

    fp = tmpfile();
    NAOS_REQUIRE_NOT_NULL(fp);
    NAOS_CHECK_INT_EQ(clopts_write_help(program, NULL, clopts_file_write, fp), CLOPTS_WRITE_OK);
    NAOS_CHECK_INT_EQ(fflush(fp), 0);
    rewind(fp);
    n = fread(back, 1, sizeof(back), fp);
    NAOS_CHECK_INT_EQ(ferror(fp), 0);
    fclose(fp);

    NAOS_CHECK_MEM_EQ(back, n, rec.buf, rec.len);
}

NAOS_TEST(file_write_outputs_the_same_bytes_as_the_recorder)
{
    text_fixture_tool_t tool;
    text_fixture_wide_t wide;

    text_fixture_tool_init(&tool);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    out_check_file_help(tool.program);
    text_fixture_tool_free(&tool);

    /* non-ASCII names and bytes that are not valid UTF-8 pass through as is */
    text_fixture_wide_init(&wide);
    NAOS_REQUIRE_NOT_NULL(wide.program);
    out_check_file_help(wide.program);
    text_fixture_wide_free(&wide);
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
    NAOS_ADD_TEST(init_starts_over),
    NAOS_ADD_TEST(public_help_functions_stop_after_a_write_failure),
    NAOS_ADD_TEST(write_error_stops_after_a_write_failure),
    NAOS_ADD_TEST(file_write_outputs_the_same_bytes_as_the_recorder)
)
