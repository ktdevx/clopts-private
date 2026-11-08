/*
 * Suite positional: positional arguments, "--", "-" and variadic positionals.
 *
 * Until the scanner is implemented, only the sentinel is checked here: it has
 * no positionals.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(sentinel_has_no_positionals)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
}

NAOS_SUITE(positional,
    NAOS_ADD_TEST(sentinel_has_no_positionals)
)
