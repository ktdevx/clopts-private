/*
 * Suite short: short options and short option groups in the main scan.
 *
 * Until the scanner is implemented, only the sentinel is checked here: it
 * reports no offending short name and no name length.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(sentinel_has_no_short_name)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);
    clopts_result_free(result);
}

NAOS_SUITE(short,
    NAOS_ADD_TEST(sentinel_has_no_short_name)
)
