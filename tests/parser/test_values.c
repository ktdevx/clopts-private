/*
 * Suite values: recording of FLAG / COUNT / VALUE / LIST values through argv.
 *
 * Until the scanner is implemented, only the sentinel is checked here: every
 * value accessor returns the unset value.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(sentinel_values_are_unset)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_FALSE(clopts_result_flag(result, NULL));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, NULL), 0);
    NAOS_CHECK_NULL(clopts_result_value(result, NULL));
    NAOS_CHECK_UINT_EQ(clopts_result_list_count(result, NULL), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, NULL), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);
}

NAOS_SUITE(values,
    NAOS_ADD_TEST(sentinel_values_are_unset)
)
