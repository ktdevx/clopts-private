/*
 * Suite dispatch: subcommand selection, global options before and after the
 * subcommand, NO_COMMAND and the help / --help / --version forms handled
 * before a subcommand.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(fixture_commands_are_shaped_as_designed)
{
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    const clopts_command_t *copy = &fixture_tool_commands[FIXTURE_TOOL_CMD_COPY];
    const clopts_command_t *cat = &fixture_tool_commands[FIXTURE_TOOL_CMD_CAT];

    NAOS_CHECK_STR_EQ(build->name, "build");
    NAOS_CHECK_PTR_EQ(build->options, fixture_build_options);
    NAOS_CHECK_UINT_EQ(build->option_count, 3);
    NAOS_CHECK_PTR_EQ(build->positionals, fixture_build_positionals);
    NAOS_CHECK_UINT_EQ(build->positional_count, 2);
    NAOS_CHECK_TRUE(build->handler == fixture_handler);
    NAOS_CHECK_PTR_EQ(build->userdata, &fixture_build_handler_record);

    NAOS_CHECK_STR_EQ(copy->name, "copy");
    NAOS_CHECK_UINT_EQ(copy->option_count, 0);
    NAOS_CHECK_UINT_EQ(copy->positional_count, 2);

    NAOS_CHECK_STR_EQ(cat->name, "cat");
    NAOS_CHECK_PTR_EQ(cat->options, fixture_cat_options);
    NAOS_CHECK_UINT_EQ(cat->option_count, 1);
    NAOS_CHECK_UINT_EQ(cat->positional_count, 1);
}

NAOS_TEST(fixture_filt_has_no_commands_and_a_handler)
{
    NAOS_CHECK_UINT_EQ(fixture_filt.command_count, 0);
    NAOS_CHECK_PTR_EQ(fixture_filt.positionals, fixture_filt_positionals);
    NAOS_CHECK_TRUE(fixture_filt.handler == fixture_handler);
    NAOS_CHECK_PTR_EQ(fixture_filt.userdata, &fixture_filt_handler_record);
}

NAOS_SUITE(dispatch,
    NAOS_ADD_TEST(fixture_commands_are_shaped_as_designed),
    NAOS_ADD_TEST(fixture_filt_has_no_commands_and_a_handler)
)
