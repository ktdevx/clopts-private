/*
 * Suite oom: the allocation gateway (clopts_parser_malloc / calloc / strdup /
 * strndup / vec_push / vec_free / free) and the allocation probe.
 *
 * Checked here: the gateway without a probe, fault points and the n-th and
 * later failures, the live block count returning to 0, and overflow checks
 * of count * size and len + 1.
 */

#include "parser/parser_internal.h"

#include <naos/naos.h>

#include <stdint.h>
#include <string.h>

/* Returns a probe with the given fail_at and zero counters. */
static clopts_parser_probe_t make_probe(size_t fail_at)
{
    clopts_parser_probe_t probe;

    probe.fail_at = fail_at;
    probe.calls = 0;
    probe.live = 0;
    return probe;
}

/* ---- without a probe --------------------------------------------------- */

NAOS_TEST(null_probe_allocates_and_frees)
{
    unsigned char *block;
    unsigned char *zeroed;
    char *copy;
    char *part;
    size_t i;

    block = (unsigned char *)clopts_parser_malloc(NULL, 16);
    NAOS_REQUIRE_NOT_NULL(block);
    memset(block, 0xab, 16);

    zeroed = (unsigned char *)clopts_parser_calloc(NULL, 4, 8);
    NAOS_REQUIRE_NOT_NULL(zeroed);
    for (i = 0; i < 32; i++)
    {
        NAOS_CHECK_UINT_EQ(zeroed[i], 0);
    }

    copy = clopts_parser_strdup(NULL, "build");
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_CHECK_STR_EQ(copy, "build");

    part = clopts_parser_strndup(NULL, "output=file", 6);
    NAOS_REQUIRE_NOT_NULL(part);
    NAOS_CHECK_STR_EQ(part, "output");

    NAOS_CHECK_NULL(clopts_parser_strdup(NULL, NULL));

    clopts_parser_free(NULL, block);
    clopts_parser_free(NULL, zeroed);
    clopts_parser_free(NULL, copy);
    clopts_parser_free(NULL, part);
    clopts_parser_free(NULL, NULL);
}

NAOS_TEST(null_probe_vec_push_and_free)
{
    clopts_core_vec_t vec;
    int i;

    clopts_core_vec_init(&vec, sizeof(int));
    for (i = 0; i < 20; i++)
    {
        NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(NULL, &vec, &i), 0);
    }
    NAOS_REQUIRE_UINT_EQ(vec.len, 20);
    NAOS_CHECK_INT_EQ(*(const int *)clopts_core_vec_at(&vec, 19), 19);

    clopts_parser_vec_free(NULL, &vec);
    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(vec.elem_size, sizeof(int));
}

NAOS_TEST(null_probe_overflow_fails)
{
    NAOS_CHECK_NULL(clopts_parser_calloc(NULL, SIZE_MAX, 2));
    NAOS_CHECK_NULL(clopts_parser_calloc(NULL, 2, SIZE_MAX));
    NAOS_CHECK_NULL(clopts_parser_strndup(NULL, "x", SIZE_MAX));
}

/* ---- fault points ------------------------------------------------------ */

NAOS_TEST(each_call_is_one_fault_point)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec;
    void *block;
    void *zeroed;
    char *copy;
    char *part;
    int value = 7;

    clopts_core_vec_init(&vec, sizeof(int));

    block = clopts_parser_malloc(&probe, 8);
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    zeroed = clopts_parser_calloc(&probe, 2, 4);
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    copy = clopts_parser_strdup(&probe, "tool");
    NAOS_CHECK_UINT_EQ(probe.calls, 3);
    part = clopts_parser_strndup(&probe, "tool", 2);
    NAOS_CHECK_UINT_EQ(probe.calls, 4);
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 5);
    /* The second push needs no reallocation but is still a fault point. */
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 6);

    NAOS_REQUIRE_NOT_NULL(block);
    NAOS_REQUIRE_NOT_NULL(zeroed);
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_REQUIRE_NOT_NULL(part);
    NAOS_CHECK_STR_EQ(part, "to");

    /* Releases are not fault points. */
    clopts_parser_free(&probe, block);
    clopts_parser_free(&probe, zeroed);
    clopts_parser_free(&probe, copy);
    clopts_parser_free(&probe, part);
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_UINT_EQ(probe.calls, 6);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(strdup_null_is_not_a_fault_point)
{
    clopts_parser_probe_t probe = make_probe(1);

    NAOS_CHECK_NULL(clopts_parser_strdup(&probe, NULL));
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(fail_at_zero_never_fails)
{
    clopts_parser_probe_t probe = make_probe(0);
    void *blocks[50];
    size_t i;

    for (i = 0; i < 50; i++)
    {
        blocks[i] = clopts_parser_malloc(&probe, 4);
        NAOS_REQUIRE_NOT_NULL(blocks[i]);
    }
    NAOS_CHECK_UINT_EQ(probe.calls, 50);
    NAOS_CHECK_INT_EQ((int)probe.live, 50);

    for (i = 0; i < 50; i++)
    {
        clopts_parser_free(&probe, blocks[i]);
    }
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(nth_and_later_fault_points_fail)
{
    clopts_parser_probe_t probe = make_probe(3);
    clopts_core_vec_t vec;
    void *first;
    char *second;
    int value = 1;

    clopts_core_vec_init(&vec, sizeof(int));

    first = clopts_parser_malloc(&probe, 8);
    second = clopts_parser_strdup(&probe, "build");
    NAOS_REQUIRE_NOT_NULL(first);
    NAOS_REQUIRE_NOT_NULL(second);
    NAOS_CHECK_INT_EQ((int)probe.live, 2);

    /* The 3rd fault point and every later one fail, whatever the function. */
    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, 1, 8));
    NAOS_CHECK_NULL(clopts_parser_malloc(&probe, 8));
    NAOS_CHECK_NULL(clopts_parser_strdup(&probe, "x"));
    NAOS_CHECK_NULL(clopts_parser_strndup(&probe, "xy", 1));
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), -1);
    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(vec.len, 0);
    NAOS_CHECK_UINT_EQ(vec.cap, 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 7);

    /* Failures allocate nothing. */
    NAOS_CHECK_INT_EQ((int)probe.live, 2);

    clopts_parser_free(&probe, first);
    clopts_parser_free(&probe, second);
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(fail_at_one_fails_everything)
{
    clopts_parser_probe_t probe = make_probe(1);
    clopts_core_vec_t vec;
    int value = 1;

    clopts_core_vec_init(&vec, sizeof(int));

    NAOS_CHECK_NULL(clopts_parser_malloc(&probe, 1));
    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, 1, 1));
    NAOS_CHECK_NULL(clopts_parser_strdup(&probe, "a"));
    NAOS_CHECK_NULL(clopts_parser_strndup(&probe, "a", 1));
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), -1);
    NAOS_CHECK_UINT_EQ(probe.calls, 5);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(failing_push_keeps_existing_elements)
{
    clopts_parser_probe_t probe = make_probe(2);
    clopts_core_vec_t vec;
    int value = 5;

    clopts_core_vec_init(&vec, sizeof(int));

    NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);

    value = 6;
    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, &value), -1);
    NAOS_REQUIRE_UINT_EQ(vec.len, 1);
    NAOS_CHECK_INT_EQ(*(const int *)clopts_core_vec_at(&vec, 0), 5);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

/* ---- live block count -------------------------------------------------- */

NAOS_TEST(live_counts_objects_and_strings)
{
    clopts_parser_probe_t probe = make_probe(0);
    void *block;
    void *zeroed;
    char *copy;
    char *part;

    block = clopts_parser_malloc(&probe, 8);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);
    zeroed = clopts_parser_calloc(&probe, 3, 3);
    NAOS_CHECK_INT_EQ((int)probe.live, 2);
    copy = clopts_parser_strdup(&probe, "");
    NAOS_CHECK_INT_EQ((int)probe.live, 3);
    part = clopts_parser_strndup(&probe, "abc", 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 4);

    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_REQUIRE_NOT_NULL(part);
    NAOS_CHECK_STR_EQ(copy, "");
    NAOS_CHECK_STR_EQ(part, "");

    clopts_parser_free(&probe, part);
    NAOS_CHECK_INT_EQ((int)probe.live, 3);
    clopts_parser_free(&probe, copy);
    clopts_parser_free(&probe, zeroed);
    clopts_parser_free(&probe, block);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(free_null_does_not_change_live)
{
    clopts_parser_probe_t probe = make_probe(0);

    clopts_parser_free(&probe, NULL);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
}

NAOS_TEST(vec_counts_once_per_buffer)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec;
    int i;

    clopts_core_vec_init(&vec, sizeof(int));

    /* Grows from 8 to 16 to 32; still one block. */
    for (i = 0; i < 20; i++)
    {
        NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(&probe, &vec, &i), 0);
        NAOS_CHECK_INT_EQ((int)probe.live, 1);
    }
    NAOS_CHECK_UINT_EQ(probe.calls, 20);

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);

    /* Freeing an empty vec, again or zero-initialized, does not count. */
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);

    /* After vec_free the vec can be reused and is counted again. */
    NAOS_REQUIRE_INT_EQ(clopts_parser_vec_push(&probe, &vec, &i), 0);
    NAOS_CHECK_INT_EQ((int)probe.live, 1);
    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(free_zero_initialized_vec_does_not_change_live)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec = {0};

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
}

NAOS_TEST(live_returns_to_zero_at_every_fail_at)
{
    size_t fail_at;

    /* 6 fault points in total; fail_at 7 lets everything succeed. */
    for (fail_at = 1; fail_at <= 7; fail_at++)
    {
        clopts_parser_probe_t probe = make_probe(fail_at);
        clopts_core_vec_t vec;
        void *block;
        void *zeroed;
        char *copy;
        char *part;
        int value = 3;
        int rc1;
        int rc2;

        clopts_core_vec_init(&vec, sizeof(int));

        block = clopts_parser_malloc(&probe, 8);
        zeroed = clopts_parser_calloc(&probe, 2, 8);
        copy = clopts_parser_strdup(&probe, "positional");
        rc1 = clopts_parser_vec_push(&probe, &vec, &value);
        part = clopts_parser_strndup(&probe, "positional", 3);
        rc2 = clopts_parser_vec_push(&probe, &vec, &value);

        NAOS_CHECK_UINT_EQ(probe.calls, 6);
        NAOS_CHECK_INT_EQ((block == NULL) ? 1 : 0, (fail_at <= 1) ? 1 : 0);
        NAOS_CHECK_INT_EQ((zeroed == NULL) ? 1 : 0, (fail_at <= 2) ? 1 : 0);
        NAOS_CHECK_INT_EQ((copy == NULL) ? 1 : 0, (fail_at <= 3) ? 1 : 0);
        NAOS_CHECK_INT_EQ(rc1, (fail_at <= 4) ? -1 : 0);
        NAOS_CHECK_INT_EQ((part == NULL) ? 1 : 0, (fail_at <= 5) ? 1 : 0);
        NAOS_CHECK_INT_EQ(rc2, (fail_at <= 6) ? -1 : 0);

        clopts_parser_free(&probe, block);
        clopts_parser_free(&probe, zeroed);
        clopts_parser_free(&probe, copy);
        clopts_parser_free(&probe, part);
        clopts_parser_vec_free(&probe, &vec);
        NAOS_CHECK_INT_EQ((int)probe.live, 0);
    }
}

/* ---- overflow ---------------------------------------------------------- */

NAOS_TEST(calloc_overflow_fails_without_allocating)
{
    clopts_parser_probe_t probe = make_probe(0);

    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, SIZE_MAX, 2));
    NAOS_CHECK_NULL(clopts_parser_calloc(&probe, (SIZE_MAX / 2) + 1, 2));
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(strndup_overflow_fails_without_allocating)
{
    clopts_parser_probe_t probe = make_probe(0);

    NAOS_CHECK_NULL(clopts_parser_strndup(&probe, "x", SIZE_MAX));
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_TEST(vec_push_overflow_fails_without_counting)
{
    clopts_parser_probe_t probe = make_probe(0);
    clopts_core_vec_t vec;
    unsigned char elem[16] = {0};

    /* 8 * (SIZE_MAX / 4) overflows, so the first growth is refused. */
    clopts_core_vec_init(&vec, SIZE_MAX / 4);

    NAOS_CHECK_INT_EQ(clopts_parser_vec_push(&probe, &vec, elem), -1);
    NAOS_CHECK_NULL(vec.data);
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);

    clopts_parser_vec_free(&probe, &vec);
    NAOS_CHECK_INT_EQ((int)probe.live, 0);
}

NAOS_SUITE(oom,
    NAOS_ADD_TEST(null_probe_allocates_and_frees),
    NAOS_ADD_TEST(null_probe_vec_push_and_free),
    NAOS_ADD_TEST(null_probe_overflow_fails),
    NAOS_ADD_TEST(each_call_is_one_fault_point),
    NAOS_ADD_TEST(strdup_null_is_not_a_fault_point),
    NAOS_ADD_TEST(fail_at_zero_never_fails),
    NAOS_ADD_TEST(nth_and_later_fault_points_fail),
    NAOS_ADD_TEST(fail_at_one_fails_everything),
    NAOS_ADD_TEST(failing_push_keeps_existing_elements),
    NAOS_ADD_TEST(live_counts_objects_and_strings),
    NAOS_ADD_TEST(free_null_does_not_change_live),
    NAOS_ADD_TEST(vec_counts_once_per_buffer),
    NAOS_ADD_TEST(free_zero_initialized_vec_does_not_change_live),
    NAOS_ADD_TEST(live_returns_to_zero_at_every_fail_at),
    NAOS_ADD_TEST(calloc_overflow_fails_without_allocating),
    NAOS_ADD_TEST(strndup_overflow_fails_without_allocating),
    NAOS_ADD_TEST(vec_push_overflow_fails_without_counting)
)
