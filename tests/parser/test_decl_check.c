/*
 * Suite decl: declaration validation (clopts_decl_validate and the
 * declaration check run by clopts_parse*).
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(fixture_programs_are_shaped_as_designed)
{
    NAOS_CHECK_STR_EQ(fixture_tool.name, "tool");
    NAOS_CHECK_STR_EQ(fixture_tool.version, "1.2.3");
    NAOS_CHECK_PTR_EQ(fixture_tool.options, fixture_tool_options);
    NAOS_CHECK_UINT_EQ(fixture_tool.option_count, 3);
    NAOS_CHECK_PTR_EQ(fixture_tool.commands, fixture_tool_commands);
    NAOS_CHECK_UINT_EQ(fixture_tool.command_count, 3);

    NAOS_CHECK_STR_EQ(fixture_filt.name, "filt");
    NAOS_CHECK_NULL(fixture_filt.version);
    NAOS_CHECK_UINT_EQ(fixture_filt.option_count, 1);
    NAOS_CHECK_UINT_EQ(fixture_filt.positional_count, 1);
    NAOS_CHECK_UINT_EQ(fixture_filt.command_count, 0);
    NAOS_CHECK_NULL(fixture_filt.commands);

    NAOS_CHECK_STR_EQ(fixture_strict.name, "strict");
    NAOS_CHECK_UINT_EQ(fixture_strict.option_count, 1);
    NAOS_CHECK_UINT_EQ(fixture_strict.command_count, 1);
    NAOS_CHECK_STR_EQ(fixture_strict_commands[FIXTURE_STRICT_CMD_RUN].name, "run");
    NAOS_CHECK_UINT_EQ(fixture_strict_commands[FIXTURE_STRICT_CMD_RUN].option_count, 0);
    NAOS_CHECK_UINT_EQ(fixture_strict_commands[FIXTURE_STRICT_CMD_RUN].positional_count, 0);
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(fixture_programs_are_shaped_as_designed)
)
