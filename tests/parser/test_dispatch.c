/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite dispatch: the parse entry (sentinel, read-only program, out of
 * memory), subcommand selection, NO_COMMAND, the declared help command and
 * the terminal options (-h/--help, --version): where they stop the scan, the
 * recovery of a usage error by a later terminal option, and the values that
 * stay readable in TERMINATED. A built program is always valid, so a parse
 * never reports a declaration error. The parser knows no built-in option or
 * command: everything here comes from the declarations of the fixtures.
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

/* Checks that the error fields of result are still empty (TERMINATED). */
static void check_error_untouched(const clopts_result_t *result)
{
    NAOS_CHECK_INT_EQ(result->error.usage, CLOPTS_USAGE_NONE);
    NAOS_CHECK_NULL(result->error.command);
    NAOS_CHECK_NULL(result->error.option);
    NAOS_CHECK_NULL(result->error.positional);
    NAOS_CHECK_NULL(result->error.token);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);
}

/* Checks that the FLAG option was not given (false, no source). */
#define CHECK_FALSE_FLAG(result, option)                                            \
    do                                                                              \
    {                                                                               \
        NAOS_CHECK_FALSE(clopts_result_flag((result), (option)));                   \
        NAOS_CHECK_INT_EQ(clopts_result_source((result), (option)), CLOPTS_SOURCE_NONE); \
    } while (0)

/*
 * Checks a TERMINATED result: terminal is the option that ended the parse,
 * command the selected command (NULL for none), and the error fields are
 * still empty (also when the terminal option replaced a usage error).
 */
static void check_terminated(const clopts_result_t *result, const clopts_option_t *terminal,
                             const clopts_command_t *command)
{
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_terminal(result), terminal);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), command);
    /* The terminal FLAG itself is recorded as given. */
    NAOS_CHECK_TRUE(clopts_result_flag(result, terminal));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, terminal), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_NONE);
    check_error_untouched(result);
}

/* ---- fixtures ---------------------------------------------------------- */

NAOS_TEST(fixture_commands_are_shaped_as_designed)
{
    fixture_tool_t t;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    NAOS_CHECK_PTR_EQ(t.program->root, t.root);
    NAOS_REQUIRE_UINT_EQ(t.program->command_count, 4);
    NAOS_CHECK_PTR_EQ(t.program->commands[0], t.build);
    NAOS_CHECK_PTR_EQ(t.program->commands[1], t.copy);
    NAOS_CHECK_PTR_EQ(t.program->commands[2], t.cat);
    NAOS_CHECK_PTR_EQ(t.program->commands[3], t.help_command);

    /* Root: the global options, then the terminal options of clopts/help.h. */
    NAOS_REQUIRE_UINT_EQ(t.root->options.len, 5);
    NAOS_CHECK_PTR_EQ(option_at(t.root, 0), t.verbose);
    NAOS_CHECK_PTR_EQ(option_at(t.root, 1), t.color);
    NAOS_CHECK_PTR_EQ(option_at(t.root, 2), t.config);
    NAOS_CHECK_PTR_EQ(option_at(t.root, 3), t.help);
    NAOS_CHECK_PTR_EQ(option_at(t.root, 4), t.version);
    NAOS_CHECK_TRUE(t.help->terminal);
    NAOS_CHECK_TRUE(t.version->terminal);
    NAOS_CHECK_FALSE(t.verbose->terminal);
    NAOS_CHECK_UINT_EQ(t.help->index, 3);
    NAOS_CHECK_UINT_EQ(t.version->index, 4);

    NAOS_CHECK_STR_EQ(t.build->name, "build");
    NAOS_REQUIRE_UINT_EQ(t.build->options.len, 3);
    NAOS_CHECK_PTR_EQ(option_at(t.build, 0), t.output);
    NAOS_CHECK_PTR_EQ(option_at(t.build, 1), t.keys);
    NAOS_CHECK_PTR_EQ(option_at(t.build, 2), t.jobs);
    NAOS_CHECK_UINT_EQ(t.output->index, 0);
    NAOS_CHECK_UINT_EQ(t.jobs->index, 2);
    NAOS_REQUIRE_UINT_EQ(t.build->positionals.len, 2);
    NAOS_CHECK_PTR_EQ(positional_at(t.build, 0), t.build_src);
    NAOS_CHECK_PTR_EQ(positional_at(t.build, 1), t.build_extra);

    NAOS_CHECK_STR_EQ(t.copy->name, "copy");
    NAOS_CHECK_UINT_EQ(t.copy->options.len, 0);
    NAOS_CHECK_UINT_EQ(t.copy->positionals.len, 2);

    NAOS_CHECK_STR_EQ(t.cat->name, "cat");
    NAOS_REQUIRE_UINT_EQ(t.cat->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(t.cat, 0), t.tags);
    NAOS_CHECK_UINT_EQ(t.cat->positionals.len, 1);

    /* help [COMMAND]: no options, one optional positional. */
    NAOS_CHECK_STR_EQ(t.help_command->name, "help");
    NAOS_CHECK_UINT_EQ(t.help_command->options.len, 0);
    NAOS_REQUIRE_UINT_EQ(t.help_command->positionals.len, 1);
    NAOS_CHECK_FALSE(positional_at(t.help_command, 0)->required);
    NAOS_CHECK_FALSE(positional_at(t.help_command, 0)->variadic);
    fixture_tool_free(&t);
}

NAOS_TEST(fixture_filt_has_no_commands_and_a_help_option)
{
    fixture_filt_t f;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    NAOS_CHECK_UINT_EQ(f.program->command_count, 0);
    NAOS_CHECK_NULL(f.program->commands);
    NAOS_REQUIRE_UINT_EQ(f.root->options.len, 2);
    NAOS_CHECK_PTR_EQ(option_at(f.root, 0), f.lines);
    NAOS_CHECK_PTR_EQ(option_at(f.root, 1), f.help);
    NAOS_CHECK_TRUE(f.help->terminal);
    NAOS_REQUIRE_UINT_EQ(f.root->positionals.len, 1);
    NAOS_CHECK_PTR_EQ(positional_at(f.root, 0), f.file);
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
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    clopts_result_free(result);
}

/*
 * The program is read-only (there is no freezing anymore): a parse changes
 * nothing of the declaration, and the result does not point to the program.
 */
NAOS_TEST(parse_leaves_the_program_unchanged)
{
    char *argv[] = { "tool", "-v", "build", "-o", "x", "src", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 6, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(t.program->root, t.root);
    NAOS_CHECK_UINT_EQ(t.program->command_count, 4);
    NAOS_CHECK_UINT_EQ(t.root->options.len, 5);
    NAOS_CHECK_UINT_EQ(t.build->options.len, 3);
    NAOS_CHECK_UINT_EQ(t.output->index, 0);
    NAOS_CHECK_NULL(t.output->default_value);
    NAOS_CHECK_PTR_EQ(result->command, t.build);
    NAOS_CHECK_NULL(result->terminal);
    clopts_result_free(result);
    fixture_tool_free(&t);
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

/* ---- help command: an ordinary declared command ------------------------ */

/*
 * "help" is not special before a subcommand: the tool fixture declares the
 * help command of clopts_help_command_create(), and "tool help" selects it.
 */
NAOS_TEST(tool_help_command_alone_selects_the_help_command)
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
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.help_command);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    check_error_untouched(result);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* The name after help is the optional COMMAND positional of the help command. */
NAOS_TEST(tool_help_with_command_takes_it_as_positional)
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
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.help_command);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[2]);
    /* copy is not selected: its required positionals are not checked. */
    check_error_untouched(result);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/*
 * The parser does not check the name given to the help command: an unknown
 * name is resolved later by clopts_help_target() of the help module.
 */
NAOS_TEST(tool_help_with_unknown_name_is_ok_at_parse_level)
{
    char *argv[] = { "tool", "help", "bogus", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.help_command);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[2]);
    check_error_untouched(result);
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
    check_token_error(result, CLOPTS_USAGE_UNEXPECTED_ARGUMENT, t.help_command, argv, 3);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), t.help_command);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* A terminal option after the help command is looked up in the root as usual. */
NAOS_TEST(tool_help_command_then_long_help_is_terminated)
{
    char *argv[] = { "tool", "help", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    check_terminated(result, t.help, t.help_command);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* ---- terminal options before a subcommand ------------------------------ */

NAOS_TEST(tool_long_help_is_terminated)
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
    check_terminated(result, t.help, NULL);
    NAOS_CHECK_PTR_EQ(result->terminal, t.help);
    /* The global scope is resolved (TOOL_CONFIG), no command scope. */
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    CHECK_FALSE_FLAG(result, t.version);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_short_help_is_terminated)
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
    check_terminated(result, t.help, NULL);
    NAOS_CHECK_UINT_EQ(env.query_count, 1);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* A terminal FLAG inside a short group ends the group and the scan. */
NAOS_TEST(tool_short_help_in_a_group_is_terminated)
{
    char *argv[] = { "tool", "-vhv", "build", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    check_terminated(result, t.help, NULL);
    /* Only the -v before -h was recorded; build was not read. */
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, t.verbose), 1);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_long_version_is_terminated)
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
    check_terminated(result, t.version, NULL);
    CHECK_FALSE_FLAG(result, t.help);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_long_version_ignores_following_tokens)
{
    char *argv[] = { "tool", "--version", "build", "--bogus", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 4, argv, &env);
    /* build is not looked at, so it is not selected; --bogus is not an error. */
    check_terminated(result, t.version, NULL);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* The first terminal option wins: the scan stops before the second one. */
NAOS_TEST(tool_first_terminal_option_wins)
{
    char *argv[] = { "tool", "--version", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    check_terminated(result, t.version, NULL);
    CHECK_FALSE_FLAG(result, t.help);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* A value given to a terminal FLAG is an error on that FLAG, and no recovery takes "--help=x". */
NAOS_TEST(tool_long_help_with_value_is_unexpected_value)
{
    char *argv[] = { "tool", "--help=x", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 2, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_UNEXPECTED_VALUE);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), t.help);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[1]);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), 1);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    NAOS_CHECK_UINT_EQ(env.query_count, 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* A terminal option does not check the required global options. */
NAOS_TEST(strict_long_help_skips_the_required_global_option)
{
    char *argv[] = { "strict", "--help", NULL };
    fixture_strict_t s;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(s.program, 2, argv, &env);
    check_terminated(result, s.help, NULL);
    NAOS_CHECK_NULL(clopts_result_value(result, s.token));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.token), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);
    fixture_strict_free(&s);
}

NAOS_TEST(strict_run_long_help_skips_the_required_global_option)
{
    char *argv[] = { "strict", "run", "-h", NULL };
    fixture_strict_t s;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(s.program, 3, argv, &env);
    check_terminated(result, s.help, s.run);
    clopts_result_free(result);
    fixture_strict_free(&s);
}

/* ---- terminal options after a subcommand -------------------------------- */

/* The root's --help is in scope after build; build's required ones are not checked. */
NAOS_TEST(tool_build_long_help_is_terminated_with_build)
{
    char *argv[] = { "tool", "build", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    check_terminated(result, t.help, t.build);
    NAOS_CHECK_NULL(clopts_result_value(result, t.output));
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_copy_version_is_terminated_with_copy)
{
    char *argv[] = { "tool", "copy", "a", "--version", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 4, argv, &env);
    check_terminated(result, t.version, t.copy);
    /* DST is missing, but required positionals are not checked either. */
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[2]);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* The tokens after the terminal option are not read: --bogus is no error. */
NAOS_TEST(tool_build_help_then_unknown_option_is_terminated)
{
    char *argv[] = { "tool", "build", "--help", "--bogus", "src", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 5, argv, &env);
    check_terminated(result, t.help, t.build);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* A token taken as an option value is not a terminal option. */
NAOS_TEST(tool_build_help_as_option_value_is_not_terminal)
{
    char *argv[] = { "tool", "build", "-o", "--help", "src", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 5, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, t.output), argv[3]);
    CHECK_FALSE_FLAG(result, t.help);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[4]);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/*
 * In TERMINATED the values recorded before the terminal option are readable,
 * global and command ones, and the environment and defaults are resolved;
 * what follows it is not read.
 */
NAOS_TEST(tool_terminated_keeps_the_values_given_before_it)
{
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x" },
    };
    char *argv[] = { "tool", "-v", "--no-color", "build", "-o", "out", "-k", "a,b", "-j", "s1", "--help",
                     "-v", "s2", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = clopts_parse_with_env(t.program, 13, argv, fixture_env_getenv, &env);
    check_terminated(result, t.help, t.build);

    NAOS_CHECK_UINT_EQ(clopts_result_count(result, t.verbose), 1);
    NAOS_CHECK_FALSE(clopts_result_flag(result, t.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, t.color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, t.config), "/etc/t");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, t.config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, t.output), argv[5]);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, t.keys), 2);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, t.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, t.keys, 1), "b");
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, t.jobs), 1);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[9]);
    CHECK_FALSE_FLAG(result, t.version);

    /* cat was not selected: its options read as unset and TOOL_TAGS was not looked up. */
    NAOS_CHECK_UINT_EQ(clopts_result_list_count(result, t.tags), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, t.tags), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_TAGS"));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* Without subcommands: the root positionals before -h are kept, the default is resolved. */
NAOS_TEST(filt_short_help_keeps_positionals_before_it_and_the_default)
{
    char *argv[] = { "filt", "a", "-h", "b", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(f.program, 4, argv, &env);
    check_terminated(result, f.help, NULL);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[1]);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.lines), "10");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.lines), CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    clopts_result_free(result);
    fixture_filt_free(&f);
}

/* ---- recovery: a terminal option after a usage error ------------------ */

NAOS_TEST(tool_build_unknown_option_then_help_is_terminated)
{
    char *argv[] = { "tool", "build", "--bogus", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 4, argv, &env);
    /* The usage error is dropped: no error field is set. */
    check_terminated(result, t.help, t.build);
    /* Resolution runs as for any TERMINATED. */
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* Before a subcommand: an unknown command is recovered by a later root terminal option. */
NAOS_TEST(tool_unknown_command_then_version_is_terminated)
{
    char *argv[] = { "tool", "bogus", "--version", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 3, argv, &env);
    check_terminated(result, t.version, NULL);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/*
 * After the error nothing is interpreted: "-h" counts even where it would
 * have been the value of -o, and a short group is never searched.
 */
NAOS_TEST(tool_recovery_takes_an_exact_token_even_after_a_value_option)
{
    char *argv[] = { "tool", "build", "--bad", "-vh", "-o", "-h", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 6, argv, &env);
    check_terminated(result, t.help, t.build);
    /* Recovery records the terminal FLAG only: -v of the group and -o are not recorded. */
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, t.verbose), 0);
    NAOS_CHECK_NULL(clopts_result_value(result, t.output));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* Recovery does not take "--help=x", a short group or a negation as a terminal option. */
NAOS_TEST(tool_recovery_ignores_inexact_forms)
{
    char *argv[] = { "tool", "build", "--bogus", "--help=x", "-vh", "--no-help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 6, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[2]);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), 2);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 7);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), t.build);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* Recovery stops at "--": the terminal option after it does not count. */
NAOS_TEST(tool_recovery_stops_at_dashdash)
{
    char *argv[] = { "tool", "build", "--bogus", "--", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 5, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[2]);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), 2);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* "--" before the error: no recovery at all, "--help" is an unknown command. */
NAOS_TEST(tool_dashdash_then_help_is_unknown_command)
{
    char *argv[] = { "tool", "--", "--help", NULL };
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
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

/* After "--" a "--help" is a positional; build's required option is then missing. */
NAOS_TEST(tool_build_dashdash_help_is_a_positional)
{
    char *argv[] = { "tool", "build", "--", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 4, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), t.output);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), t.build);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), -1);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_TEST(tool_build_dashdash_help_with_output_is_ok)
{
    char *argv[] = { "tool", "build", "-o", "x", "--", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_checked(t.program, 6, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    CHECK_FALSE_FLAG(result, t.help);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, 0), argv[5]);
    clopts_result_free(result);
    fixture_tool_free(&t);
}

NAOS_SUITE(dispatch,
    NAOS_ADD_TEST(fixture_commands_are_shaped_as_designed),
    NAOS_ADD_TEST(fixture_filt_has_no_commands_and_a_help_option),
    NAOS_ADD_TEST(parse_null_program_returns_sentinel),
    NAOS_ADD_TEST(parse_leaves_the_program_unchanged),
    NAOS_ADD_TEST(parse_with_env_null_lookup_uses_the_default),
    NAOS_ADD_TEST(tool_without_arguments_is_no_command),
    NAOS_ADD_TEST(tool_with_argc_zero_is_no_command),
    NAOS_ADD_TEST(no_command_resolves_global_scope_only),
    NAOS_ADD_TEST(no_command_checks_required_global_options),
    NAOS_ADD_TEST(filt_without_arguments_is_ok_with_default),
    NAOS_ADD_TEST(filt_with_argc_zero_is_ok_with_default),
    NAOS_ADD_TEST(filt_takes_value_from_env),
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
    NAOS_ADD_TEST(tool_help_command_alone_selects_the_help_command),
    NAOS_ADD_TEST(tool_help_with_command_takes_it_as_positional),
    NAOS_ADD_TEST(tool_help_with_unknown_name_is_ok_at_parse_level),
    NAOS_ADD_TEST(tool_help_with_command_and_extra_is_unexpected_argument),
    NAOS_ADD_TEST(tool_help_command_then_long_help_is_terminated),
    NAOS_ADD_TEST(tool_long_help_is_terminated),
    NAOS_ADD_TEST(tool_short_help_is_terminated),
    NAOS_ADD_TEST(tool_short_help_in_a_group_is_terminated),
    NAOS_ADD_TEST(tool_long_version_is_terminated),
    NAOS_ADD_TEST(tool_long_version_ignores_following_tokens),
    NAOS_ADD_TEST(tool_first_terminal_option_wins),
    NAOS_ADD_TEST(tool_long_help_with_value_is_unexpected_value),
    NAOS_ADD_TEST(strict_long_help_skips_the_required_global_option),
    NAOS_ADD_TEST(strict_run_long_help_skips_the_required_global_option),
    NAOS_ADD_TEST(tool_build_long_help_is_terminated_with_build),
    NAOS_ADD_TEST(tool_copy_version_is_terminated_with_copy),
    NAOS_ADD_TEST(tool_build_help_then_unknown_option_is_terminated),
    NAOS_ADD_TEST(tool_build_help_as_option_value_is_not_terminal),
    NAOS_ADD_TEST(tool_terminated_keeps_the_values_given_before_it),
    NAOS_ADD_TEST(filt_short_help_keeps_positionals_before_it_and_the_default),
    NAOS_ADD_TEST(tool_build_unknown_option_then_help_is_terminated),
    NAOS_ADD_TEST(tool_unknown_command_then_version_is_terminated),
    NAOS_ADD_TEST(tool_recovery_takes_an_exact_token_even_after_a_value_option),
    NAOS_ADD_TEST(tool_recovery_ignores_inexact_forms),
    NAOS_ADD_TEST(tool_recovery_stops_at_dashdash),
    NAOS_ADD_TEST(tool_dashdash_then_help_is_unknown_command),
    NAOS_ADD_TEST(tool_build_dashdash_help_is_a_positional),
    NAOS_ADD_TEST(tool_build_dashdash_help_with_output_is_ok)
)
