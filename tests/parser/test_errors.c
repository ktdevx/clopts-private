/*
 * Suite errors: usage errors, their fields, the recovery scan and the
 * priority between help, version and errors.
 *
 * Until the scanner is implemented, only the sentinel is checked here: every
 * error accessor reports no error.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(sentinel_has_no_usage_error)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_NONE);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_NULL(clopts_result_error_token(result));
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), -1);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');
    clopts_result_free(result);
}

NAOS_SUITE(errors,
    NAOS_ADD_TEST(sentinel_has_no_usage_error)
)
