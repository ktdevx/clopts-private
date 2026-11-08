/*
 * Suite values: repetition rules of FLAG / COUNT / VALUE / LIST, list
 * delimiters and the count limit.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(fixture_list_delimiters_are_as_designed)
{
    const clopts_option_t *keys = &fixture_build_options[FIXTURE_BUILD_OPT_KEYS];
    const clopts_option_t *tags = &fixture_cat_options[FIXTURE_CAT_OPT_TAGS];

    NAOS_CHECK_INT_EQ(keys->kind, CLOPTS_LIST);
    NAOS_CHECK_INT_EQ(keys->list_delimiter, ',');
    NAOS_CHECK_INT_EQ(tags->kind, CLOPTS_LIST);
    NAOS_CHECK_INT_EQ(tags->list_delimiter, ',');
}

NAOS_TEST(fixture_count_limits_are_as_designed)
{
    const clopts_option_t *jobs = &fixture_build_options[FIXTURE_BUILD_OPT_JOBS];
    const clopts_option_t *verbose = &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE];

    NAOS_CHECK_INT_EQ(jobs->kind, CLOPTS_COUNT);
    NAOS_CHECK_UINT_EQ(jobs->count_max, 8);
    /* 0 selects CLOPTS_COUNT_MAX_DEFAULT. */
    NAOS_CHECK_INT_EQ(verbose->kind, CLOPTS_COUNT);
    NAOS_CHECK_UINT_EQ(verbose->count_max, 0);
}

NAOS_SUITE(values,
    NAOS_ADD_TEST(fixture_list_delimiters_are_as_designed),
    NAOS_ADD_TEST(fixture_count_limits_are_as_designed)
)
