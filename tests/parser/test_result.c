/*
 * Suite result: access by descriptor pointer, positionals, clopts_result_run,
 * idempotent release, release of a zero-initialized result and release after
 * CLOPTS_OUT_OF_MEMORY.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(free_zero_initialized_result_is_safe)
{
    clopts_result_t result = { 0 };

    clopts_result_free(&result);

    NAOS_CHECK_NULL(result.impl);
    NAOS_CHECK_INT_EQ(result.status, CLOPTS_OK);
    NAOS_CHECK_NULL(result.command);
}

NAOS_TEST(free_zero_initialized_result_twice_is_safe)
{
    clopts_result_t result = { 0 };

    clopts_result_free(&result);
    clopts_result_free(&result);

    NAOS_CHECK_NULL(result.impl);
}

NAOS_SUITE(result,
    NAOS_ADD_TEST(free_zero_initialized_result_is_safe),
    NAOS_ADD_TEST(free_zero_initialized_result_twice_is_safe)
)
