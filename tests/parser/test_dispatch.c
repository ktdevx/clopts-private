/*
 * Suite dispatch: the parse entry (sentinel, freezing, out of memory),
 * subcommand selection, NO_COMMAND and the help / --help / --version forms
 * handled before a subcommand. A built program is always valid, so a parse
 * never reports a declaration error.
 *
 * Ported from the first-version dispatch suite onto the builder fixtures of
 * test_fixtures.c and the result handle accessors. The status is always
 * checked with NAOS_REQUIRE_INT_EQ before values are read.
 */

#include "parser/parser_internal.h"
#include "test_alloc.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Number of elements of a static array. */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Maximum number of argv elements the helpers below handle. */
#define MAX_ARGV 8

/* Returns option index of scope. */
static const clopts_option_t *option_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_option_t *const *)clopts_core_vec_at(&scope->options, index);
}

/* Returns positional index of scope. */
static const clopts_positional_t *positional_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_positional_t *const *)clopts_core_vec_at(&scope->positionals, index);
}

/*
 * Copies of argv[0..argc-1]: the pointers and the strings they point to, so
 * that a parse can be checked not to modify either.
 */
typedef struct saved_argv
{
    int argc;
    char *pointers[MAX_ARGV];
    char strings[MAX_ARGV][32];
} saved_argv_t;

static void save_argv(saved_argv_t *saved, int argc, char **argv)
{
    int i;

    NAOS_REQUIRE_TRUE(argc < MAX_ARGV);
    saved->argc = argc;
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(argv[i]) < sizeof(saved->strings[i]));
        saved->pointers[i] = argv[i];
        strcpy(saved->strings[i], argv[i]);
    }
}

static void check_argv_unchanged(const saved_argv_t *saved, char **argv)
{
    int i;

    for (i = 0; i < saved->argc; i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved->pointers[i]);
        NAOS_CHECK_STR_EQ(argv[i], saved->strings[i]);
    }
    NAOS_CHECK_NULL(argv[saved->argc]);
}

/*
 * Parses argv[0..argc-1] (NULL-terminated) for program with the mock
 * environment env and checks that argv is unchanged afterwards and that the
 * result is a parse result (not the sentinel).
 */
static clopts_result_t *parse_checked(clopts_program_t *program, int argc, char **argv, fixture_env_t *env)
{
    saved_argv_t saved;
    clopts_result_t *result;

    save_argv(&saved, argc, argv);
    result = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, env);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
    check_argv_unchanged(&saved, argv);
    return result;
}

/*
 * Checks a usage error of kind usage on token argv[index] that names no
 * option, positional or short name; command is the expected error command.
 */
static void check_token_error(const clopts_result_t *result, clopts_usage_error_t usage,
                              const clopts_command_t *command, char **argv, int index)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), usage);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), command);
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[index]);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), index);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');
}

/* Checks that the error fields of result are still empty (HELP / VERSION). */
static void check_error_untouched(const clopts_result_t *result)
{
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_NONE);
    NAOS_CHECK_NULL(result->error.command);
    NAOS_CHECK_NULL(result->error.option);
    NAOS_CHECK_NULL(result->error.positional);
    NAOS_CHECK_NULL(result->error.token);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);
}

/* ---- fixtures ---------------------------------------------------------- */

NAOS_TEST(fixture_commands_are_shaped_as_designed)
{
    fixture_tool_t t;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    NAOS_REQUIRE_UINT_EQ(t.program->commands.len, 3);

    NAOS_CHECK_STR_EQ(t.build->name, "build");
    NAOS_REQUIRE_UINT_EQ(t.build->options.len, 3);
    NAOS_CHECK_PTR_EQ(option_at(t.build, 0), t.output);
    NAOS_CHECK_PTR_EQ(option_at(t.build, 1), t.keys);
    NAOS_CHECK_PTR_EQ(option_at(t.build, 2), t.jobs);
    NAOS_REQUIRE_UINT_EQ(t.build->positionals.len, 2);
    NAOS_CHECK_PTR_EQ(positional_at(t.build, 0), t.build_src);
    NAOS_CHECK_PTR_EQ(positional_at(t.build, 1), t.build_extra);
    NAOS_CHECK_TRUE(t.build->handler == fixture_handler);
    NAOS_CHECK_PTR_EQ(t.build->userdata, &fixture_build_handler_record);

    NAOS_CHECK_STR_EQ(t.copy->name, "copy");
    NAOS_CHECK_UINT_EQ(t.copy->options.len, 0);
    NAOS_CHECK_UINT_EQ(t.copy->positionals.len, 2);

    NAOS_CHECK_STR_EQ(t.cat->name, "cat");
    NAOS_REQUIRE_UINT_EQ(t.cat->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(t.cat, 0), t.tags);
    NAOS_CHECK_UINT_EQ(t.cat->positionals.len, 1);
    fixture_tool_free(&t);
}

NAOS_TEST(fixture_filt_has_no_commands_and_a_handler)
{
    fixture_filt_t f;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_UINT_EQ(f.program->commands.len, 0);
    NAOS_REQUIRE_UINT_EQ(f.root->positionals.len, 1);
    NAOS_CHECK_PTR_EQ(positional_at(f.root, 0), f.file);
    NAOS_CHECK_TRUE(f.root->handler == fixture_handler);
    NAOS_CHECK_PTR_EQ(f.root->userdata, &fixture_filt_handler_record);
    fixture_filt_free(&f);
}

/* ---- parse entry -------------------------------------------------------- */

NAOS_TEST(parse_null_program_returns_sentinel)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_PTR_EQ(result, clopts_parser_result_oom());
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
}

NAOS_TEST(parse_freezes_the_program)
{
    char *argv[] = { "filt", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_FALSE(f.program->frozen);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(f.program, 1, argv, &env);
    NAOS_CHECK_TRUE(f.program->frozen);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result->program, f.program);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(parse_with_env_null_lookup_uses_the_default)
{
    char *argv[] = { "filt", "--lines", "3", NULL };
    fixture_filt_t f;
    clopts_result_t *with_env;
    clopts_result_t *plain;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    with_env = clopts_parse_with_env(f.program, 3, argv, NULL, NULL);
    plain = clopts_parse(f.program, 3, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(with_env), CLOPTS_OK);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(plain), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(with_env, f.lines), "3");
    NAOS_CHECK_STR_EQ(clopts_result_value(plain, f.lines), "3");
    NAOS_CHECK_INT_EQ(clopts_result_source(with_env, f.lines), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(with_env);
    clopts_result_free(plain);
    fixture_filt_free(&f);
}

NAOS_TEST(tool_without_arguments_is_no_command)
{
    char *argv[] = { "tool", NULL };
    fixture_tool_t t;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    result = clopts_parse(t.program, 1, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, t.verbose), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, t.verbose), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_FALSE(clopts_result_flag(result, t.color));
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    NAOS_CHECK_STR_EQ(argv[0], "tool");
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_with_argc_zero_is_no_command)
{
    fixture_tool_t t;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    result = clopts_parse(t.program, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(no_command_resolves_global_scope_only)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x" },
    };
    char *argv[] = { "tool", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_checked(t.program, 1, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, t.config), "/etc/t");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, t.config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_UINT_EQ(clopts_result_list_count(result, t.tags), 0);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(no_command_checks_required_global_options)
{
    char *argv[] = { "strict", NULL };
    fixture_strict_t s;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(s.program, 1, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), s.token);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_NULL(clopts_result_error_token(result));
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), -1);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_strict_free(&s);
}

NAOS_TEST(filt_without_arguments_is_ok_with_default)
{
    char *argv[] = { "filt", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(f.program, 1, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "10");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.lines), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(filt_with_argc_zero_is_ok_with_default)
{
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    result = clopts_parse_with_env(f.program, 0, NULL, fixture_env_getenv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "10");
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(filt_takes_value_from_env)
{
    static const fixture_env_var_t vars[] = {
        { "FILT_LINES", "5" },
    };
    char *argv[] = { "filt", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_checked(f.program, 1, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "5");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.lines), CLOPTS_SOURCE_ENV);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(filt_parse_result_runs_its_handler)
{
    char *argv[] = { "filt", "a", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    fixture_handler_reset(&fixture_filt_handler_record, 7);
    result = parse_checked(f.program, 2, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[1]);
    NAOS_CHECK_INT_EQ(clopts_result_run(result), 7);
    NAOS_CHECK_INT_EQ(fixture_filt_handler_record.calls, 1);
    NAOS_CHECK_PTR_EQ(fixture_filt_handler_record.last_result, result);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(env_failure_is_out_of_memory_and_result_is_freeable)
{
    char *argv[] = { "filt", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    test_alloc_begin(0);
    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    env.fail_next = true;
    result = parse_checked(f.program, 1, argv, &env);
    /* A real result, not the sentinel, ends with OUT_OF_MEMORY. */
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_value(result, f.lines));
    clopts_result_free(result);
    fixture_filt_free(&f);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(result_allocation_failure_returns_sentinel)
{
    char *argv[] = { "tool", "build", NULL };
    fixture_tool_t t;
    clopts_result_t *result;

    test_alloc_begin(0);
    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    test_alloc_set_fail_at(test_alloc.calls + 1);
    result = clopts_parse(t.program, 2, argv);
    NAOS_CHECK_PTR_EQ(result, clopts_parser_result_oom());
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(t.program->frozen);
    clopts_result_free(result);
    fixture_tool_free(&t);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(selection_allocation_failure_is_out_of_memory)
{
    char *argv[] = { "tool", "build", "-o", "x", "src", NULL };
    fixture_tool_t t;
    clopts_result_t *result;

    test_alloc_begin(0);
    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    /*
     * Fault points of the parse: the result (1), the root slots (2) and the
     * build slots (3), allocated when build is selected at argv[1], before
     * anything else of the parse allocates. The third one fails.
     */
    test_alloc_set_fail_at(test_alloc.calls + 3);
    result = clopts_parse(t.program, 5, argv);
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(same_program_parsed_twice_gives_independent_results)
{
    static const fixture_env_var_t vars[] = {
        { "FILT_LINES", "7" },
    };
    char *first_argv[] = { "filt", "a", NULL };
    char *second_argv[] = { "filt", "--lines", "2", "b", "c", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *first;
    clopts_result_t *second;

    test_alloc_begin(0);
    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    first = parse_checked(f.program, 2, first_argv, &env);
    second = parse_checked(f.program, 5, second_argv, &env);
    NAOS_CHECK_TRUE(first != second);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(first), CLOPTS_OK);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(second), CLOPTS_OK);

    /* The environment copy of the first parse survives the second one. */
    fixture_env_clobber(&env);
    NAOS_CHECK_STR_EQ(clopts_result_value(first, f.lines), "7");
    NAOS_CHECK_INT_EQ(clopts_result_source(first, f.lines), CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(first), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(first, 0), first_argv[1]);

    NAOS_CHECK_STR_EQ(clopts_result_value(second, f.lines), "2");
    NAOS_CHECK_INT_EQ(clopts_result_source(second, f.lines), CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(second), 2);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(second, 0), second_argv[3]);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(second, 1), second_argv[4]);

    /* Releasing one leaves the other intact. */
    clopts_result_free(first);
    NAOS_CHECK_STR_EQ(clopts_result_value(second, f.lines), "2");
    clopts_result_free(second);
    fixture_filt_free(&f);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- pre-command phase: selection and unknown commands ---------------- */

NAOS_TEST(tool_build_selects_build_and_checks_its_required_options)
{
    char *argv[] = { "tool", "build", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.build);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), t.output);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), t.build);
    NAOS_CHECK_NULL(clopts_result_error_token(result));
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), -1);
    /* The selected scope (global and build) was resolved. */
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_build_with_global_option_before_it_is_ok)
{
    char *argv[] = { "tool", "--verbose", "build", "-o", "y", "src", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 6, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.build);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, t.verbose), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, t.output), argv[4]);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[5]);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_misspelled_command_is_unknown_command)
{
    char *argv[] = { "tool", "bulid", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    check_token_error(result, CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, argv, 1);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_command_name_is_case_sensitive)
{
    char *argv[] = { "tool", "Build", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    check_token_error(result, CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, argv, 1);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_dashdash_then_command_is_unknown_command)
{
    char *argv[] = { "tool", "--", "build", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    check_token_error(result, CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, argv, 2);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_single_dash_is_unknown_command)
{
    char *argv[] = { "tool", "-", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    check_token_error(result, CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, argv, 1);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* ---- pre-command phase: --help / -h / help / --version ---------------- */

NAOS_TEST(tool_long_help_is_program_help)
{
    char *argv[] = { "tool", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_HELP);
    NAOS_CHECK_NULL(clopts_result_command(result));
    check_error_untouched(result);
    /* No resolution and no required check with HELP. */
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_short_help_is_program_help)
{
    char *argv[] = { "tool", "-h", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_HELP);
    NAOS_CHECK_NULL(clopts_result_command(result));
    check_error_untouched(result);
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_help_command_alone_is_program_help)
{
    char *argv[] = { "tool", "help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_HELP);
    NAOS_CHECK_NULL(clopts_result_command(result));
    check_error_untouched(result);
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_help_with_command_is_that_command_help)
{
    char *argv[] = { "tool", "help", "copy", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_HELP);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.copy);
    check_error_untouched(result);
    /* copy has required positionals, but HELP does not check them. */
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_help_with_unknown_command_is_unknown_command)
{
    char *argv[] = { "tool", "help", "nope", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    check_token_error(result, CLOPTS_USAGE_UNKNOWN_COMMAND, NULL, argv, 2);
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_help_with_command_and_extra_is_unexpected_argument)
{
    char *argv[] = { "tool", "help", "copy", "x", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 4, argv, &env);
    check_token_error(result, CLOPTS_USAGE_UNEXPECTED_ARGUMENT, t.copy, argv, 3);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.copy);
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_long_version_is_version)
{
    char *argv[] = { "tool", "--version", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_VERSION);
    NAOS_CHECK_NULL(clopts_result_command(result));
    check_error_untouched(result);
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_long_version_ignores_following_tokens)
{
    char *argv[] = { "tool", "--version", "build", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_VERSION);
    /* build is not looked at, so it is not selected. */
    NAOS_CHECK_NULL(clopts_result_command(result));
    check_error_untouched(result);
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_SUITE(dispatch,
    NAOS_ADD_TEST(fixture_commands_are_shaped_as_designed),
    NAOS_ADD_TEST(fixture_filt_has_no_commands_and_a_handler),
    NAOS_ADD_TEST(parse_null_program_returns_sentinel),
    NAOS_ADD_TEST(parse_freezes_the_program),
    NAOS_ADD_TEST(parse_with_env_null_lookup_uses_the_default),
    NAOS_ADD_TEST(tool_without_arguments_is_no_command),
    NAOS_ADD_TEST(tool_with_argc_zero_is_no_command),
    NAOS_ADD_TEST(no_command_resolves_global_scope_only),
    NAOS_ADD_TEST(no_command_checks_required_global_options),
    NAOS_ADD_TEST(filt_without_arguments_is_ok_with_default),
    NAOS_ADD_TEST(filt_with_argc_zero_is_ok_with_default),
    NAOS_ADD_TEST(filt_takes_value_from_env),
    NAOS_ADD_TEST(filt_parse_result_runs_its_handler),
    NAOS_ADD_TEST(env_failure_is_out_of_memory_and_result_is_freeable),
    NAOS_ADD_TEST(result_allocation_failure_returns_sentinel),
    NAOS_ADD_TEST(selection_allocation_failure_is_out_of_memory),
    NAOS_ADD_TEST(same_program_parsed_twice_gives_independent_results),
    NAOS_ADD_TEST(tool_build_selects_build_and_checks_its_required_options),
    NAOS_ADD_TEST(tool_build_with_global_option_before_it_is_ok),
    NAOS_ADD_TEST(tool_misspelled_command_is_unknown_command),
    NAOS_ADD_TEST(tool_command_name_is_case_sensitive),
    NAOS_ADD_TEST(tool_dashdash_then_command_is_unknown_command),
    NAOS_ADD_TEST(tool_single_dash_is_unknown_command),
    NAOS_ADD_TEST(tool_long_help_is_program_help),
    NAOS_ADD_TEST(tool_short_help_is_program_help),
    NAOS_ADD_TEST(tool_help_command_alone_is_program_help),
    NAOS_ADD_TEST(tool_help_with_command_is_that_command_help),
    NAOS_ADD_TEST(tool_help_with_unknown_command_is_unknown_command),
    NAOS_ADD_TEST(tool_help_with_command_and_extra_is_unexpected_argument),
    NAOS_ADD_TEST(tool_long_version_is_version),
    NAOS_ADD_TEST(tool_long_version_ignores_following_tokens)
)
