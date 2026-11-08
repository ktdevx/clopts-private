/*
 * Suite resolve: environment variables, defaults and required checks.
 *
 * Until the resolver is implemented, only the sentinel is checked here:
 * parsing a NULL program returns it without calling the lookup callback.
 */

#include <clopts/parse.h>

#include <naos/naos.h>

/* Counts its calls in *ctx and reports every variable as unset. */
static clopts_env_status_t counting_getenv(void *ctx, const char *name, const char **value)
{
    int *calls = (int *)ctx;

    (void)name;
    (void)value;
    (*calls)++;
    return CLOPTS_ENV_UNSET;
}

NAOS_TEST(null_program_does_not_call_getenv)
{
    clopts_result_t *result;
    int calls = 0;

    result = clopts_parse_with_env(NULL, 0, NULL, counting_getenv, &calls);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(calls, 0);
    clopts_result_free(result);
}

NAOS_SUITE(resolve,
    NAOS_ADD_TEST(null_program_does_not_call_getenv)
)
