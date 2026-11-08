/*
 * Suite long: long options (--name, --name=value, --no-name) in the main scan.
 *
 * Until the scanner is implemented, only the sentinel is checked here:
 * parsing a NULL program with the default environment lookup returns the
 * same sentinel as clopts_parse().
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(parse_with_env_null_program_returns_sentinel)
{
    clopts_result_t *result;

    result = clopts_parse_with_env(NULL, 0, NULL, NULL, NULL);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_PTR_EQ(result, clopts_parse(NULL, 0, NULL));
    clopts_result_free(result);
}

NAOS_SUITE(long,
    NAOS_ADD_TEST(parse_with_env_null_program_returns_sentinel)
)
