/*
 * Suite dispatch: the stage before a subcommand is selected (subcommand
 * names, help, version, global options and NO_COMMAND).
 *
 * Until the scanner is implemented, only the sentinel is checked here:
 * parsing a NULL program returns it, with no selected command.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(parse_null_program_returns_sentinel)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
}

NAOS_SUITE(dispatch,
    NAOS_ADD_TEST(parse_null_program_returns_sentinel)
)
