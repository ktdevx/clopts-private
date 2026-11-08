/*
 * Suite result: the result handle (recording rules, accessors, run and
 * release), driven through the internal API.
 *
 * Until the result handle is implemented, only the sentinel is checked here:
 * a NULL result reads like the sentinel, and releasing NULL or the sentinel
 * (even repeatedly) is safe.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

NAOS_TEST(null_result_reads_like_sentinel)
{
    clopts_result_t *sentinel;

    sentinel = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(sentinel), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_status(NULL), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_command(NULL));
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(NULL), CLOPTS_USAGE_NONE);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(NULL), CLOPTS_DECL_NONE);
    NAOS_CHECK_NULL(clopts_result_error_token(NULL));
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(NULL), -1);
    NAOS_CHECK_FALSE(clopts_result_flag(NULL, NULL));
    NAOS_CHECK_NULL(clopts_result_value(NULL, NULL));
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(NULL), 0);
    clopts_result_free(NULL);
    clopts_result_free(sentinel);
    clopts_result_free(sentinel);
    NAOS_CHECK_INT_EQ(clopts_result_status(sentinel), CLOPTS_OUT_OF_MEMORY);
}

NAOS_SUITE(result,
    NAOS_ADD_TEST(null_result_reads_like_sentinel)
)
