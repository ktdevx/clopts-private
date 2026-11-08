/*
 * Suite resolve: environment variables, defaults and required checks.
 *
 * Driven through the internal API: a program is built with the builder, a
 * result is created with clopts_parser_result_new(), a command is selected
 * with clopts_parser_result_select(), explicit values and positionals are
 * recorded with the slot functions, then clopts_parser_resolve() and
 * clopts_parser_check_required() are called. The environment comes from a
 * local mock that lends strings from its own buffer.
 */

#include "parser/parser_internal.h"

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
 *   root:  --config/-c (VALUE, env TOOL_CONFIG, default "def.conf"),
 *          --include/-I (LIST ',', env TOOL_INCLUDE, default "a,b"),
 *          --name (VALUE), --token (VALUE, required, env TOOL_TOKEN),
 *          --tag (LIST without delimiter, env TOOL_TAG), --verbose/-v (COUNT)
 *   build: --output/-o (VALUE, env BUILD_OUTPUT, default "out"),
 *          --mode (VALUE, required); positional src (required)
 *   copy:  --dest (VALUE, env COPY_DEST); positional files (required, variadic)
 */
typedef struct sample
{
    clopts_parser_probe_t probe;
    clopts_program_t *program;
    clopts_command_t *root;
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

static void sample_init(sample_t *s)
{
    s->probe.fail_at = 0;
    s->probe.calls = 0;
    s->probe.live = 0;
    s->program = clopts_program_new("tool");
    if (s->program == NULL)
    {
        return;
    }
    clopts_parser_set_probe(s->program, &s->probe);
    s->root = clopts_program_root(s->program);
    s->config = clopts_command_add_value(s->root, "config", 'c');
    clopts_option_set_env(s->config, "TOOL_CONFIG");
    clopts_option_set_default(s->config, "def.conf");
    s->include = clopts_command_add_list(s->root, "include", 'I');
    clopts_option_set_delimiter(s->include, ',');
    clopts_option_set_env(s->include, "TOOL_INCLUDE");
    clopts_option_set_default(s->include, "a,b");
    s->name = clopts_command_add_value(s->root, "name", '\0');
    s->token = clopts_command_add_value(s->root, "token", '\0');
    clopts_option_set_required(s->token, true);
    clopts_option_set_env(s->token, "TOOL_TOKEN");
    s->tag = clopts_command_add_list(s->root, "tag", '\0');
    clopts_option_set_env(s->tag, "TOOL_TAG");
    s->verbose = clopts_command_add_count(s->root, "verbose", 'v');
    s->build = clopts_program_add_command(s->program, "build");
    s->output = clopts_command_add_value(s->build, "output", 'o');
    clopts_option_set_env(s->output, "BUILD_OUTPUT");
    clopts_option_set_default(s->output, "out");
    s->mode = clopts_command_add_value(s->build, "mode", '\0');
    clopts_option_set_required(s->mode, true);
    s->src = clopts_command_add_positional(s->build, "src");
    clopts_positional_set_required(s->src, true);
    s->copy = clopts_program_add_command(s->program, "copy");
    s->dest = clopts_command_add_value(s->copy, "dest", '\0');
    clopts_option_set_env(s->dest, "COPY_DEST");
    s->files = clopts_command_add_positional(s->copy, "files");
    clopts_positional_set_required(s->files, true);
    clopts_positional_set_variadic(s->files, true);
}

/* Returns the slot of option (in the root or the selected command). */
static clopts_parser_slot_t *slot_of(clopts_result_t *result, const clopts_option_t *option)
{
    clopts_parser_ref_t ref;

    ref.option = option;
    ref.global = (option->scope == &option->program->root);
    ref.index = option->index;
    ref.negated = false;
    return clopts_parser_result_slot(result, &ref);
}

/* Returns item index of the LIST slot. */
static const char *item_of(const clopts_parser_slot_t *slot, size_t index)
{
    return *(char *const *)clopts_core_vec_at(&slot->items, index);
}

#define SAMPLE_BEGIN(s, result)                                                     \
    do                                                                              \
    {                                                                               \
        sample_init(&(s));                                                          \
        NAOS_REQUIRE_NOT_NULL((s).program);                                         \
        NAOS_REQUIRE_INT_EQ((s).program->error.status, CLOPTS_OK);                  \
        (result) = clopts_parser_result_new((s).program);                           \
        NAOS_REQUIRE_NOT_NULL(result);                                              \
    } while (0)

#define SAMPLE_END(s, result)                                                       \
    do                                                                              \
    {                                                                               \
        clopts_result_free(result);                                                 \
        clopts_program_free((s).program);                                           \
        NAOS_CHECK_INT_EQ((s).probe.live, 0);                                       \
    } while (0)

#define SURELY_UNSET_NAME "CLOPTS_TEST_SURELY_UNSET_VARIABLE_6_3"

/* ---- sentinel --------------------------------------------------------- */

NAOS_TEST(null_program_does_not_call_getenv)
{
    clopts_result_t *result;
    int calls = 0;

    result = clopts_parse_with_env(NULL, 0, NULL, counting_getenv, &calls);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(calls, 0);
    clopts_result_free(result);
}

/* ---- default lookup --------------------------------------------------- */

NAOS_TEST(getenv_default_lends_the_getenv_pointer)
{
    const char *value = NULL;
    const char *expected = getenv("PATH");

    if (expected != NULL)
    {
        NAOS_CHECK_INT_EQ(clopts_parser_getenv_default(NULL, "PATH", &value), CLOPTS_ENV_FOUND);
        NAOS_CHECK_PTR_EQ(value, expected);
    }
    else
    {
        NAOS_CHECK_INT_EQ(clopts_parser_getenv_default(NULL, "PATH", &value), CLOPTS_ENV_UNSET);
    }
    NAOS_CHECK_INT_EQ(clopts_parser_getenv_default(NULL, SURELY_UNSET_NAME, &value), CLOPTS_ENV_UNSET);
}

NAOS_TEST(null_getenv_fn_uses_the_default_lookup)
{
    sample_t s;
    clopts_result_t *result;

    sample_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    clopts_option_set_env(s.config, SURELY_UNSET_NAME);
    clopts_option_set_env(s.include, SURELY_UNSET_NAME);
    clopts_option_set_env(s.token, SURELY_UNSET_NAME);
    clopts_option_set_env(s.tag, SURELY_UNSET_NAME);
    NAOS_REQUIRE_INT_EQ(s.program->error.status, CLOPTS_OK);
    result = clopts_parser_result_new(s.program);
    NAOS_REQUIRE_NOT_NULL(result);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, NULL, NULL, true), 0);
    NAOS_CHECK_INT_EQ(slot_of(result, s.config)->source, CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_STR_EQ(slot_of(result, s.config)->value, "def.conf");
    NAOS_CHECK_INT_EQ(slot_of(result, s.token)->source, CLOPTS_SOURCE_NONE);

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
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.config), cli, CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot_of(result, s.include), "x",
                                                       ',', CLOPTS_SOURCE_EXPLICIT), 0);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
    slot = slot_of(result, s.config);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_PTR_EQ(slot->value, s.config->default_value);
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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
    slot = slot_of(result, s.config);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_NOT_NULL(slot->value);
    NAOS_CHECK_STR_EQ(slot->value, "");

    SAMPLE_END(s, result);
}

NAOS_TEST(declaration_order_root_then_selected_command)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "COPY_DEST"), 0);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "BUILD_OUTPUT"), 1);
    NAOS_CHECK_INT_EQ(slot_of(result, s.output)->source, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_STR_EQ(slot_of(result, s.output)->value, "bin");

    SAMPLE_END(s, result);
}

NAOS_TEST(no_selected_command_queries_only_the_root)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "COPY_DEST", "d");
    mock_set(&env, "BUILD_OUTPUT", "bin");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
    NAOS_CHECK_UINT_EQ(env.queried_count, 4);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "COPY_DEST"), 0);
    NAOS_CHECK_UINT_EQ(mock_queries(&env, "BUILD_OUTPUT"), 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(include_command_false_resolves_only_the_root)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);
    mock_set(&env, "BUILD_OUTPUT", "bin");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, false), 0);
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

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), -1);
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

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), -1);
    NAOS_CHECK_INT_EQ(slot_of(result, s.include)->source, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_INT_EQ(slot_of(result, s.output)->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_NULL(slot_of(result, s.output)->value);

    SAMPLE_END(s, result);
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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
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

NAOS_TEST(probe_failure_on_the_env_copy_returns_error)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_CONFIG", "env.conf");
    s.probe.fail_at = s.probe.calls + 1;

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), -1);
    NAOS_CHECK_UINT_EQ(env.queried_count, 1);
    NAOS_CHECK_INT_EQ(slot_of(result, s.config)->source, CLOPTS_SOURCE_NONE);
    s.probe.fail_at = 0;

    SAMPLE_END(s, result);
}

NAOS_TEST(probe_failure_on_a_list_item_releases_the_env_copy)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    mock_init(&env);
    mock_set(&env, "TOOL_INCLUDE", "x,y");
    /* config takes its default (no allocation); the env copy is point 1, the first item point 2. */
    s.probe.fail_at = s.probe.calls + 2;

    NAOS_CHECK_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), -1);
    s.probe.fail_at = 0;

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

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
    NAOS_CHECK_TRUE(s.probe.live > 0);

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
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.mode), "fast", CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, src), 0);
    mock_init(&env);
    mock_set(&env, "TOOL_TOKEN", "secret");

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), 0);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_NONE);
    NAOS_CHECK_INT_EQ(result->status, CLOPTS_OK);

    SAMPLE_END(s, result);
}

NAOS_TEST(missing_option_reports_root_before_command)
{
    sample_t s;
    clopts_result_t *result;
    mock_env_t env;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    mock_init(&env);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, s.program, mock_getenv, &env, true), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result->error.option, s.token);
    NAOS_CHECK_PTR_EQ(result->error.command, s.build);
    NAOS_CHECK_NULL(result->error.positional);
    NAOS_CHECK_NULL(result->error.token);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);
    NAOS_CHECK_UINT_EQ(result->error.name_len, 0);
    NAOS_CHECK_INT_EQ(result->error.short_name, '\0');
    NAOS_CHECK_INT_EQ(result->status, CLOPTS_OK);

    SAMPLE_END(s, result);
}

NAOS_TEST(missing_option_in_the_selected_command)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result->error.option, s.mode);
    NAOS_CHECK_PTR_EQ(result->error.command, s.build);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);

    SAMPLE_END(s, result);
}

NAOS_TEST(default_source_does_not_satisfy_required)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    /* A DEFAULT source recorded directly (the builder forbids required + default). */
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.token), "d", CLOPTS_SOURCE_DEFAULT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result->error.option, s.token);

    SAMPLE_END(s, result);
}

NAOS_TEST(missing_required_positional)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.mode), "m", CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result->error.positional, s.src);
    NAOS_CHECK_NULL(result->error.option);
    NAOS_CHECK_PTR_EQ(result->error.command, s.build);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);

    SAMPLE_END(s, result);
}

NAOS_TEST(required_variadic_needs_one_token)
{
    sample_t s;
    clopts_result_t *result;
    static const char *const first = "a.txt";

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.copy), 0);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.token), "t", CLOPTS_SOURCE_ENV);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), -1);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result->error.positional, s.files);
    NAOS_CHECK_PTR_EQ(result->error.command, s.copy);

    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, first), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(include_command_false_checks_only_the_root)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);

    /* Root missing: reported without a command. */
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, false), -1);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(result->error.option, s.token);
    NAOS_CHECK_NULL(result->error.command);

    /* Root satisfied: the command's missing option and positional are ignored. */
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, false), 0);

    SAMPLE_END(s, result);
}

NAOS_TEST(no_selected_command_checks_only_root_options)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.token), "t", CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, s.program, true), 0);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_NONE);

    SAMPLE_END(s, result);
}

NAOS_TEST(program_without_subcommands_checks_root_positionals)
{
    clopts_parser_probe_t probe = { 0, 0, 0 };
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_option_t *level;
    clopts_positional_t *input;
    clopts_positional_t *rest;
    clopts_result_t *result;
    static const char *const token = "in.txt";
    int calls = 0;

    program = clopts_program_new("filt");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_parser_set_probe(program, &probe);
    root = clopts_program_root(program);
    level = clopts_command_add_value(root, "level", 'n');
    clopts_option_set_default(level, "1");
    input = clopts_command_add_positional(root, "input");
    clopts_positional_set_required(input, true);
    rest = clopts_command_add_positional(root, "rest");
    clopts_positional_set_variadic(rest, true);
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);
    result = clopts_parser_result_new(program);
    NAOS_REQUIRE_NOT_NULL(result);

    NAOS_REQUIRE_INT_EQ(clopts_parser_resolve(result, program, counting_getenv, &calls, true), 0);
    NAOS_CHECK_INT_EQ(calls, 0);
    NAOS_CHECK_INT_EQ(result->global_slots[level->index].source, CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_STR_EQ(result->global_slots[level->index].value, "1");

    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, program, true), -1);
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(result->error.positional, input);
    NAOS_CHECK_NULL(result->error.command);

    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, token), 0);
    NAOS_CHECK_INT_EQ(clopts_parser_check_required(result, program, true), 0);

    clopts_result_free(result);
    clopts_program_free(program);
    NAOS_CHECK_INT_EQ(probe.live, 0);
}

NAOS_SUITE(resolve,
    NAOS_ADD_TEST(null_program_does_not_call_getenv),
    NAOS_ADD_TEST(getenv_default_lends_the_getenv_pointer),
    NAOS_ADD_TEST(null_getenv_fn_uses_the_default_lookup),
    NAOS_ADD_TEST(explicit_wins_over_env_and_default),
    NAOS_ADD_TEST(env_wins_over_default),
    NAOS_ADD_TEST(default_when_env_is_unset),
    NAOS_ADD_TEST(empty_env_string_is_an_env_value),
    NAOS_ADD_TEST(declaration_order_root_then_selected_command),
    NAOS_ADD_TEST(list_default_is_split_by_the_delimiter),
    NAOS_ADD_TEST(list_env_is_split_by_the_delimiter),
    NAOS_ADD_TEST(unselected_command_env_is_not_queried),
    NAOS_ADD_TEST(no_selected_command_queries_only_the_root),
    NAOS_ADD_TEST(include_command_false_resolves_only_the_root),
    NAOS_ADD_TEST(failed_lookup_returns_error_without_default),
    NAOS_ADD_TEST(failed_lookup_in_the_command_returns_error),
    NAOS_ADD_TEST(borrowed_buffer_overwritten_after_return_keeps_values),
    NAOS_ADD_TEST(probe_failure_on_the_env_copy_returns_error),
    NAOS_ADD_TEST(probe_failure_on_a_list_item_releases_the_env_copy),
    NAOS_ADD_TEST(full_resolve_leaves_no_live_blocks_after_free),
    NAOS_ADD_TEST(required_satisfied_by_env),
    NAOS_ADD_TEST(missing_option_reports_root_before_command),
    NAOS_ADD_TEST(missing_option_in_the_selected_command),
    NAOS_ADD_TEST(default_source_does_not_satisfy_required),
    NAOS_ADD_TEST(missing_required_positional),
    NAOS_ADD_TEST(required_variadic_needs_one_token),
    NAOS_ADD_TEST(include_command_false_checks_only_the_root),
    NAOS_ADD_TEST(no_selected_command_checks_only_root_options),
    NAOS_ADD_TEST(program_without_subcommands_checks_root_positionals)
)
