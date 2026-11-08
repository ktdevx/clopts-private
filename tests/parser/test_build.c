/*
 * Suite build: the builder functions, NULL safety, the first recorded error,
 * string copies, freezing and handle stability.
 *
 * Until the builder is implemented, only the behavior that never changes is
 * checked here: validating a NULL program returns the out-of-memory
 * sentinel, and releasing it and a NULL program is safe.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(validate_null_program_returns_sentinel)
{
    clopts_result_t *result;

    result = clopts_program_validate(NULL);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    clopts_result_free(result);
    clopts_program_free(NULL);
}

NAOS_SUITE(build,
    NAOS_ADD_TEST(validate_null_program_returns_sentinel)
)
