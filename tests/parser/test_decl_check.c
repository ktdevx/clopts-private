/*
 * Suite decl: the immediate checks of the builder (one case per rule) and the
 * lookups shared with the scanner.
 *
 * Until the checks are implemented, only the sentinel is checked here:
 * validating a NULL program reports no declaration error and no offending
 * handle.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(validate_null_program_has_no_decl_error)
{
    clopts_result_t *result;

    result = clopts_program_validate(NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    clopts_result_free(result);
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(validate_null_program_has_no_decl_error)
)
