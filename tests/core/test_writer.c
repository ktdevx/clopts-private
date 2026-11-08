/*
 * Unit tests for the writer contract (clopts_write_fn, the static inline
 * clopts_file_write) and the internal helpers (clopts_core_write /
 * clopts_core_write_str).
 */

#include "core/core_internal.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

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
static const char read_only_path[] = "clopts_core_tests_writer_read_only.tmp";

/* Memory-accumulating callback context. */
typedef struct mem_sink
{
    char buf[64];
    size_t len;          /* bytes stored in buf */
    size_t calls;        /* number of callback invocations */
    size_t last_len;     /* len of the most recent invocation */
    size_t fail_on_call; /* 1-based call that fails; 0 = never fail */
} mem_sink_t;

static void mem_sink_init(mem_sink_t *sink)
{
    memset(sink, 0, sizeof(*sink));
}

static int mem_sink_write(void *ctx, const char *data, size_t len)
{
    mem_sink_t *sink = (mem_sink_t *)ctx;

    sink->calls++;
    sink->last_len = len;

    if ((sink->fail_on_call != 0) && (sink->calls == sink->fail_on_call))
    {
        return 1;
    }

    if (len > (sizeof(sink->buf) - sink->len))
    {
        return 1;
    }

    if (len > 0)
    {
        memcpy(sink->buf + sink->len, data, len);
        sink->len += len;
    }

    return 0;
}

/* Counts calls through ctx and always reports failure. */
static int always_fail_write(void *ctx, const char *data, size_t len)
{
    (void)data;
    (void)len;

    (*(int *)ctx)++;
    return 7;
}

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
    NAOS_CHECK_INT_EQ(clopts_core_write(clopts_file_write, stream, expected + 7, 3), 0);

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
    NAOS_CHECK_INT_EQ(clopts_core_write_str(clopts_file_write, stream, ""), 0);

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
    NAOS_CHECK_INT_NE(clopts_core_write_str(clopts_file_write, stream, "abc"), 0);

    fclose(stream);
    NAOS_CHECK_INT_EQ(remove(read_only_path), 0);
}

NAOS_TEST(failing_callback_returns_nonzero_without_retry)
{
    int calls = 0;

    NAOS_CHECK_INT_EQ(clopts_core_write(always_fail_write, &calls, "abc", 3), 7);
    NAOS_CHECK_INT_EQ(calls, 1);

    NAOS_CHECK_INT_EQ(clopts_core_write_str(always_fail_write, &calls, "abc"), 7);
    NAOS_CHECK_INT_EQ(calls, 2);
}

NAOS_TEST(caller_can_continue_after_failure)
{
    mem_sink_t sink;

    mem_sink_init(&sink);
    sink.fail_on_call = 2;

    NAOS_CHECK_INT_EQ(clopts_core_write_str(mem_sink_write, &sink, "one "), 0);
    NAOS_CHECK_INT_NE(clopts_core_write_str(mem_sink_write, &sink, "two "), 0);
    NAOS_CHECK_INT_EQ(clopts_core_write_str(mem_sink_write, &sink, "three"), 0);

    NAOS_CHECK_UINT_EQ(sink.calls, 3);
    NAOS_CHECK_MEM_EQ(sink.buf, sink.len, "one three", 9);
}

NAOS_TEST(memory_callback_receives_writes_in_order)
{
    mem_sink_t sink;

    mem_sink_init(&sink);

    NAOS_REQUIRE_INT_EQ(clopts_core_write(mem_sink_write, &sink, "Hello", 5), 0);
    NAOS_REQUIRE_INT_EQ(clopts_core_write_str(mem_sink_write, &sink, ", "), 0);
    NAOS_REQUIRE_INT_EQ(clopts_core_write(mem_sink_write, &sink, "world!xyz", 6), 0);

    NAOS_CHECK_UINT_EQ(sink.calls, 3);
    NAOS_CHECK_MEM_EQ(sink.buf, sink.len, "Hello, world!", 13);
}

NAOS_TEST(core_write_len_zero_is_passed_through)
{
    mem_sink_t sink;

    mem_sink_init(&sink);
    sink.last_len = 99;

    NAOS_CHECK_INT_EQ(clopts_core_write(mem_sink_write, &sink, NULL, 0), 0);
    NAOS_CHECK_UINT_EQ(sink.calls, 1);
    NAOS_CHECK_UINT_EQ(sink.last_len, 0);
    NAOS_CHECK_UINT_EQ(sink.len, 0);
}

NAOS_TEST(write_str_writes_strlen_bytes)
{
    static const char embedded[] = "abc\0def";
    mem_sink_t sink;

    mem_sink_init(&sink);

    NAOS_CHECK_INT_EQ(clopts_core_write_str(mem_sink_write, &sink, embedded), 0);
    NAOS_CHECK_UINT_EQ(sink.last_len, 3);

    NAOS_CHECK_INT_EQ(clopts_core_write_str(mem_sink_write, &sink, ""), 0);
    NAOS_CHECK_UINT_EQ(sink.last_len, 0);

    NAOS_CHECK_INT_EQ(clopts_core_write_str(mem_sink_write, &sink, "0123456789"), 0);
    NAOS_CHECK_UINT_EQ(sink.last_len, 10);

    NAOS_CHECK_UINT_EQ(sink.calls, 3);
    NAOS_CHECK_MEM_EQ(sink.buf, sink.len, "abc0123456789", 13);
}

NAOS_SUITE(writer,
    NAOS_ADD_TEST(file_write_writes_all_bytes_in_order),
    NAOS_ADD_TEST(file_write_len_zero_writes_nothing),
    NAOS_ADD_TEST(file_write_to_read_only_stream_fails),
    NAOS_ADD_TEST(failing_callback_returns_nonzero_without_retry),
    NAOS_ADD_TEST(caller_can_continue_after_failure),
    NAOS_ADD_TEST(memory_callback_receives_writes_in_order),
    NAOS_ADD_TEST(core_write_len_zero_is_passed_through),
    NAOS_ADD_TEST(write_str_writes_strlen_bytes)
)
