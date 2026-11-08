/*
 * Suite resolve: environment variables (through the mock lookup), default
 * values, sources, required checks, scope restriction and
 * CLOPTS_ENV_FAILED -> CLOPTS_OUT_OF_MEMORY.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <stdlib.h>

NAOS_TEST(fixture_env_and_defaults_are_as_designed)
{
    const clopts_option_t *config = &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG];
    const clopts_option_t *tags = &fixture_cat_options[FIXTURE_CAT_OPT_TAGS];
    const clopts_option_t *lines = &fixture_filt_options[FIXTURE_FILT_OPT_LINES];

    NAOS_CHECK_STR_EQ(config->env, "TOOL_CONFIG");
    NAOS_CHECK_NULL(config->default_value);
    NAOS_CHECK_STR_EQ(tags->env, "TOOL_TAGS");
    NAOS_CHECK_STR_EQ(tags->default_value, "a,b");
    NAOS_CHECK_STR_EQ(lines->env, "FILT_LINES");
    NAOS_CHECK_STR_EQ(lines->default_value, "10");

    NAOS_CHECK_NULL(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].env);
    NAOS_CHECK_NULL(fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT].default_value);
    NAOS_CHECK_NULL(fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN].env);
}

NAOS_TEST(mock_env_finds_copies_and_records)
{
    static const fixture_env_var_t vars[] = {
        { "FILT_LINES", "5" },
        { "EMPTY", "" },
    };
    fixture_env_t env;
    char *out = NULL;

    fixture_env_init(&env, vars, sizeof(vars) / sizeof(vars[0]));

    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "FILT_LINES", &out), CLOPTS_ENV_FOUND);
    NAOS_REQUIRE_NOT_NULL(out);
    NAOS_CHECK_STR_EQ(out, "5");
    NAOS_CHECK_TRUE(out != vars[0].value);
    free(out);

    out = NULL;
    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "EMPTY", &out), CLOPTS_ENV_FOUND);
    NAOS_REQUIRE_NOT_NULL(out);
    NAOS_CHECK_STR_EQ(out, "");
    free(out);

    out = NULL;
    NAOS_CHECK_INT_EQ(fixture_env_getenv(&env, "TOOL_CONFIG", &out), CLOPTS_ENV_UNSET);
    NAOS_CHECK_NULL(out);

    NAOS_CHECK_UINT_EQ(env.query_count, 3);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
}

NAOS_TEST(mock_env_fail_next_fails_once)
{
    static const fixture_env_var_t vars[] = {
        { "FILT_LINES", "5" },
    };
    fixture_env_t env;
    char *out = NULL;

    fixture_env_init(&env, vars, 1);
    env.fail_next = true;

    NAOS_CHECK_INT_EQ(fixture_env_getenv(&env, "FILT_LINES", &out), CLOPTS_ENV_FAILED);
    NAOS_CHECK_NULL(out);
    NAOS_CHECK_FALSE(env.fail_next);

    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "FILT_LINES", &out), CLOPTS_ENV_FOUND);
    NAOS_REQUIRE_NOT_NULL(out);
    NAOS_CHECK_STR_EQ(out, "5");
    free(out);
    NAOS_CHECK_UINT_EQ(env.query_count, 2);
}

NAOS_SUITE(resolve,
    NAOS_ADD_TEST(fixture_env_and_defaults_are_as_designed),
    NAOS_ADD_TEST(mock_env_finds_copies_and_records),
    NAOS_ADD_TEST(mock_env_fail_next_fails_once)
)
