/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite resolve: environment variables, defaults and required checks.
 *
 * Driven through the internal API: a program is built with the builder, a
 * result is created with clopts_parser_result_new(), a command is selected
 * with clopts_parser_result_select(), explicit values and positionals are
 * recorded with the slot functions, then clopts_parser_resolve() and
 * clopts_parser_check_required() are called. The environment comes from a
 * local mock that lends strings from its own buffer.
 *
 * The last part goes through clopts_parse_with_env() with the shared fixtures
 * of test_fixtures.c and fixture_env_t (ported from the first-version suite).
 */

#include "parser/parser_internal.h"
#include "test_alloc.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <stdlib.h>
#include <string.h>

/* ---- borrowing mock getenv -------------------------------------------- */

#define MOCK_MAX_VARS 8
#define MOCK_MAX_QUERIES 32
#define MOCK_BUFFER_SIZE 64

/*
 * A name -> value table. A found value is copied into buffer and lent from
 * there, so the same storage is reused by every call. With clobber set, the
 * buffer is overwritten at the start of every call (the previous loan ends).
 * fail_next makes the next call return CLOPTS_ENV_FAILED once; fail_name
 * makes every query of that name fail.
 */
typedef struct mock_env
{
    const char *names[MOCK_MAX_VARS];
    const char *values[MOCK_MAX_VARS];
    size_t count;
    bool fail_next;
    const char *fail_name;  /* every query of this name fails; NULL for none */
    const char *invalid_names[2];  /* every query of these names reports INVALID; NULL for none */
    bool clobber;
    const char *queried[MOCK_MAX_QUERIES];
    size_t queried_count;
    char buffer[MOCK_BUFFER_SIZE];
} mock_env_t;

static void mock_init(mock_env_t *env)
{
    memset(env, 0, sizeof(*env));
}

static void mock_set(mock_env_t *env, const char *name, const char *value)
{
    if (env->count < MOCK_MAX_VARS)
    {
        env->names[env->count] = name;
        env->values[env->count] = value;
        env->count++;
    }
}

/* Overwrites the lent buffer. */
static void mock_clobber(mock_env_t *env)
{
    memset(env->buffer, '#', sizeof(env->buffer) - 1);
    env->buffer[sizeof(env->buffer) - 1] = '\0';
}

/* Returns how many times name was queried. */
static size_t mock_queries(const mock_env_t *env, const char *name)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < env->queried_count; i++)
    {
        if (strcmp(env->queried[i], name) == 0)
        {
            n++;
        }
    }
    return n;
}

static clopts_env_status_t mock_getenv(void *ctx, const char *name, const char **value)
{
    mock_env_t *env = (mock_env_t *)ctx;
    size_t i;

    if (env->clobber)
    {
        mock_clobber(env);
    }
    if (env->queried_count < MOCK_MAX_QUERIES)
    {
        env->queried[env->queried_count] = name;
        env->queried_count++;
    }
    if (env->fail_next)
    {
        env->fail_next = false;
        return CLOPTS_ENV_FAILED;
    }
    if ((env->fail_name != NULL) && (strcmp(env->fail_name, name) == 0))
    {
        return CLOPTS_ENV_FAILED;
    }
    for (i = 0; i < sizeof(env->invalid_names) / sizeof(env->invalid_names[0]); i++)
    {
        if ((env->invalid_names[i] != NULL) && (strcmp(env->invalid_names[i], name) == 0))
        {
            return CLOPTS_ENV_INVALID;
        }
    }
    for (i = 0; i < env->count; i++)
    {
        if (strcmp(env->names[i], name) == 0)
        {
            size_t len = strlen(env->values[i]);

            if (len >= sizeof(env->buffer))
            {
                return CLOPTS_ENV_FAILED;
            }
            memcpy(env->buffer, env->values[i], len + 1);
            *value = env->buffer;
            return CLOPTS_ENV_FOUND;
        }
    }
    return CLOPTS_ENV_UNSET;
}

/* Counts its calls in *ctx and reports every variable as unset. */
static clopts_env_status_t counting_getenv(void *ctx, const char *name, const char **value)
{
    int *calls = (int *)ctx;

    (void)name;
    (void)value;
    (*calls)++;
    return CLOPTS_ENV_UNSET;
}

/* ---- fixture ---------------------------------------------------------- */

/*
 * tool
 *   global: --config/-c (VALUE, env TOOL_CONFIG, default "def.conf"),
 *           --include/-I (LIST ',', env TOOL_INCLUDE, default "a,b"),
 *           --name (VALUE), --token (VALUE, required, env TOOL_TOKEN),
 *           --tag (LIST without delimiter, env TOOL_TAG), --verbose/-v (COUNT)
 *   build:  --output/-o (VALUE, env BUILD_OUTPUT, default "out"),
 *           --mode (VALUE, required); positional src (required)
 *   copy:   --dest (VALUE, env COPY_DEST); positional files (required, variadic)
 */
typedef struct sample
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of sample_build() succeeded */
    clopts_option_t *config;
    clopts_option_t *include;
    clopts_option_t *name;
    clopts_option_t *token;
    clopts_option_t *tag;
    clopts_option_t *verbose;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *mode;
    clopts_positional_t *src;
    clopts_command_t *copy;
    clopts_option_t *dest;
    clopts_positional_t *files;
} sample_t;

#define SURELY_UNSET_NAME "CLOPTS_TEST_SURELY_UNSET_VARIABLE_6_3"

/*
 * Builds the sample program. With unset_env, every option that has an
 * environment variable among the global options gets SURELY_UNSET_NAME instead (the
 * variable must be chosen before the option is attached).
 */
static void sample_build(sample_t *s, bool unset_env)
{
    memset(s, 0, sizeof(*s));
    test_decl_reset();
    s->program = test_decl_begin("tool");
    s->config = test_decl_option(CLOPTS_OPTION_VALUE, "config", 'c');
    test_decl_check(clopts_option_set_env(s->config, unset_env ? SURELY_UNSET_NAME : "TOOL_CONFIG"));
    test_decl_check(clopts_option_set_default(s->config, "def.conf"));
    s->config = test_decl_global_attach(s->program, s->config);
    s->include = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    test_decl_check(clopts_option_set_delimiter(s->include, ','));
    test_decl_check(clopts_option_set_env(s->include, unset_env ? SURELY_UNSET_NAME : "TOOL_INCLUDE"));
    test_decl_check(clopts_option_set_default(s->include, "a,b"));
    s->include = test_decl_global_attach(s->program, s->include);
    s->name = test_decl_option(CLOPTS_OPTION_VALUE, "name", '\0');
    s->name = test_decl_global_attach(s->program, s->name);
    s->token = test_decl_option(CLOPTS_OPTION_VALUE, "token", '\0');
    test_decl_check(clopts_option_set_required(s->token, true));
    test_decl_check(clopts_option_set_env(s->token, unset_env ? SURELY_UNSET_NAME : "TOOL_TOKEN"));
    s->token = test_decl_global_attach(s->program, s->token);
    s->tag = test_decl_option(CLOPTS_OPTION_LIST, "tag", '\0');
    test_decl_check(clopts_option_set_env(s->tag, unset_env ? SURELY_UNSET_NAME : "TOOL_TAG"));
    s->tag = test_decl_global_attach(s->program, s->tag);
    s->verbose = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    s->verbose = test_decl_global_attach(s->program, s->verbose);
    s->build = test_decl_command(s->program, "build");
    s->output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    test_decl_check(clopts_option_set_env(s->output, "BUILD_OUTPUT"));
    test_decl_check(clopts_option_set_default(s->output, "out"));
    s->output = test_decl_attach(s->build, s->output);
    s->mode = test_decl_option(CLOPTS_OPTION_VALUE, "mode", '\0');
    test_decl_check(clopts_option_set_required(s->mode, true));
    s->mode = test_decl_attach(s->build, s->mode);
    s->src = test_decl_positional("src");
    clopts_positional_set_required(s->src, true);
    s->src = test_decl_attach_positional(s->build, s->src);
    s->copy = test_decl_command(s->program, "copy");
    s->dest = test_decl_option(CLOPTS_OPTION_VALUE, "dest", '\0');
    test_decl_check(clopts_option_set_env(s->dest, "COPY_DEST"));
    s->dest = test_decl_attach(s->copy, s->dest);
    s->files = test_decl_positional("files");
    clopts_positional_set_required(s->files, true);
    clopts_positional_set_variadic(s->files, true);
    s->files = test_decl_attach_positional(s->copy, s->files);
    s->program = test_decl_finish(s->program);
    if (s->program == NULL)
    {
        /* Everything was released with the program. */
        memset(s, 0, sizeof(*s));
    }
    s->ok = (test_decl_failures == 0);
}

static void sample_init(sample_t *s)
{
    sample_build(s, false);
}

/* Returns the slot of option (a global option or one of the selected command), found by its address. */
static clopts_parser_slot_t *slot_of(clopts_result_t *result, const clopts_option_t *option)
{
    clopts_parser_slot_t *slot = clopts_parser_result_find_slot(result, option);

    NAOS_REQUIRE_NOT_NULL(slot);
    return slot;
}

/* Returns item index of the LIST slot. */
static const char *item_of(const clopts_parser_slot_t *slot, size_t index)
{
    return *(char *const *)clopts_core_vec_at(&slot->items, index);
}

#define SAMPLE_BEGIN(s, result)                                                     \
    do                                                                              \
    {                                                                               \
        test_alloc_begin(0);                                                        \
        sample_init(&(s));                                                          \
        NAOS_REQUIRE_NOT_NULL((s).program);                                         \
        NAOS_REQUIRE_TRUE((s).ok);                                                  \
        (result) = clopts_parser_result_new((s).program);                           \
        NAOS_REQUIRE_NOT_NULL(result);                                              \
    } while (0)

#define SAMPLE_END(s, result)                                                       \
    do                                                                              \
    {                                                                               \
        clopts_result_destroy(result);                                                 \
        clopts_program_destroy((s).program);                                           \
        NAOS_CHECK_TRUE(test_alloc_end());                                          \
    } while (0)


/* ---- sentinel --------------------------------------------------------- */

NAOS_TEST(null_program_does_not_call_getenv)
{
    clopts_result_t *result;
    int calls = 0;

    result = clopts_parse_with_env(NULL, 0, NULL, counting_getenv, &calls);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(calls, 0);
    clopts_result_destroy(result);
}

/* ---- process environment (the lookup itself: tests/process/test_env.c) ---- */

/*
 * Without a lookup callback, the parse reads the process environment: the
 * variable of --config is unset there, so its default applies.
 */
NAOS_TEST(null_getenv_fn_reads_the_process_environment)
{
    sample_t s;
    clopts_result_t *result;
    static char *const argv[] = { "tool", "--token", "t", NULL };

    test_alloc_begin(0);
    sample_build(&s, true);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);

    result = clopts_parse_with_env(s.program, 3, argv, NULL, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.config), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.config), "def.conf");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.token), CLOPTS_SOURCE_EXPLICIT);

    SAMPLE_END(s, result);
}

/* ---- priority --------------------------------------------------------- */

NAOS_TEST(explicit_wins_over_env_and_default)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    static const char cli[] = "cli.conf";
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_CONFIG", "env.conf");
    mock_set(&env, "TOOL_INCLUDE", "e1,e2");
    clopts_parser_slot_set_value(slot_of(result, s.config), cli, CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot_of(result, s.include), "x",
                                                       ',', CLOPTS_SOURCE_EXPLICIT), 0);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    slot = slot_of(result, s.config);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(slot->value, cli);
    slot = slot_of(result, s.include);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 1);
    NAOS_CHECK_STR_EQ(item_of(slot, 0), "x");
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "TOOL_CONFIG"), 0);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "TOOL_INCLUDE"), 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(env_wins_over_default)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_CONFIG", "env.conf");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    slot = slot_of(result, s.config);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(slot->value, "env.conf");
    NAOS_CHECK_NOT_NULL(slot->owned);
    NAOS_CHECK_PTR_EQ(slot->value, slot->owned);
    NAOS_CHECK_TRUE(slot->value != env.buffer);

    SAMPLE_END(s, result);
}

NAOS_TEST(default_when_env_is_unset)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    slot = slot_of(result, s.config);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_PTR_EQ(slot->value, clopts_option_get_default(s.config));
    NAOS_CHECK_NULL(slot->owned);
    slot = slot_of(result, s.name);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot->value);
    slot = slot_of(result, s.tag);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(slot->items.len, 0);
    NAOS_CHECK_INT_EQ(slot_of(result, s.verbose)->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "TOOL_CONFIG"), 1);

    SAMPLE_END(s, result);
}

NAOS_TEST(empty_env_string_is_an_env_value)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_CONFIG", "");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    slot = slot_of(result, s.config);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_NOT_NULL(slot->value);
    NAOS_CHECK_STR_EQ(slot->value, "");

    SAMPLE_END(s, result);
}

NAOS_TEST(declaration_order_global_then_selected_command)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_REQUIRE_UINT_EQ(env.queried_count, 5);
    NAOS_CHECK_STR_EQ(env.queried[0], "TOOL_CONFIG");
    NAOS_CHECK_STR_EQ(env.queried[1], "TOOL_INCLUDE");
    NAOS_CHECK_STR_EQ(env.queried[2], "TOOL_TOKEN");
    NAOS_CHECK_STR_EQ(env.queried[3], "TOOL_TAG");
    NAOS_CHECK_STR_EQ(env.queried[4], "BUILD_OUTPUT");
    NAOS_CHECK_INT_EQ(slot_of(result, s.output)->source, CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_STR_EQ(slot_of(result, s.output)->value, "out");
    NAOS_CHECK_INT_EQ(slot_of(result, s.mode)->source, CLOPTS_SOURCE_NONE);

    SAMPLE_END(s, result);
}

/* ---- LIST ------------------------------------------------------------- */

NAOS_TEST(list_default_is_split_by_the_delimiter)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    slot = slot_of(result, s.include);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_DEFAULT);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 2);
    NAOS_CHECK_STR_EQ(item_of(slot, 0), "a");
    NAOS_CHECK_STR_EQ(item_of(slot, 1), "b");

    SAMPLE_END(s, result);
}

NAOS_TEST(list_env_is_split_by_the_delimiter)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_INCLUDE", "x,,y");
    mock_set(&env, "TOOL_TAG", "p,q");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    slot = slot_of(result, s.include);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 3);
    NAOS_CHECK_STR_EQ(item_of(slot, 0), "x");
    NAOS_CHECK_STR_EQ(item_of(slot, 1), "");
    NAOS_CHECK_STR_EQ(item_of(slot, 2), "y");
    slot = slot_of(result, s.tag);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 1);
    NAOS_CHECK_STR_EQ(item_of(slot, 0), "p,q");

    SAMPLE_END(s, result);
}

/* ---- scope ------------------------------------------------------------ */

NAOS_TEST(unselected_command_env_is_not_queried)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    mock_set(&env, "COPY_DEST", "d");
    mock_set(&env, "BUILD_OUTPUT", "bin");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "COPY_DEST"), 0);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "BUILD_OUTPUT"), 1);
    NAOS_CHECK_INT_EQ(slot_of(result, s.output)->source, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(slot_of(result, s.output)->value, "bin");

    SAMPLE_END(s, result);
}

NAOS_TEST(no_selected_command_queries_only_the_global_options)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "COPY_DEST", "d");
    mock_set(&env, "BUILD_OUTPUT", "bin");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_CHECK_UINT_EQ(env.queried_count, 4);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "COPY_DEST"), 0);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "BUILD_OUTPUT"), 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(include_command_false_resolves_only_the_global_options)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    mock_set(&env, "BUILD_OUTPUT", "bin");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, false), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "BUILD_OUTPUT"), 0);
    NAOS_CHECK_INT_EQ(slot_of(result, s.output)->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot_of(result, s.output)->value);
    NAOS_CHECK_INT_EQ(slot_of(result, s.config)->source, CLOPTS_SOURCE_DEFAULT);

    SAMPLE_END(s, result);
}

/* ---- failures and borrowing ------------------------------------------- */

NAOS_TEST(failed_lookup_returns_error_without_default)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    env.fail_next = true;

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(env.queried_count, 1);
    NAOS_CHECK_INT_EQ(slot_of(result, s.config)->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot_of(result, s.config)->value);

    SAMPLE_END(s, result);
}

NAOS_TEST(failed_lookup_in_the_command_returns_error)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    mock_set(&env, "TOOL_INCLUDE", "i");
    env.fail_name = "BUILD_OUTPUT";

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(slot_of(result, s.include)->source, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_INT_EQ(slot_of(result, s.output)->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot_of(result, s.output)->value);

    SAMPLE_END(s, result);
}

/* ---- values that cannot be read ---------------------------------------- */

/*
 * An INVALID value leaves its slot unset (the default is not used either),
 * the first one is recorded as the error, and the other options are still
 * resolved.
 */
NAOS_TEST(invalid_value_leaves_the_slot_unset_and_records_the_first_error)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    const clopts_parser_error_t *error;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    env.invalid_names[0] = "TOOL_CONFIG";
    env.invalid_names[1] = "BUILD_OUTPUT";
    mock_set(&env, "TOOL_TOKEN", "t");

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true),
                      CLOPTS_PARSER_RESOLVE_INVALID);
    NAOS_CHECK_INT_EQ(slot_of(result, s.config)->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot_of(result, s.config)->value);
    NAOS_CHECK_INT_EQ(slot_of(result, s.output)->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot_of(result, s.output)->value);
    /* the resolution went on after the first one */
    NAOS_CHECK_INT_EQ(slot_of(result, s.token)->source, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(slot_of(result, s.token)->value, "t");
    NAOS_CHECK_INT_EQ(slot_of(result, s.include)->source, CLOPTS_SOURCE_DEFAULT);

    error = clopts_parser_result_get_error(result);
    NAOS_CHECK_INT_EQ(error->usage, CLOPTS_USAGE_INVALID_ENV_VALUE);
    NAOS_CHECK_PTR_EQ(error->option, s.config);
    NAOS_CHECK_PTR_EQ(error->command, s.build);
    NAOS_CHECK_NULL(error->positional);
    NAOS_CHECK_NULL(error->token);
    NAOS_CHECK_INT_EQ(error->argv_index, -1);
    NAOS_CHECK_UINT_EQ(error->name_len, 0);
    NAOS_CHECK_INT_EQ(error->short_name, '\0');

    SAMPLE_END(s, result);
}

/* Without the selected command, the error names no command, as for a missing option. */
NAOS_TEST(invalid_value_without_the_command_names_no_command)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    env.invalid_names[0] = "TOOL_TAG";

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, false),
                      CLOPTS_PARSER_RESOLVE_INVALID);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->option, s.tag);
    NAOS_CHECK_NULL(clopts_parser_result_get_error(result)->command);
    NAOS_CHECK_UINT_EQ(slot_of(result, s.tag)->items.len, 0);

    SAMPLE_END(s, result);
}

/* A FAILED lookup after an INVALID one still ends the resolution with out of memory. */
NAOS_TEST(failed_lookup_wins_over_an_invalid_value)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    env.invalid_names[0] = "TOOL_CONFIG";
    env.fail_name = "TOOL_TOKEN";

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true),
                      CLOPTS_PARSER_RESOLVE_OUT_OF_MEMORY);

    SAMPLE_END(s, result);
}

/*
 * tool with --format (VALUE, env TOOL_FORMAT, default "text") and the
 * terminal --help, parsed with mock as the environment.
 */
static clopts_result_t *parse_format_tool(clopts_program_t **program, clopts_option_t **format, mock_env_t *env,
                                          int argc, char **argv)
{
    clopts_option_t *option;

    test_decl_reset();
    *program = test_decl_begin("tool");
    option = test_decl_option(CLOPTS_OPTION_VALUE, "format", '\0');
    test_decl_check(clopts_option_set_env(option, "TOOL_FORMAT"));
    test_decl_check(clopts_option_set_default(option, "text"));
    *format = test_decl_global_attach(*program, option);
    (void)test_decl_global_attach(*program, test_decl_help_option());
    *program = test_decl_finish(*program);
    NAOS_REQUIRE_NOT_NULL(*program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    return clopts_parse_with_env(*program, argc, argv, mock_getenv, env);
}

NAOS_TEST(parse_reports_an_invalid_value_as_a_usage_error)
{
    static char arg0[] = "tool";
    static char *argv[] = {arg0, NULL};
    clopts_program_t *program;
    clopts_option_t *format;
    clopts_result_t *result;
    mock_env_t env;

    test_alloc_begin(0);
    mock_init(&env);
    env.invalid_names[0] = "TOOL_FORMAT";
    result = parse_format_tool(&program, &format, &env, 1, argv);

    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_INVALID_ENV_VALUE);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), format);
    NAOS_CHECK_NULL(clopts_result_get_error_command(result));
    NAOS_CHECK_NULL(clopts_result_get_error_token(result));
    NAOS_CHECK_INT_EQ(clopts_result_get_error_argv_index(result), -1);

    clopts_result_destroy(result);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* A terminal option still ends the parse with TERMINATED, as it does after a usage error in argv. */
NAOS_TEST(parse_terminal_option_wins_over_an_invalid_value)
{
    static char arg0[] = "tool";
    static char arg1[] = "--help";
    static char *argv[] = {arg0, arg1, NULL};
    clopts_program_t *program;
    clopts_option_t *format;
    clopts_result_t *result;
    mock_env_t env;

    test_alloc_begin(0);
    mock_init(&env);
    env.invalid_names[0] = "TOOL_FORMAT";
    result = parse_format_tool(&program, &format, &env, 2, argv);

    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_NONE);
    NAOS_CHECK_NULL(clopts_result_get_value(result, format));

    clopts_result_destroy(result);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(borrowed_buffer_overwritten_after_return_keeps_values)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    env.clobber = true;
    mock_set(&env, "TOOL_CONFIG", "one");
    mock_set(&env, "TOOL_INCLUDE", "i1,i2");
    mock_set(&env, "TOOL_TOKEN", "tok");
    mock_set(&env, "BUILD_OUTPUT", "bin");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    mock_clobber(&env);
    NAOS_CHECK_STR_EQ(slot_of(result, s.config)->value, "one");
    NAOS_CHECK_INT_EQ(slot_of(result, s.config)->source, CLOPTS_SOURCE_ENV);
    slot = slot_of(result, s.include);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 2);
    NAOS_CHECK_STR_EQ(item_of(slot, 0), "i1");
    NAOS_CHECK_STR_EQ(item_of(slot, 1), "i2");
    NAOS_CHECK_STR_EQ(slot_of(result, s.token)->value, "tok");
    NAOS_CHECK_STR_EQ(slot_of(result, s.output)->value, "bin");

    SAMPLE_END(s, result);
}

NAOS_TEST(alloc_failure_on_the_env_copy_returns_error)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_CONFIG", "env.conf");
    test_alloc_set_fail_at(test_alloc.calls + 1);

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(env.queried_count, 1);
    NAOS_CHECK_INT_EQ(slot_of(result, s.config)->source, CLOPTS_SOURCE_NONE);

    SAMPLE_END(s, result);
}

NAOS_TEST(alloc_failure_on_a_list_item_releases_the_env_copy)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_INCLUDE", "x,y");
    /*
     * config takes its default (no allocation); the env copy of include is
     * point 1, the copy of its first item point 2 (the item buffer would be
     * point 3).
     */
    test_alloc_set_fail_at(test_alloc.calls + 2);

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(slot_of(result, s.include)->items.len, 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(full_resolve_leaves_no_live_blocks_after_free)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    mock_set(&env, "TOOL_CONFIG", "c");
    mock_set(&env, "TOOL_TOKEN", "t");
    mock_set(&env, "TOOL_TAG", "g");
    mock_set(&env, "BUILD_OUTPUT", "o");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_CHECK_INT_GT(test_alloc.live, 0);

    SAMPLE_END(s, result);
}

/* ---- required checks -------------------------------------------------- */

NAOS_TEST(required_satisfied_by_env)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;
    static const char *const src = "main.c";

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_value(slot_of(result, s.mode), "fast", CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, src), 0);
    mock_init(&env);
    mock_set(&env, "TOOL_TOKEN", "secret");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_NONE);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);

    SAMPLE_END(s, result);
}

NAOS_TEST(missing_option_reports_global_before_command)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->option, s.token);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->command, s.build);
    NAOS_CHECK_NULL(clopts_parser_result_get_error(result)->positional);
    NAOS_CHECK_NULL(clopts_parser_result_get_error(result)->token);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->argv_index, -1);
    NAOS_CHECK_UINT_EQ(clopts_parser_result_get_error(result)->name_len, 0);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->short_name, '\0');
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);

    SAMPLE_END(s, result);
}

NAOS_TEST(missing_option_in_the_selected_command)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_value(slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->option, s.mode);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->command, s.build);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->argv_index, -1);

    SAMPLE_END(s, result);
}

NAOS_TEST(default_source_does_not_satisfy_required)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    /* A DEFAULT source recorded directly (the builder forbids required + default). */
    clopts_parser_slot_set_value(slot_of(result, s.token), "d", CLOPTS_SOURCE_DEFAULT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->option, s.token);

    SAMPLE_END(s, result);
}

NAOS_TEST(missing_required_positional)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_value(slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_value(slot_of(result, s.mode), "m", CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->positional, s.src);
    NAOS_CHECK_NULL(clopts_parser_result_get_error(result)->option);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->command, s.build);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->argv_index, -1);

    SAMPLE_END(s, result);
}

NAOS_TEST(required_variadic_needs_one_token)
{
    sample_t s;
    clopts_result_t *result;
    static const char *const first = "a.txt";

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.copy), 0);
    clopts_parser_slot_set_value(slot_of(result, s.token), "t", CLOPTS_SOURCE_ENV);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->positional, s.files);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->command, s.copy);

    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, first), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(include_command_false_checks_only_the_global_options)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);

    /* Global option missing: reported without a command. */
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, false), -1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->option, s.token);
    NAOS_CHECK_NULL(clopts_parser_result_get_error(result)->command);

    /* Global option satisfied: the command's missing option and positional are ignored. */
    clopts_parser_slot_set_value(slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, false), 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(no_selected_command_checks_only_global_options)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    clopts_parser_slot_set_value(slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_NONE);

    SAMPLE_END(s, result);
}

NAOS_TEST(program_without_subcommands_checks_program_positionals)
{
    clopts_program_t *program;
    clopts_option_t *level;
    clopts_positional_t *input;
    clopts_positional_t *rest;
    clopts_result_t *result;
    static const char *const token = "in.txt";
    int calls = 0;

    test_alloc_begin(0);
    test_decl_reset();
    program = test_decl_begin("filt");
    level = test_decl_option(CLOPTS_OPTION_VALUE, "level", 'n');
    test_decl_check(clopts_option_set_default(level, "1"));
    level = test_decl_global_attach(program, level);
    input = test_decl_positional("input");
    clopts_positional_set_required(input, true);
    input = test_decl_global_attach_positional(program, input);
    rest = test_decl_positional("rest");
    clopts_positional_set_variadic(rest, true);
    rest = test_decl_global_attach_positional(program, rest);
    program = test_decl_finish(program);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    result = clopts_parser_result_new(program);
    NAOS_REQUIRE_NOT_NULL(result);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, program, counting_getenv, &calls, true), CLOPTS_PARSER_RESOLVE_DONE);
    NAOS_CHECK_INT_EQ(calls, 0);
    /* level is the only global option: slot 0. */
    NAOS_CHECK_INT_EQ(slot_of(result, level)->source, CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_STR_EQ(slot_of(result, level)->value, "1");

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, program, true), -1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_get_error(result)->usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_parser_result_get_error(result)->positional, input);
    NAOS_CHECK_NULL(clopts_parser_result_get_error(result)->command);

    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, token), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, program, true), 0);

    clopts_result_destroy(result);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ==== through clopts_parse_with_env (7.5) ============================== */

/*
 * The cases below go through the parse function with the shared fixtures of
 * test_fixtures.c and the borrowing mock environment fixture_env_t. They are
 * ported from the parse-level cases of the first-version resolve suite (same
 * argv), plus first-version store-level cases that the internal-API tests
 * above do not cover, plus the copy-on-borrow check of 8.8. The status is
 * always checked with NAOS_REQUIRE_INT_EQ before values are read.
 */

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Parses the NULL-terminated argv for program with the mock environment env
 * and checks that the result is a parse result (not the sentinel).
 */
static clopts_result_t *parse_env(clopts_program_t *program, char **argv, fixture_env_t *env)
{
    clopts_result_t *result;
    int argc = 0;

    while (argv[argc] != NULL)
    {
        argc++;
    }
    result = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, env);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
    return result;
}

/* Checks the error accessors of a missing-required error besides kind and target. */
static void check_missing_common(const clopts_result_t *result, const clopts_command_t *command)
{
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_command(result), command);
    NAOS_CHECK_NULL(clopts_result_get_error_token(result));
    NAOS_CHECK_INT_EQ(clopts_result_get_error_argv_index(result), -1);
    NAOS_CHECK_UINT_EQ(clopts_result_get_error_name_len(result), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_error_short_name(result), '\0');
}

#define TOOL_BEGIN(f)                                                               \
    do                                                                              \
    {                                                                               \
        fixture_tool_init(&(f));                                                    \
        NAOS_REQUIRE_NOT_NULL((f).program);                                         \
        NAOS_REQUIRE_TRUE((f).ok);                                                  \
    } while (0)

/*
 * Local declarations, assembled with the builder (fictional programs).
 *
 *   need    without subcommands: --key VALUE required (env NEED_KEY),
 *           --items LIST ',' (env NEED_ITEMS), FILES required variadic
 *   order   global --global VALUE required
 *           cmd:   --local VALUE required, ARG required
 */
typedef struct need_decl
{
    clopts_program_t *program;
    clopts_option_t *key;
    clopts_option_t *items;
    clopts_positional_t *files;
} need_decl_t;

static void need_init(need_decl_t *d)
{
    test_decl_reset();
    d->program = test_decl_begin("need");
    d->key = test_decl_option(CLOPTS_OPTION_VALUE, "key", '\0');
    test_decl_check(clopts_option_set_env(d->key, "NEED_KEY"));
    test_decl_check(clopts_option_set_required(d->key, true));
    d->key = test_decl_global_attach(d->program, d->key);
    d->items = test_decl_option(CLOPTS_OPTION_LIST, "items", '\0');
    test_decl_check(clopts_option_set_env(d->items, "NEED_ITEMS"));
    test_decl_check(clopts_option_set_delimiter(d->items, ','));
    d->items = test_decl_global_attach(d->program, d->items);
    d->files = test_decl_positional("FILES");
    clopts_positional_set_required(d->files, true);
    clopts_positional_set_variadic(d->files, true);
    d->files = test_decl_global_attach_positional(d->program, d->files);
    d->program = test_decl_finish(d->program);
    NAOS_REQUIRE_NOT_NULL(d->program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
}

typedef struct order_decl
{
    clopts_program_t *program;
    clopts_option_t *global;
    clopts_command_t *cmd;
    clopts_option_t *local;
    clopts_positional_t *arg;
} order_decl_t;

static void order_init(order_decl_t *d)
{
    test_decl_reset();
    d->program = test_decl_begin("order");
    d->global = test_decl_option(CLOPTS_OPTION_VALUE, "global", '\0');
    test_decl_check(clopts_option_set_required(d->global, true));
    d->global = test_decl_global_attach(d->program, d->global);
    d->cmd = test_decl_command(d->program, "cmd");
    d->local = test_decl_option(CLOPTS_OPTION_VALUE, "local", '\0');
    test_decl_check(clopts_option_set_required(d->local, true));
    d->local = test_decl_attach(d->cmd, d->local);
    d->arg = test_decl_positional("ARG");
    clopts_positional_set_required(d->arg, true);
    d->arg = test_decl_attach_positional(d->cmd, d->arg);
    d->program = test_decl_finish(d->program);
    NAOS_REQUIRE_NOT_NULL(d->program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
}

NAOS_TEST(fixture_env_and_defaults_are_as_designed)
{
    fixture_tool_t t;
    fixture_filt_t f;
    fixture_strict_t s;

    TOOL_BEGIN(t);
    NAOS_CHECK_STR_EQ(clopts_option_get_env(t.config), "TOOL_CONFIG");
    NAOS_CHECK_NULL(clopts_option_get_default(t.config));
    NAOS_CHECK_STR_EQ(clopts_option_get_env(t.tags), "TOOL_TAGS");
    NAOS_CHECK_STR_EQ(clopts_option_get_default(t.tags), "a,b");
    NAOS_CHECK_NULL(clopts_option_get_env(t.output));
    NAOS_CHECK_NULL(clopts_option_get_default(t.output));
    fixture_tool_free(&t);

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_STR_EQ(clopts_option_get_env(f.lines), "FILT_LINES");
    NAOS_CHECK_STR_EQ(clopts_option_get_default(f.lines), "10");
    fixture_filt_free(&f);

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    NAOS_CHECK_NULL(clopts_option_get_env(s.token));
    fixture_strict_free(&s);
}

/* ---- environment and sources (8.1 to 8.4, 8.6, 8.7) ------------------- */

NAOS_TEST(parse_config_from_env_is_env)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, t.config), "/etc/t");
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_config_explicit_wins_over_env)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "--config", "x", "cat", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_EXPLICIT);
    /* An explicit value points into argv, and the environment is not looked up. */
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, t.config), argv[2]);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_config_unset_is_none)
{
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(clopts_result_get_value(result, t.config));
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_config_empty_env_is_env)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "" } };
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_NOT_NULL(clopts_result_get_value(result, t.config));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, t.config), "");
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_list_empty_env_is_one_empty_item)
{
    /* Store-level in the first version (list_empty_env_is_one_empty_item). */
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "" } };
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.tags), CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, t.tags), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, t.tags, 0), "");
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_filt_default_points_into_the_declaration)
{
    char *argv[] = { "filt", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(f.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.lines), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.lines), "10");
    /* The default is not copied: it points into the declaration model. */
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.lines), clopts_option_get_default(f.lines));
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

/* ---- scope of the lookup (8.9, 9.6) ----------------------------------- */

NAOS_TEST(parse_config_from_env_with_no_command)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "-vv", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    /* NO_COMMAND still resolves the global options. */
    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, t.config), "/etc/t");
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, t.verbose), 2);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_build_does_not_query_other_commands_env)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x,y" },
    };
    char *argv[] = { "tool", "build", "src", "-o", "out", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    /* TOOL_TAGS belongs to cat, which was not selected. */
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    NAOS_CHECK_UINT_EQ(clopts_result_get_list_count(result, t.tags), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.tags), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

/* ---- required checks (9.2 to 9.4, 9.6, 9.7) --------------------------- */

NAOS_TEST(parse_required_satisfied_by_env)
{
    static const fixture_env_var_t vars[] = { { "NEED_KEY", "k" } };
    char *argv[] = { "need", "f", NULL };
    need_decl_t d;
    fixture_env_t env;
    clopts_result_t *result;

    need_init(&d);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(d.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, d.key), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, d.key), "k");
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    clopts_result_destroy(result);
    clopts_program_destroy(d.program);
}

NAOS_TEST(parse_required_missing_when_env_unset)
{
    char *argv[] = { "need", "f", NULL };
    need_decl_t d;
    fixture_env_t env;
    clopts_result_t *result;

    need_init(&d);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(d.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), d.key);
    NAOS_CHECK_NULL(clopts_result_get_error_positional(result));
    check_missing_common(result, NULL);
    clopts_result_destroy(result);
    clopts_program_destroy(d.program);
}

NAOS_TEST(parse_required_variadic_needs_one_token)
{
    /* Store-level in the first version (required_satisfied_by_env_and_variadic_needs_one). */
    static const fixture_env_var_t vars[] = { { "NEED_KEY", "k" } };
    char *argv[] = { "need", NULL };
    need_decl_t d;
    fixture_env_t env;
    clopts_result_t *result;

    need_init(&d);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(d.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_positional(result), d.files);
    NAOS_CHECK_NULL(clopts_result_get_error_option(result));
    check_missing_common(result, NULL);
    clopts_result_destroy(result);
    clopts_program_destroy(d.program);
}

NAOS_TEST(parse_build_without_output_is_missing_option)
{
    char *argv[] = { "tool", "build", "x", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), t.output);
    NAOS_CHECK_NULL(clopts_result_get_error_positional(result));
    check_missing_common(result, t.build);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), t.build);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_copy_second_positional_missing)
{
    /* Store-level in the first version (required_second_positional_missing). */
    char *missing[] = { "tool", "copy", "a", NULL };
    char *given[] = { "tool", "copy", "a", "b", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, missing, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_positional(result), t.copy_dst);
    NAOS_CHECK_NULL(clopts_result_get_error_option(result));
    check_missing_common(result, t.copy);
    clopts_result_destroy(result);

    result = parse_env(t.program, given, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 2);
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, 1), given[3]);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_required_order_global_command_positional)
{
    /* Store-level in the first version (required_order_global_command_positional). */
    char *none[] = { "order", "cmd", NULL };
    char *global[] = { "order", "--global", "g", "cmd", NULL };
    char *local[] = { "order", "--global", "g", "cmd", "--local", "l", NULL };
    char *all[] = { "order", "--global", "g", "cmd", "--local", "l", "arg", NULL };
    order_decl_t d;
    fixture_env_t env;
    clopts_result_t *result;

    order_init(&d);
    fixture_env_init(&env, NULL, 0);

    result = parse_env(d.program, none, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), d.global);
    check_missing_common(result, d.cmd);
    clopts_result_destroy(result);

    result = parse_env(d.program, global, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), d.local);
    check_missing_common(result, d.cmd);
    clopts_result_destroy(result);

    result = parse_env(d.program, local, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_positional(result), d.arg);
    NAOS_CHECK_NULL(clopts_result_get_error_option(result));
    check_missing_common(result, d.cmd);
    clopts_result_destroy(result);

    result = parse_env(d.program, all, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, d.local), all[5]);
    clopts_result_destroy(result);
    clopts_program_destroy(d.program);
}

NAOS_TEST(parse_no_command_skips_the_command_required_checks)
{
    /* Store-level in the first version (required_global_only_skips_command). */
    char *missing[] = { "order", NULL };
    char *given[] = { "order", "--global", "g", NULL };
    order_decl_t d;
    fixture_env_t env;
    clopts_result_t *result;

    order_init(&d);
    fixture_env_init(&env, NULL, 0);

    result = parse_env(d.program, missing, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), d.global);
    check_missing_common(result, NULL);
    clopts_result_destroy(result);

    /* --local and ARG of cmd are required, but cmd was not selected. */
    result = parse_env(d.program, given, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, d.global), given[2]);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    clopts_result_destroy(result);
    clopts_program_destroy(d.program);
}

NAOS_TEST(parse_strict_no_command_with_required_global)
{
    char *missing[] = { "strict", NULL };
    char *given[] = { "strict", "--token", "t", NULL };
    fixture_strict_t s;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    fixture_env_init(&env, NULL, 0);

    /* No subcommand: the required global is still checked. */
    result = parse_env(s.program, missing, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), s.token);
    check_missing_common(result, NULL);
    clopts_result_destroy(result);

    /* A second, independent parse of the same program. */
    result = parse_env(s.program, given, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, s.token), given[2]);
    clopts_result_destroy(result);
    fixture_strict_free(&s);
}

/* ---- lookup failure (8.10) -------------------------------------------- */

NAOS_TEST(parse_env_failed_is_out_of_memory_and_freeable)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x" } };
    char *argv[] = { "tool", "cat", "f", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    env.fail_next = true;
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    /* The lookup stops at the first failure: TOOL_TAGS is not looked up. */
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    /* No silent fallback: the values read as unset. */
    NAOS_CHECK_NULL(clopts_result_get_value(result, t.config));
    NAOS_CHECK_UINT_EQ(clopts_result_get_list_count(result, t.tags), 0);
    /*
     * The first version freed the caller's result struct twice. A result is
     * now a handle released once; NULL and the sentinel are no-ops.
     */
    clopts_result_destroy(result);
    clopts_result_destroy(NULL);
    clopts_result_destroy(clopts_parser_result_oom());
    fixture_tool_free(&t);
}

/* ---- borrowed strings are copied (8.8) -------------------------------- */

NAOS_TEST(parse_env_values_survive_clobber_after_parse)
{
    static const fixture_env_var_t tool_vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x,y" },
    };
    static const fixture_env_var_t filt_vars[] = { { "FILT_LINES", "5" } };
    char *tool_argv[] = { "tool", "cat", "f", NULL };
    char *filt_argv[] = { "filt", NULL };
    fixture_tool_t t;
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, tool_vars, ARRAY_LEN(tool_vars));
    result = parse_env(t.program, tool_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    fixture_env_clobber(&env);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, t.config), "/etc/t");
    NAOS_CHECK_TRUE(clopts_result_get_value(result, t.config) != env.buffers[0]);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.tags), CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, t.tags), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, t.tags, 0), "x");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, t.tags, 1), "y");
    clopts_result_destroy(result);
    fixture_tool_free(&t);

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, filt_vars, ARRAY_LEN(filt_vars));
    result = parse_env(f.program, filt_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    fixture_env_clobber(&env);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.lines), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.lines), "5");
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

NAOS_TEST(parse_env_value_is_copied_before_the_next_lookup)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x,y" },
    };
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    /* Every lookup overwrites all buffers: TOOL_TAGS destroys the TOOL_CONFIG loan. */
    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    env.clobber = true;
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_UINT_EQ(env.query_count, 2);
    NAOS_CHECK_STR_EQ(env.queried[0], "TOOL_CONFIG");
    NAOS_CHECK_STR_EQ(env.queried[1], "TOOL_TAGS");
    fixture_env_clobber(&env);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, t.config), "/etc/t");
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, t.tags), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, t.tags, 0), "x");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, t.tags, 1), "y");
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_SUITE(resolve,
    NAOS_ADD_TEST(null_program_does_not_call_getenv),
    NAOS_ADD_TEST(null_getenv_fn_reads_the_process_environment),
    NAOS_ADD_TEST(explicit_wins_over_env_and_default),
    NAOS_ADD_TEST(env_wins_over_default),
    NAOS_ADD_TEST(default_when_env_is_unset),
    NAOS_ADD_TEST(empty_env_string_is_an_env_value),
    NAOS_ADD_TEST(declaration_order_global_then_selected_command),
    NAOS_ADD_TEST(list_default_is_split_by_the_delimiter),
    NAOS_ADD_TEST(list_env_is_split_by_the_delimiter),
    NAOS_ADD_TEST(unselected_command_env_is_not_queried),
    NAOS_ADD_TEST(no_selected_command_queries_only_the_global_options),
    NAOS_ADD_TEST(include_command_false_resolves_only_the_global_options),
    NAOS_ADD_TEST(failed_lookup_returns_error_without_default),
    NAOS_ADD_TEST(failed_lookup_in_the_command_returns_error),
    NAOS_ADD_TEST(invalid_value_leaves_the_slot_unset_and_records_the_first_error),
    NAOS_ADD_TEST(invalid_value_without_the_command_names_no_command),
    NAOS_ADD_TEST(failed_lookup_wins_over_an_invalid_value),
    NAOS_ADD_TEST(parse_reports_an_invalid_value_as_a_usage_error),
    NAOS_ADD_TEST(parse_terminal_option_wins_over_an_invalid_value),
    NAOS_ADD_TEST(borrowed_buffer_overwritten_after_return_keeps_values),
    NAOS_ADD_TEST(alloc_failure_on_the_env_copy_returns_error),
    NAOS_ADD_TEST(alloc_failure_on_a_list_item_releases_the_env_copy),
    NAOS_ADD_TEST(full_resolve_leaves_no_live_blocks_after_free),
    NAOS_ADD_TEST(required_satisfied_by_env),
    NAOS_ADD_TEST(missing_option_reports_global_before_command),
    NAOS_ADD_TEST(missing_option_in_the_selected_command),
    NAOS_ADD_TEST(default_source_does_not_satisfy_required),
    NAOS_ADD_TEST(missing_required_positional),
    NAOS_ADD_TEST(required_variadic_needs_one_token),
    NAOS_ADD_TEST(include_command_false_checks_only_the_global_options),
    NAOS_ADD_TEST(no_selected_command_checks_only_global_options),
    NAOS_ADD_TEST(program_without_subcommands_checks_program_positionals),
    NAOS_ADD_TEST(fixture_env_and_defaults_are_as_designed),
    NAOS_ADD_TEST(parse_config_from_env_is_env),
    NAOS_ADD_TEST(parse_config_explicit_wins_over_env),
    NAOS_ADD_TEST(parse_config_unset_is_none),
    NAOS_ADD_TEST(parse_config_empty_env_is_env),
    NAOS_ADD_TEST(parse_list_empty_env_is_one_empty_item),
    NAOS_ADD_TEST(parse_filt_default_points_into_the_declaration),
    NAOS_ADD_TEST(parse_config_from_env_with_no_command),
    NAOS_ADD_TEST(parse_build_does_not_query_other_commands_env),
    NAOS_ADD_TEST(parse_required_satisfied_by_env),
    NAOS_ADD_TEST(parse_required_missing_when_env_unset),
    NAOS_ADD_TEST(parse_required_variadic_needs_one_token),
    NAOS_ADD_TEST(parse_build_without_output_is_missing_option),
    NAOS_ADD_TEST(parse_copy_second_positional_missing),
    NAOS_ADD_TEST(parse_required_order_global_command_positional),
    NAOS_ADD_TEST(parse_no_command_skips_the_command_required_checks),
    NAOS_ADD_TEST(parse_strict_no_command_with_required_global),
    NAOS_ADD_TEST(parse_env_failed_is_out_of_memory_and_freeable),
    NAOS_ADD_TEST(parse_env_values_survive_clobber_after_parse),
    NAOS_ADD_TEST(parse_env_value_is_copied_before_the_next_lookup)
)
