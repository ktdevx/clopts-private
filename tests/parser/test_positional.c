/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite positional: positionals mixed with options, --, a lone -, required /
 * optional / variadic positionals and unexpected arguments.
 *
 * Ported from the first-version positional suite onto the builder fixtures of
 * test_fixtures.c (handles instead of index constants) and the result handle
 * accessors. The status is always checked with NAOS_REQUIRE_INT_EQ before
 * values are read.
 */

#include "parser/parser_internal.h"
#include "test_fixtures.h"

#include <clopts/parse.h>

#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Maximum number of argv elements parse_checked handles. */
#define MAX_ARGV 8

/*
 * Parses the NULL-terminated argv for program with an empty mock environment
 * and checks that the result is a parse result (not the sentinel) and that
 * neither the argv array nor the strings it points to changed.
 */
static clopts_result_t *parse_checked(clopts_program_t *program, char **argv)
{
    char *saved[MAX_ARGV];
    char text[MAX_ARGV][16];
    fixture_env_t env;
    clopts_result_t *result;
    int argc = 0;
    int i;

    while (argv[argc] != NULL)
    {
        NAOS_REQUIRE_TRUE(argc + 1 < MAX_ARGV);
        NAOS_REQUIRE_TRUE(strlen(argv[argc]) < sizeof(text[0]));
        saved[argc] = argv[argc];
        strcpy(text[argc], argv[argc]);
        argc++;
    }
    fixture_env_init(&env, NULL, 0);
    result = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, &env);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved[i]);
        NAOS_CHECK_STR_EQ(argv[i], text[i]);
    }
    NAOS_CHECK_NULL(argv[argc]);
    return result;
}

/* Builds the tool fixture into f and parses argv with it. */
static clopts_result_t *parse_tool(fixture_tool_t *f, char **argv)
{
    fixture_tool_init(f);
    NAOS_REQUIRE_NOT_NULL(f->program);
    NAOS_REQUIRE_TRUE(f->ok);
    return parse_checked(f->program, argv);
}

/*
 * Checks that result is OK with the positionals expected[0..count-1], each
 * being the very argv element (not a copy).
 */
static void check_positionals(const clopts_result_t *result, char *const *expected, size_t count)
{
    size_t i;

    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, i), expected[i]);
    }
}

/* Checks name, required and variadic of one positional. */
static void check_positional(const clopts_positional_t *positional, const char *name,
                             bool required, bool variadic)
{
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_STR_EQ(clopts_positional_name(positional), name);
    NAOS_CHECK_INT_EQ(clopts_positional_required(positional) ? 1 : 0, required ? 1 : 0);
    NAOS_CHECK_INT_EQ(clopts_positional_variadic(positional) ? 1 : 0, variadic ? 1 : 0);
}

/* ---- sentinel ----------------------------------------------------------- */

NAOS_TEST(sentinel_has_no_positionals)
{
    clopts_result_t *result;

    result = clopts_parse(NULL, 0, NULL);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    clopts_result_free(result);
}

/* ---- fixtures ---------------------------------------------------------- */

NAOS_TEST(fixture_positionals_are_as_designed)
{
    fixture_tool_t t;
    fixture_filt_t f;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    check_positional(t.build_src, "SRC", true, false);
    check_positional(t.build_extra, "EXTRA", false, true);
    check_positional(t.copy_src, "SRC", true, false);
    check_positional(t.copy_dst, "DST", true, false);
    check_positional(t.cat_file, "FILE", false, true);
    fixture_tool_free(&t);

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    check_positional(f.file, "FILE", false, true);
    fixture_filt_free(&f);
}

NAOS_TEST(fixture_programs_with_commands_have_no_program_positionals)
{
    fixture_tool_t t;
    fixture_strict_t s;

    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    NAOS_CHECK_UINT_EQ(t.root->positional_count, 0);
    fixture_tool_free(&t);

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    NAOS_CHECK_UINT_EQ(s.root->positional_count, 0);
    fixture_strict_free(&s);
}

/* ---- through the parse function ---------------------------------------- */

NAOS_TEST(copy_takes_two_positionals_in_order)
{
    char *argv[] = { "tool", "copy", "a", "b", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.copy);
    check_positionals(result, &argv[2], 2);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(copy_without_dst_is_missing_argument)
{
    char *argv[] = { "tool", "copy", "a", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_MISSING_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), f.copy);
    NAOS_CHECK_PTR_EQ(clopts_result_error_positional(result), f.copy_dst);
    NAOS_REQUIRE_NOT_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_STR_EQ(clopts_positional_name(clopts_result_error_positional(result)), "DST");
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(copy_with_a_third_token_is_unexpected_argument)
{
    char *argv[] = { "tool", "copy", "a", "b", "c", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_UNEXPECTED_ARGUMENT);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), f.copy);
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), argv[4]);
    NAOS_CHECK_STR_EQ(clopts_result_error_token(result), "c");
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), 4);
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(cat_without_tokens_has_no_positionals)
{
    char *argv[] = { "tool", "cat", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.cat);
    check_positionals(result, NULL, 0);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(cat_variadic_takes_every_remaining_token)
{
    char *argv[] = { "tool", "cat", "x", "y", "z", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    check_positionals(result, &argv[2], 3);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(cat_after_dashdash_dash_tokens_are_positionals)
{
    char *argv[] = { "tool", "cat", "--", "-x", "--y", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    check_positionals(result, &argv[3], 2);
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(cat_lone_dash_is_a_positional)
{
    char *argv[] = { "tool", "cat", "-", NULL };
    fixture_tool_t f;
    clopts_result_t *result;

    result = parse_tool(&f, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    check_positionals(result, &argv[2], 1);
    NAOS_CHECK_STR_EQ(clopts_result_positional_at(result, 0), "-");
    clopts_result_free(result);
    fixture_tool_free(&f);
}

NAOS_TEST(filt_without_subcommands_takes_positionals)
{
    char *argv[] = { "filt", "a", "b", NULL };
    fixture_filt_t f;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    result = parse_checked(f.program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_command(result));
    check_positionals(result, &argv[1], 2);
    clopts_result_free(result);
    fixture_filt_free(&f);
}

NAOS_TEST(strict_without_tokens_checks_the_global_required_option)
{
    char *argv[] = { "strict", NULL };
    fixture_strict_t s;
    clopts_result_t *result;

    fixture_strict_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    result = parse_checked(s.program, argv);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_MISSING_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), s.token);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_NULL(clopts_result_command(result));
    clopts_result_free(result);
    fixture_strict_free(&s);
}

NAOS_SUITE(positional,
    NAOS_ADD_TEST(sentinel_has_no_positionals),
    NAOS_ADD_TEST(fixture_positionals_are_as_designed),
    NAOS_ADD_TEST(fixture_programs_with_commands_have_no_program_positionals),
    NAOS_ADD_TEST(copy_takes_two_positionals_in_order),
    NAOS_ADD_TEST(copy_without_dst_is_missing_argument),
    NAOS_ADD_TEST(copy_with_a_third_token_is_unexpected_argument),
    NAOS_ADD_TEST(cat_without_tokens_has_no_positionals),
    NAOS_ADD_TEST(cat_variadic_takes_every_remaining_token),
    NAOS_ADD_TEST(cat_after_dashdash_dash_tokens_are_positionals),
    NAOS_ADD_TEST(cat_lone_dash_is_a_positional),
    NAOS_ADD_TEST(filt_without_subcommands_takes_positionals),
    NAOS_ADD_TEST(strict_without_tokens_checks_the_global_required_option)
)
