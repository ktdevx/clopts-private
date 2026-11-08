/*
 * Suite resolve: environment variables (through the mock lookup), default
 * values, sources, required checks, scope restriction and
 * CLOPTS_ENV_FAILED -> CLOPTS_OUT_OF_MEMORY, through the internal API and through
 * clopts_parse_with_env with the mock lookup.
 */

#include "test_fixtures.h"

#include "parser/parser_internal.h"

#include <naos/naos.h>

#include <stdlib.h>
#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Slot of options[index] in result (global == true: program->options). */
static clopts_parser_slot_t *slot_of(clopts_result_t *result, const clopts_option_t *options,
                                     bool global, size_t index)
{
    clopts_parser_ref_t ref = { 0 };

    ref.option = &options[index];
    ref.global = global;
    ref.index = index;
    return clopts_parser_store_slot(result->impl, &ref);
}

/* Starts a store for program, optionally selecting command, with status. */
#define BEGIN_STORE(result, prog_, cmd_, status_)                                         \
    do                                                                                    \
    {                                                                                     \
        NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&(result), (prog_)), 0);            \
        if ((cmd_) != NULL)                                                               \
        {                                                                                 \
            NAOS_REQUIRE_INT_EQ(clopts_parser_store_select(&(result), (cmd_)), 0);        \
        }                                                                                 \
        (result).status = (status_);                                                      \
        (result).command = (cmd_);                                                        \
    } while (0)

#define TOOL_BUILD (&fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD])
#define TOOL_COPY (&fixture_tool_commands[FIXTURE_TOOL_CMD_COPY])
#define TOOL_CAT (&fixture_tool_commands[FIXTURE_TOOL_CMD_CAT])
#define NO_CMD ((const clopts_command_t *)NULL)

/*
 * Local declarations.
 *
 *   order   global --global VALUE required
 *           cmd:   --local VALUE required, ARG required
 *   need    without subcommands: --key VALUE required (env NEED_KEY),
 *           --items LIST ',' (env NEED_ITEMS), FILES required variadic
 */
static const clopts_option_t order_global_options[] = {
    { .long_name = "global", .kind = CLOPTS_VALUE, .required = true },
};
static const clopts_option_t order_cmd_options[] = {
    { .long_name = "local", .kind = CLOPTS_VALUE, .required = true },
};
static const clopts_positional_t order_cmd_positionals[] = {
    { .name = "ARG", .required = true },
};
static const clopts_command_t order_commands[] = {
    { .name = "cmd",
      .options = order_cmd_options,
      .option_count = 1,
      .positionals = order_cmd_positionals,
      .positional_count = 1 },
};
static const clopts_program_t order_program = {
    .name = "order",
    .options = order_global_options,
    .option_count = 1,
    .commands = order_commands,
    .command_count = 1,
};

enum
{
    NEED_OPT_KEY,
    NEED_OPT_ITEMS,
    NEED_OPT_COUNT
};
static const clopts_option_t need_options[NEED_OPT_COUNT] = {
    { .long_name = "key", .kind = CLOPTS_VALUE, .env = "NEED_KEY", .required = true },
    { .long_name = "items", .kind = CLOPTS_LIST, .env = "NEED_ITEMS", .list_delimiter = ',' },
};
static const clopts_positional_t need_positionals[] = {
    { .name = "FILES", .required = true, .variadic = true },
};
static const clopts_program_t need_program = {
    .name = "need",
    .options = need_options,
    .option_count = NEED_OPT_COUNT,
    .positionals = need_positionals,
    .positional_count = 1,
};

/* Checks the fields of a missing-required error besides kind and target. */
#define CHECK_MISSING_COMMON(error, command_)                                             \
    do                                                                                    \
    {                                                                                     \
        NAOS_CHECK_PTR_EQ((error).command, (command_));                                   \
        NAOS_CHECK_NULL((error).token);                                                   \
        NAOS_CHECK_INT_EQ((error).argv_index, -1);                                        \
        NAOS_CHECK_UINT_EQ((error).name_len, 0);                                          \
        NAOS_CHECK_INT_EQ((error).short_name, '\0');                                      \
    } while (0)

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

NAOS_TEST(local_declarations_are_valid)
{
    NAOS_CHECK_INT_EQ(clopts_decl_validate(&order_program, NULL), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_decl_validate(&need_program, NULL), CLOPTS_OK);
}

/* ---- default lookup ---------------------------------------------------- */

NAOS_TEST(getenv_default_unset_leaves_out_untouched)
{
    char sentinel = 'x';
    char *out = &sentinel;

    NAOS_CHECK_INT_EQ(
        clopts_parser_getenv_default(NULL, "CLOPTS_TEST_UNSET_9f3c1a7e_NEVER_DEFINED", &out),
        CLOPTS_ENV_UNSET);
    NAOS_CHECK_PTR_EQ(out, &sentinel);
}

/* ---- VALUE: sources ---------------------------------------------------- */

NAOS_TEST(value_from_env_is_env_and_owned_copy)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    const clopts_option_t *config = &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG];
    clopts_result_t result = { 0 };
    fixture_env_t env;
    const char *value;

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &fixture_tool, NO_CMD, CLOPTS_NO_COMMAND);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, false), 0);
    value = clopts_result_value(&result, config);
    NAOS_REQUIRE_NOT_NULL(value);
    NAOS_CHECK_STR_EQ(value, "/etc/t");
    NAOS_CHECK_TRUE(value != vars[0].value);
    NAOS_CHECK_PTR_EQ(slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_CONFIG)->owned,
                      value);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));

    clopts_result_free(&result);
}

NAOS_TEST(value_explicit_wins_over_env)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    const clopts_option_t *config = &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG];
    clopts_result_t result = { 0 };
    fixture_env_t env;
    static const char explicit_value[] = "x";

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &fixture_tool, TOOL_BUILD, CLOPTS_OK);
    clopts_parser_store_set_value(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_CONFIG), explicit_value,
        CLOPTS_SOURCE_EXPLICIT);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, config), explicit_value);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, config), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_CONFIG"));

    clopts_result_free(&result);
}

NAOS_TEST(value_without_env_or_default_is_none)
{
    const clopts_option_t *config = &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG];
    const clopts_option_t *output = &fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    BEGIN_STORE(result, &fixture_tool, TOOL_BUILD, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_NULL(clopts_result_value(&result, config));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, config), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(clopts_result_value(&result, output));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, output), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(clopts_result_list_count(
                           &result, &fixture_build_options[FIXTURE_BUILD_OPT_KEYS]),
                       0);
    NAOS_CHECK_INT_EQ(
        clopts_result_source(&result, &fixture_build_options[FIXTURE_BUILD_OPT_KEYS]),
        CLOPTS_SOURCE_NONE);
    /* FLAG / COUNT are left alone. */
    NAOS_CHECK_FALSE(clopts_result_flag(&result, &fixture_tool_options[FIXTURE_TOOL_OPT_COLOR]));
    NAOS_CHECK_UINT_EQ(
        clopts_result_count(&result, &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE]), 0);
    NAOS_CHECK_INT_EQ(
        clopts_result_source(&result, &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE]),
        CLOPTS_SOURCE_NONE);
    /* Only TOOL_CONFIG is declared in this scope. */
    NAOS_CHECK_UINT_EQ(env.query_count, 1);

    clopts_result_free(&result);
}

NAOS_TEST(value_default_when_env_unset)
{
    const clopts_option_t *lines = &fixture_filt_options[FIXTURE_FILT_OPT_LINES];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    BEGIN_STORE(result, &fixture_filt, NO_CMD, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, lines), lines->default_value);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, lines), "10");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, lines), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    NAOS_CHECK_NULL(slot_of(&result, fixture_filt_options, true, FIXTURE_FILT_OPT_LINES)->owned);

    clopts_result_free(&result);
}

NAOS_TEST(value_env_wins_over_default)
{
    static const fixture_env_var_t vars[] = { { "FILT_LINES", "5" } };
    const clopts_option_t *lines = &fixture_filt_options[FIXTURE_FILT_OPT_LINES];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &fixture_filt, NO_CMD, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, lines), CLOPTS_SOURCE_ENV);

    clopts_result_free(&result);
}

NAOS_TEST(value_empty_env_is_env)
{
    static const fixture_env_var_t vars[] = { { "FILT_LINES", "" } };
    const clopts_option_t *lines = &fixture_filt_options[FIXTURE_FILT_OPT_LINES];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &fixture_filt, NO_CMD, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_REQUIRE_NOT_NULL(clopts_result_value(&result, lines));
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, lines), "");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, lines), CLOPTS_SOURCE_ENV);

    clopts_result_free(&result);
}

/* ---- LIST -------------------------------------------------------------- */

NAOS_TEST(list_default_is_split)
{
    const clopts_option_t *tags = &fixture_cat_options[FIXTURE_CAT_OPT_TAGS];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    BEGIN_STORE(result, &fixture_tool, TOOL_CAT, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(&result, tags), 2);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, tags, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, tags, 1), "b");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, tags), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_TAGS"));

    clopts_result_free(&result);
}

NAOS_TEST(list_env_is_split_including_empty_items)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x,,y" } };
    const clopts_option_t *tags = &fixture_cat_options[FIXTURE_CAT_OPT_TAGS];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &fixture_tool, TOOL_CAT, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(&result, tags), 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, tags, 0), "x");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, tags, 1), "");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, tags, 2), "y");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, tags), CLOPTS_SOURCE_ENV);

    clopts_result_free(&result);
}

NAOS_TEST(list_empty_env_is_one_empty_item)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "" } };
    const clopts_option_t *tags = &fixture_cat_options[FIXTURE_CAT_OPT_TAGS];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &fixture_tool, TOOL_CAT, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(&result, tags), 1);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, tags, 0), "");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, tags), CLOPTS_SOURCE_ENV);

    clopts_result_free(&result);
}

NAOS_TEST(list_explicit_is_kept)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x,y" } };
    const clopts_option_t *tags = &fixture_cat_options[FIXTURE_CAT_OPT_TAGS];
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &fixture_tool, TOOL_CAT, CLOPTS_OK);
    NAOS_REQUIRE_INT_EQ(
        clopts_parser_store_append_list(slot_of(&result, fixture_cat_options, false,
                                                FIXTURE_CAT_OPT_TAGS),
                                        "q", ',', CLOPTS_SOURCE_EXPLICIT),
        0);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(&result, tags), 1);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, tags, 0), "q");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, tags), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));

    clopts_result_free(&result);
}

/* ---- failure ----------------------------------------------------------- */

NAOS_TEST(env_failed_fails_without_default_fallback)
{
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    env.fail_next = true;
    BEGIN_STORE(result, &fixture_filt, NO_CMD, CLOPTS_OK);

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), -1);
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    /* Not silently replaced by the default. */
    NAOS_CHECK_INT_EQ(
        slot_of(&result, fixture_filt_options, true, FIXTURE_FILT_OPT_LINES)->source,
        CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot_of(&result, fixture_filt_options, true, FIXTURE_FILT_OPT_LINES)->value);

    clopts_result_free(&result);
}

NAOS_TEST(env_failed_stops_at_once)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x" } };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 1);
    env.fail_next = true;
    BEGIN_STORE(result, &fixture_tool, TOOL_CAT, CLOPTS_OK);

    /* TOOL_CONFIG (global) fails; TOOL_TAGS (command) is not looked up. */
    NAOS_CHECK_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), -1);
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));

    clopts_result_free(&result);
}

/* ---- scope ------------------------------------------------------------- */

NAOS_TEST(scope_selected_command_only)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "c" },
        { "TOOL_TAGS", "t" },
    };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 2);
    BEGIN_STORE(result, &fixture_tool, TOOL_BUILD, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    NAOS_CHECK_UINT_EQ(env.query_count, 1);

    clopts_result_free(&result);
}

NAOS_TEST(scope_global_only_skips_command)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "c" },
        { "TOOL_TAGS", "t" },
    };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 2);
    BEGIN_STORE(result, &fixture_tool, TOOL_CAT, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, false), 0);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    NAOS_CHECK_INT_EQ(
        slot_of(&result, fixture_cat_options, false, FIXTURE_CAT_OPT_TAGS)->source,
        CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(
        slot_of(&result, fixture_cat_options, false, FIXTURE_CAT_OPT_TAGS)->items.len, 0);

    clopts_result_free(&result);
}

/* ---- required ---------------------------------------------------------- */

NAOS_TEST(required_option_missing)
{
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    BEGIN_STORE(result, &fixture_tool, TOOL_BUILD, CLOPTS_OK);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, "x"), 0);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT]);
    NAOS_CHECK_NULL(result.error.positional);
    CHECK_MISSING_COMMON(result.error, TOOL_BUILD);

    clopts_result_free(&result);
}

NAOS_TEST(required_positional_missing)
{
    clopts_result_t result = { 0 };

    BEGIN_STORE(result, &fixture_tool, TOOL_BUILD, CLOPTS_OK);
    clopts_parser_store_set_value(
        slot_of(&result, fixture_build_options, false, FIXTURE_BUILD_OPT_OUTPUT), "y",
        CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result.error.positional, &fixture_build_positionals[FIXTURE_BUILD_POS_SRC]);
    NAOS_CHECK_NULL(result.error.option);
    CHECK_MISSING_COMMON(result.error, TOOL_BUILD);

    /* With SRC given and EXTRA (optional variadic) empty, nothing is missing. */
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, "src"), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), 0);

    clopts_result_free(&result);
}

NAOS_TEST(required_second_positional_missing)
{
    clopts_result_t result = { 0 };

    BEGIN_STORE(result, &fixture_tool, TOOL_COPY, CLOPTS_OK);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, "a"), 0);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result.error.positional, &fixture_copy_positionals[FIXTURE_COPY_POS_DST]);
    CHECK_MISSING_COMMON(result.error, TOOL_COPY);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, "b"), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), 0);

    clopts_result_free(&result);
}

NAOS_TEST(required_order_global_command_positional)
{
    clopts_result_t result = { 0 };
    const clopts_command_t *cmd = &order_commands[0];

    BEGIN_STORE(result, &order_program, cmd, CLOPTS_OK);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &order_global_options[0]);
    CHECK_MISSING_COMMON(result.error, cmd);

    clopts_parser_store_set_value(slot_of(&result, order_global_options, true, 0), "g",
                                  CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &order_cmd_options[0]);
    CHECK_MISSING_COMMON(result.error, cmd);

    clopts_parser_store_set_value(slot_of(&result, order_cmd_options, false, 0), "l",
                                  CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result.error.positional, &order_cmd_positionals[0]);
    NAOS_CHECK_NULL(result.error.option);
    CHECK_MISSING_COMMON(result.error, cmd);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, "arg"), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), 0);

    clopts_result_free(&result);
}

NAOS_TEST(required_global_only_skips_command)
{
    clopts_result_t result = { 0 };

    /* NO_COMMAND: only global options are checked. */
    BEGIN_STORE(result, &order_program, NO_CMD, CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, false), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &order_global_options[0]);
    CHECK_MISSING_COMMON(result.error, NO_CMD);

    clopts_parser_store_set_value(slot_of(&result, order_global_options, true, 0), "g",
                                  CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, false), 0);
    clopts_result_free(&result);

    /* A selected command's requirements are skipped as well. */
    BEGIN_STORE(result, &order_program, &order_commands[0], CLOPTS_OK);
    clopts_parser_store_set_value(slot_of(&result, order_global_options, true, 0), "g",
                                  CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, false), 0);
    clopts_result_free(&result);
}

NAOS_TEST(required_global_of_strict_with_no_command)
{
    clopts_result_t result = { 0 };

    BEGIN_STORE(result, &fixture_strict, NO_CMD, CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, false), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN]);
    CHECK_MISSING_COMMON(result.error, NO_CMD);

    clopts_result_free(&result);
}

NAOS_TEST(required_satisfied_by_env_and_variadic_needs_one)
{
    static const fixture_env_var_t vars[] = { { "NEED_KEY", "k" } };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, 1);
    BEGIN_STORE(result, &need_program, NO_CMD, CLOPTS_OK);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, &need_options[NEED_OPT_KEY]),
                      CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, &need_options[NEED_OPT_KEY]), "k");

    /* The required variadic FILES got no token. */
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result.error.positional, &need_positionals[0]);
    NAOS_CHECK_NULL(result.error.option);
    CHECK_MISSING_COMMON(result.error, NO_CMD);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, "f"), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), 0);

    clopts_result_free(&result);
}

NAOS_TEST(required_missing_when_env_unset)
{
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    BEGIN_STORE(result, &need_program, NO_CMD, CLOPTS_OK);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, "f"), 0);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(&result, fixture_env_getenv, &env, true), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), -1);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &need_options[NEED_OPT_KEY]);
    CHECK_MISSING_COMMON(result.error, NO_CMD);

    clopts_result_free(&result);
}

NAOS_TEST(nothing_required_is_ok)
{
    clopts_result_t result = { 0 };

    BEGIN_STORE(result, &fixture_tool, TOOL_CAT, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), 0);
    clopts_result_free(&result);

    BEGIN_STORE(result, &fixture_filt, NO_CMD, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(&result, true), 0);
    clopts_result_free(&result);
}

/* ---- through clopts_parse_with_env ------------------------------------- */

/* Number of elements of a static array. */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define TOOL_CONFIG_OPT (&fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG])

/* Parses argv (NULL-terminated, argc elements) with the mock environment env. */
static clopts_status_t parse_env(const clopts_program_t *program, int argc, char **argv,
                                 fixture_env_t *env, clopts_result_t *result)
{
    clopts_status_t status =
        clopts_parse_with_env(program, argc, argv, fixture_env_getenv, env, result);

    NAOS_CHECK_INT_EQ(result->status, status);
    return status;
}

NAOS_TEST(parse_config_from_env_is_env)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "cat", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_tool, 2, argv, &env, &result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TOOL_CONFIG_OPT), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, TOOL_CONFIG_OPT), "/etc/t");
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    clopts_result_free(&result);
}

NAOS_TEST(parse_config_explicit_wins_over_env)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "--config", "x", "cat", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_tool, 4, argv, &env, &result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TOOL_CONFIG_OPT), CLOPTS_SOURCE_EXPLICIT);
    /* An explicit value points into argv. */
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, TOOL_CONFIG_OPT), argv[2]);
    clopts_result_free(&result);
}

NAOS_TEST(parse_config_unset_is_none)
{
    char *argv[] = { "tool", "cat", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_tool, 2, argv, &env, &result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TOOL_CONFIG_OPT), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(clopts_result_value(&result, TOOL_CONFIG_OPT));
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    clopts_result_free(&result);
}

NAOS_TEST(parse_config_empty_env_is_env)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "" } };
    char *argv[] = { "tool", "cat", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_tool, 2, argv, &env, &result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TOOL_CONFIG_OPT), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, TOOL_CONFIG_OPT), "");
    clopts_result_free(&result);
}

NAOS_TEST(parse_config_from_env_with_no_command)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "-vv", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    /* NO_COMMAND still resolves the global options. */
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_tool, 2, argv, &env, &result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TOOL_CONFIG_OPT), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, TOOL_CONFIG_OPT), "/etc/t");
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    clopts_result_free(&result);
}

NAOS_TEST(parse_build_does_not_query_other_commands_env)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x,y" },
    };
    char *argv[] = { "tool", "build", "src", "-o", "out", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_tool, 5, argv, &env, &result), CLOPTS_OK);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    /* TOOL_TAGS belongs to cat, which was not selected. */
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    clopts_result_free(&result);
}

NAOS_TEST(parse_required_satisfied_by_env)
{
    static const fixture_env_var_t vars[] = { { "NEED_KEY", "k" } };
    char *argv[] = { "need", "f", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(parse_env(&need_program, 2, argv, &env, &result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, &need_options[NEED_OPT_KEY]),
                      CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, &need_options[NEED_OPT_KEY]), "k");
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 1);
    clopts_result_free(&result);
}

NAOS_TEST(parse_required_missing_when_env_unset)
{
    char *argv[] = { "need", "f", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    NAOS_REQUIRE_INT_EQ(parse_env(&need_program, 2, argv, &env, &result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &need_options[NEED_OPT_KEY]);
    NAOS_CHECK_NULL(result.error.positional);
    CHECK_MISSING_COMMON(result.error, NO_CMD);
    clopts_result_free(&result);
}

NAOS_TEST(parse_build_without_output_is_missing_option)
{
    char *argv[] = { "tool", "build", "x", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_tool, 3, argv, &env, &result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT]);
    NAOS_CHECK_NULL(result.error.positional);
    CHECK_MISSING_COMMON(result.error, TOOL_BUILD);
    NAOS_CHECK_PTR_EQ(result.command, TOOL_BUILD);
    clopts_result_free(&result);
}

NAOS_TEST(parse_strict_no_command_with_required_global)
{
    char *missing[] = { "strict", NULL };
    char *given[] = { "strict", "--token", "t", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    /* No subcommand: the required global is still checked. */
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_strict, 1, missing, &env, &result),
                        CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(result.error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result.error.option, &fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN]);
    CHECK_MISSING_COMMON(result.error, NO_CMD);

    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_strict, 3, given, &env, &result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_PTR_EQ(
        clopts_result_value(&result, &fixture_strict_options[FIXTURE_STRICT_OPT_TOKEN]),
        given[2]);
    clopts_result_free(&result);
}

NAOS_TEST(parse_filt_default_points_into_the_declaration)
{
    const clopts_option_t *lines = &fixture_filt_options[FIXTURE_FILT_OPT_LINES];
    char *argv[] = { "filt", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, NULL, 0);
    NAOS_REQUIRE_INT_EQ(parse_env(&fixture_filt, 1, argv, &env, &result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, lines), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, lines), "10");
    /* The default is not copied. */
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, lines), lines->default_value);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    clopts_result_free(&result);
}

NAOS_TEST(parse_env_failed_is_out_of_memory_and_freeable)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x" } };
    char *argv[] = { "tool", "cat", "f", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    env.fail_next = true;
    NAOS_CHECK_INT_EQ(parse_env(&fixture_tool, 3, argv, &env, &result), CLOPTS_OUT_OF_MEMORY);
    /* The lookup stops at the first failure: TOOL_TAGS is not looked up. */
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

NAOS_SUITE(resolve,
    NAOS_ADD_TEST(fixture_env_and_defaults_are_as_designed),
    NAOS_ADD_TEST(mock_env_finds_copies_and_records),
    NAOS_ADD_TEST(mock_env_fail_next_fails_once),
    NAOS_ADD_TEST(local_declarations_are_valid),
    NAOS_ADD_TEST(getenv_default_unset_leaves_out_untouched),
    NAOS_ADD_TEST(value_from_env_is_env_and_owned_copy),
    NAOS_ADD_TEST(value_explicit_wins_over_env),
    NAOS_ADD_TEST(value_without_env_or_default_is_none),
    NAOS_ADD_TEST(value_default_when_env_unset),
    NAOS_ADD_TEST(value_env_wins_over_default),
    NAOS_ADD_TEST(value_empty_env_is_env),
    NAOS_ADD_TEST(list_default_is_split),
    NAOS_ADD_TEST(list_env_is_split_including_empty_items),
    NAOS_ADD_TEST(list_empty_env_is_one_empty_item),
    NAOS_ADD_TEST(list_explicit_is_kept),
    NAOS_ADD_TEST(env_failed_fails_without_default_fallback),
    NAOS_ADD_TEST(env_failed_stops_at_once),
    NAOS_ADD_TEST(scope_selected_command_only),
    NAOS_ADD_TEST(scope_global_only_skips_command),
    NAOS_ADD_TEST(required_option_missing),
    NAOS_ADD_TEST(required_positional_missing),
    NAOS_ADD_TEST(required_second_positional_missing),
    NAOS_ADD_TEST(required_order_global_command_positional),
    NAOS_ADD_TEST(required_global_only_skips_command),
    NAOS_ADD_TEST(required_global_of_strict_with_no_command),
    NAOS_ADD_TEST(required_satisfied_by_env_and_variadic_needs_one),
    NAOS_ADD_TEST(required_missing_when_env_unset),
    NAOS_ADD_TEST(nothing_required_is_ok),
    NAOS_ADD_TEST(parse_config_from_env_is_env),
    NAOS_ADD_TEST(parse_config_explicit_wins_over_env),
    NAOS_ADD_TEST(parse_config_unset_is_none),
    NAOS_ADD_TEST(parse_config_empty_env_is_env),
    NAOS_ADD_TEST(parse_config_from_env_with_no_command),
    NAOS_ADD_TEST(parse_build_does_not_query_other_commands_env),
    NAOS_ADD_TEST(parse_required_satisfied_by_env),
    NAOS_ADD_TEST(parse_required_missing_when_env_unset),
    NAOS_ADD_TEST(parse_build_without_output_is_missing_option),
    NAOS_ADD_TEST(parse_strict_no_command_with_required_global),
    NAOS_ADD_TEST(parse_filt_default_points_into_the_declaration),
    NAOS_ADD_TEST(parse_env_failed_is_out_of_memory_and_freeable)
)
