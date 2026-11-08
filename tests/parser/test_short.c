/*
 * Suite short: short option groups, attached values, -h and unknown short
 * options.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(fixture_short_names_are_as_designed)
{
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].short_name, 'v');
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].short_name, '\0');
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].short_name, '\0');

    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].short_name, 'o');
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].short_name, 'k');
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].short_name, 'j');
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].kind, CLOPTS_COUNT);

    NAOS_CHECK_INT_EQ(fixture_cat_options[FIXTURE_CAT_OPT_TAGS].short_name, '\0');
    NAOS_CHECK_INT_EQ(fixture_filt_options[FIXTURE_FILT_OPT_LINES].short_name, 'n');
    NAOS_CHECK_INT_EQ(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].short_name, '\0');
}

NAOS_SUITE(short,
    NAOS_ADD_TEST(fixture_short_names_are_as_designed)
)
