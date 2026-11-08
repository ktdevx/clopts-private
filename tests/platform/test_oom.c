/*
 * Suite oom: the allocation gateway and the allocation probe, and sweeps
 * that move the failing fault point over handle creation, the Windows
 * conversions and environment lookups (the sentinel or a recorded
 * OUT_OF_MEMORY, and live == 0 after the release).
 *
 * It also checks that the helpers of test_util hold the longest array they
 * accept, which the long inputs of the sweeps rely on.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

NAOS_TEST(u16_helpers_fill_the_whole_capacity)
{
    static char text[PLATFORM_TEST_U16_CAP];
    static uint16_t units[PLATFORM_TEST_U16_CAP - 1];
    static platform_test_u16_t s;
    size_t i;

    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    platform_test_u16_from_ascii(&s, text);
    NAOS_CHECK_UINT_EQ(s.len, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len - 1], 'x');
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, text));

    for (i = 0; i < PLATFORM_TEST_U16_CAP - 1; ++i)
    {
        units[i] = (uint16_t)(i + 1);
    }
    platform_test_u16_from_units(&s, units, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.len, PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len - 1], PLATFORM_TEST_U16_CAP - 1);
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
}

/* Without a probe the gateway allocates and releases usable blocks. */
NAOS_TEST(no_probe_malloc_and_free)
{
    unsigned char *p;
    uint16_t *a;

    p = (unsigned char *)clopts_platform_malloc(NULL, 16);
    NAOS_CHECK_NOT_NULL(p);
    if (p != NULL)
    {
        memset(p, 0xab, 16);
        NAOS_CHECK_UINT_EQ(p[15], 0xab);
    }
    clopts_platform_free(NULL, p);

    a = (uint16_t *)clopts_platform_malloc_array(NULL, 8, sizeof(uint16_t));
    NAOS_CHECK_NOT_NULL(a);
    if (a != NULL)
    {
        a[7] = 0x1234;
        NAOS_CHECK_UINT_EQ(a[7], 0x1234);
    }
    clopts_platform_free(NULL, a);

    clopts_platform_free(NULL, NULL);
}

/* A size of 0 (or a count of 0) is a success, not NULL, with or without a probe. */
NAOS_TEST(zero_size_succeeds)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *p;
    void *q;
    void *r;

    p = clopts_platform_malloc(NULL, 0);
    NAOS_CHECK_NOT_NULL(p);
    clopts_platform_free(NULL, p);

    q = clopts_platform_malloc(&probe, 0);
    NAOS_CHECK_NOT_NULL(q);
    r = clopts_platform_malloc_array(&probe, 0, 4);
    NAOS_CHECK_NOT_NULL(r);
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    NAOS_CHECK_UINT_EQ(probe.live, 2);
    clopts_platform_free(&probe, q);
    clopts_platform_free(&probe, r);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/* fail_at == 0 never fails; calls counts every attempt and live every block. */
NAOS_TEST(probe_counts_calls_and_live)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *blocks[5];
    size_t i;

    for (i = 0; i < 5; ++i)
    {
        blocks[i] = (i % 2 == 0) ? clopts_platform_malloc(&probe, 8)
                                 : clopts_platform_malloc_array(&probe, 3, 4);
        NAOS_CHECK_NOT_NULL(blocks[i]);
        NAOS_CHECK_UINT_EQ(probe.calls, i + 1);
        NAOS_CHECK_UINT_EQ(probe.live, i + 1);
    }
    for (i = 0; i < 5; ++i)
    {
        clopts_platform_free(&probe, blocks[i]);
        NAOS_CHECK_UINT_EQ(probe.live, 4 - i);
    }
    NAOS_CHECK_UINT_EQ(probe.calls, 5);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/* The fail_at-th attempt and every later one fail; earlier ones succeed. */
NAOS_TEST(probe_fails_nth_and_later)
{
    size_t n;

    for (n = 1; n <= 4; ++n)
    {
        clopts_platform_probe_t probe = { 0, 0, 0 };
        void *blocks[6];
        size_t i;

        probe.fail_at = n;
        for (i = 0; i < 6; ++i)
        {
            blocks[i] = (i % 2 == 0) ? clopts_platform_malloc(&probe, 4)
                                     : clopts_platform_malloc_array(&probe, 2, 2);
            if (i + 1 < n)
            {
                NAOS_CHECK_NOT_NULL(blocks[i]);
            }
            else
            {
                NAOS_CHECK_NULL(blocks[i]);
            }
            NAOS_CHECK_UINT_EQ(probe.calls, i + 1);
        }
        NAOS_CHECK_UINT_EQ(probe.live, n - 1);
        for (i = 0; i < 6; ++i)
        {
            clopts_platform_free(&probe, blocks[i]);
        }
        NAOS_CHECK_UINT_EQ(probe.live, 0);
    }
}

/* Releasing NULL is ignored and never moves live. */
NAOS_TEST(probe_free_null_keeps_live)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *p;

    p = clopts_platform_malloc(&probe, 1);
    NAOS_CHECK_NOT_NULL(p);
    clopts_platform_free(&probe, NULL);
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    clopts_platform_free(&probe, p);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/*
 * count * size overflow returns NULL without allocating. Like the parser's
 * gateway, the attempt still passes the fault point (calls grows by one).
 */
NAOS_TEST(malloc_array_overflow_does_not_allocate)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    void *p;

    NAOS_CHECK_NULL(clopts_platform_malloc_array(NULL, SIZE_MAX, 2));
    NAOS_CHECK_NULL(clopts_platform_malloc_array(NULL, (SIZE_MAX / 4) + 1, 4));

    NAOS_CHECK_NULL(clopts_platform_malloc_array(&probe, SIZE_MAX, 2));
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
    NAOS_CHECK_NULL(clopts_platform_malloc_array(&probe, 2, SIZE_MAX));
    NAOS_CHECK_UINT_EQ(probe.calls, 2);
    NAOS_CHECK_UINT_EQ(probe.live, 0);

    /* A product that fits still allocates after the overflows. */
    p = clopts_platform_malloc_array(&probe, 1, 32);
    NAOS_CHECK_NOT_NULL(p);
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    clopts_platform_free(&probe, p);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_SUITE(oom,
    NAOS_ADD_TEST(u16_helpers_fill_the_whole_capacity),
    NAOS_ADD_TEST(no_probe_malloc_and_free),
    NAOS_ADD_TEST(zero_size_succeeds),
    NAOS_ADD_TEST(probe_counts_calls_and_live),
    NAOS_ADD_TEST(probe_fails_nth_and_later),
    NAOS_ADD_TEST(probe_free_null_keeps_live),
    NAOS_ADD_TEST(malloc_array_overflow_does_not_allocate)
)
