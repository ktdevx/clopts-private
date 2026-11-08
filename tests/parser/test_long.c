/*
 * Suite long: long options, --name=value, value consumption, unknown long
 * options and --no-<name>.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(fixture_long_names_and_kinds_are_as_designed)
{
    NAOS_CHECK_STR_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].long_name, "verbose");
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].kind, CLOPTS_COUNT);
    NAOS_CHECK_STR_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].long_name, "color");
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].kind, CLOPTS_FLAG);
    NAOS_CHECK_STR_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].long_name, "config");
    NAOS_CHECK_INT_EQ(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].kind, CLOPTS_VALUE);

    NAOS_CHECK_STR_EQ(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].long_name, "output");
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].kind, CLOPTS_VALUE);
    NAOS_CHECK_STR_EQ(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].long_name, "keys");
    NAOS_CHECK_INT_EQ(fixture_build_options[FIXTURE_BUILD_OPT_KEYS].kind, CLOPTS_LIST);
    NAOS_CHECK_NULL(fixture_build_options[FIXTURE_BUILD_OPT_JOBS].long_name);

    NAOS_CHECK_STR_EQ(fixture_cat_options[FIXTURE_CAT_OPT_TAGS].long_name, "tags");
    NAOS_CHECK_STR_EQ(fixture_filt_options[FIXTURE_FILT_OPT_LINES].long_name, "lines");
    NAOS_CHECK_STR_EQ(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].long_name, "token");
}

NAOS_TEST(fixture_only_color_is_negatable)
{
    NAOS_CHECK_TRUE(fixture_tool_options[FIXTURE_TOOL_OPT_COLOR].negatable);
    NAOS_CHECK_FALSE(fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].negatable);
    NAOS_CHECK_FALSE(fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG].negatable);
}

NAOS_SUITE(long,
    NAOS_ADD_TEST(fixture_long_names_and_kinds_are_as_designed),
    NAOS_ADD_TEST(fixture_only_color_is_negatable)
)
