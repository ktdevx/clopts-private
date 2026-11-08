/*
 * Suite errors: fields of the structured error, the first error wins,
 * recovery scan and the priority of help / version.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(fixture_required_options_are_as_designed)
{
    size_t i;

    NAOS_CHECK_TRUE(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].required);
    NAOS_CHECK_FALSE(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].required);
    NAOS_CHECK_FALSE(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].required);
    NAOS_CHECK_TRUE(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].required);
    NAOS_CHECK_INT_EQ(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].kind, CLOPTS_VALUE);

    for (i = 0; i < FIXTURE_TOOL_OPT_COUNT; i++)
    {
        NAOS_CHECK_FALSE(fixture_tool_options[i].required);
    }
    NAOS_CHECK_FALSE(fixture_cat_options[FIXTURE_CAT_OPT_TAGS].required);
    NAOS_CHECK_FALSE(fixture_filt_options[FIXTURE_FILT_OPT_LINES].required);
}

NAOS_SUITE(errors,
    NAOS_ADD_TEST(fixture_required_options_are_as_designed)
)
