/*
 * Unit tests for the writer contract (clopts_writer_t, clopts_writer_file)
 * and the internal helpers (clopts_core_write / clopts_core_write_str).
 */

#include "core/core_internal.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

/*
 * Creates the temporary file used by the FILE* writer tests. Kept in this
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

NAOS_TEST(file_writer_writes_bytes_in_order)
{
    static const char expected[] = { 'a', 'b', 'c', '\0', 'x', (char)0xFF, '\n', 'e', 'n', 'd' };
    FILE *stream = open_temp_file();
    clopts_writer_t writer;
    char buf[32];
    size_t n;

    NAOS_REQUIRE_NOT_NULL(stream);

    writer = clopts_writer_file(stream);
    NAOS_REQUIRE_TRUE(writer.write != NULL);

    NAOS_CHECK_INT_EQ(writer.write(writer.ctx, expected, 3), 0);
    NAOS_CHECK_INT_EQ(clopts_core_write(&writer, expected + 3, 4), 0);
    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, "end"), 0);

    n = read_back(stream, buf, sizeof(buf));
    NAOS_CHECK_MEM_EQ(buf, n, expected, sizeof(expected));

    fclose(stream);
}

NAOS_TEST(file_writer_len_zero_writes_nothing)
{
    FILE *stream = open_temp_file();
    clopts_writer_t writer;
    char buf[8];

    NAOS_REQUIRE_NOT_NULL(stream);

    writer = clopts_writer_file(stream);
    NAOS_REQUIRE_TRUE(writer.write != NULL);

    NAOS_CHECK_INT_EQ(writer.write(writer.ctx, "abc", 0), 0);
    NAOS_CHECK_INT_EQ(clopts_core_write(&writer, NULL, 0), 0);
    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, ""), 0);

    NAOS_CHECK_UINT_EQ(read_back(stream, buf, sizeof(buf)), 0);

    fclose(stream);
}

NAOS_TEST(failing_callback_returns_nonzero_without_retry)
{
    int calls = 0;
    clopts_writer_t writer;

    writer.write = always_fail_write;
    writer.ctx = &calls;

    NAOS_CHECK_INT_EQ(clopts_core_write(&writer, "abc", 3), 7);
    NAOS_CHECK_INT_EQ(calls, 1);

    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, "abc"), 7);
    NAOS_CHECK_INT_EQ(calls, 2);
}

NAOS_TEST(caller_can_continue_after_failure)
{
    mem_sink_t sink;
    clopts_writer_t writer;

    mem_sink_init(&sink);
    sink.fail_on_call = 2;
    writer.write = mem_sink_write;
    writer.ctx = &sink;

    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, "one "), 0);
    NAOS_CHECK_INT_NE(clopts_core_write_str(&writer, "two "), 0);
    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, "three"), 0);

    NAOS_CHECK_UINT_EQ(sink.calls, 3);
    NAOS_CHECK_MEM_EQ(sink.buf, sink.len, "one three", 9);
}

NAOS_TEST(memory_callback_receives_writes_in_order)
{
    mem_sink_t sink;
    clopts_writer_t writer;

    mem_sink_init(&sink);
    writer.write = mem_sink_write;
    writer.ctx = &sink;

    NAOS_REQUIRE_INT_EQ(clopts_core_write(&writer, "Hello", 5), 0);
    NAOS_REQUIRE_INT_EQ(clopts_core_write_str(&writer, ", "), 0);
    NAOS_REQUIRE_INT_EQ(clopts_core_write(&writer, "world!xyz", 6), 0);

    NAOS_CHECK_UINT_EQ(sink.calls, 3);
    NAOS_CHECK_MEM_EQ(sink.buf, sink.len, "Hello, world!", 13);
}

NAOS_TEST(core_write_len_zero_is_passed_through)
{
    mem_sink_t sink;
    clopts_writer_t writer;

    mem_sink_init(&sink);
    sink.last_len = 99;
    writer.write = mem_sink_write;
    writer.ctx = &sink;

    NAOS_CHECK_INT_EQ(clopts_core_write(&writer, NULL, 0), 0);
    NAOS_CHECK_UINT_EQ(sink.calls, 1);
    NAOS_CHECK_UINT_EQ(sink.last_len, 0);
    NAOS_CHECK_UINT_EQ(sink.len, 0);
}

NAOS_TEST(write_str_writes_strlen_bytes)
{
    static const char embedded[] = "abc\0def";
    mem_sink_t sink;
    clopts_writer_t writer;

    mem_sink_init(&sink);
    writer.write = mem_sink_write;
    writer.ctx = &sink;

    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, embedded), 0);
    NAOS_CHECK_UINT_EQ(sink.last_len, 3);

    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, ""), 0);
    NAOS_CHECK_UINT_EQ(sink.last_len, 0);

    NAOS_CHECK_INT_EQ(clopts_core_write_str(&writer, "0123456789"), 0);
    NAOS_CHECK_UINT_EQ(sink.last_len, 10);

    NAOS_CHECK_UINT_EQ(sink.calls, 3);
    NAOS_CHECK_MEM_EQ(sink.buf, sink.len, "abc0123456789", 13);
}

NAOS_SUITE(writer,
    NAOS_ADD_TEST(file_writer_writes_bytes_in_order),
    NAOS_ADD_TEST(file_writer_len_zero_writes_nothing),
    NAOS_ADD_TEST(failing_callback_returns_nonzero_without_retry),
    NAOS_ADD_TEST(caller_can_continue_after_failure),
    NAOS_ADD_TEST(memory_callback_receives_writes_in_order),
    NAOS_ADD_TEST(core_write_len_zero_is_passed_through),
    NAOS_ADD_TEST(write_str_writes_strlen_bytes)
)
