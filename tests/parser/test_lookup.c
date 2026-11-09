/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite lookup: the lookups of the scanner (lookup.c). A long or short name
 * is searched in the selected command and then in the program, so a name of
 * the command shadows the same global option; in each scope an exact long
 * name comes before --no-<name> of a negatable FLAG.
 */

#include "parser/parser_internal.h"
#include "test_decl.h"

#include <naos/naos.h>

/* clopts_parser_find_long() in the program and command (NULL: none selected). */
static clopts_parser_ref_t find_long(const clopts_program_t *program, const clopts_command_t *command,
                                     const char *name, size_t name_len)
{
    return clopts_parser_find_long(clopts_program_scope(program),
                                   (command != NULL) ? clopts_command_scope(command) : NULL, name, name_len);
}

/* clopts_parser_find_short() in the program and command (NULL: none selected). */
static clopts_parser_ref_t find_short(const clopts_program_t *program, const clopts_command_t *command, char c)
{
    return clopts_parser_find_short(clopts_program_scope(program),
                                    (command != NULL) ? clopts_command_scope(command) : NULL, c);
}

/*
 * A program with globals verbose/v, color (negatable), quiet/q and no-cache,
 * and commands build (output/o, cache/c negatable, -r negatable, no-color)
 * and copy (output/o).
 */
typedef struct lookup_fixture
{
    clopts_program_t *program;
    clopts_option_t *verbose;
    clopts_option_t *color;
    clopts_option_t *quiet;
    clopts_option_t *no_cache;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *build_output;
    clopts_option_t *build_cache;
    clopts_option_t *build_release;
    clopts_option_t *build_no_color;
    clopts_option_t *copy_output;
} lookup_fixture_t;

/* A new unattached negatable FLAG. */
static clopts_option_t *make_negatable(const char *long_name, char short_name)
{
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, long_name, short_name);

    if (option != NULL)
    {
        test_decl_check(clopts_option_set_negatable(option, true));
    }
    return option;
}

static lookup_fixture_t make_lookup_fixture(void)
{
    lookup_fixture_t f;
    clopts_program_t *program;

    test_decl_reset();
    program = test_decl_begin("tool");
    f.verbose = test_decl_global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    f.color = test_decl_global_attach(program, make_negatable("color", '\0'));
    f.quiet = test_decl_global_add(program, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.no_cache = test_decl_global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    f.build = test_decl_command(program, "build");
    f.copy = test_decl_command(program, "copy");
    f.build_output = test_decl_add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_cache = test_decl_attach(f.build, make_negatable("cache", 'c'));
    f.build_release = test_decl_attach(f.build, make_negatable(NULL, 'r'));
    f.build_no_color = test_decl_add(f.build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    f.copy_output = test_decl_add(f.copy, CLOPTS_OPTION_VALUE, "output", 'o');
    f.program = test_decl_finish(program);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    return f;
}

NAOS_TEST(find_long_searches_command_then_global)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    ref = find_long(f.program, f.build, "output", 6);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_output);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 0);
    NAOS_CHECK_FALSE(ref.negated);

    ref = find_long(f.program, f.copy, "output", 6);
    NAOS_CHECK_PTR_EQ(ref.option, f.copy_output);

    ref = find_long(f.program, f.build, "quiet", 5);
    NAOS_CHECK_PTR_EQ(ref.option, f.quiet);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 2);

    /* without a selected command only the global options are searched */
    ref = find_long(f.program, NULL, "output", 6);
    NAOS_CHECK_NULL(ref.option);
    ref = find_long(f.program, NULL, "verbose", 7);
    NAOS_CHECK_PTR_EQ(ref.option, f.verbose);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 0);

    /* other commands are not searched */
    ref = find_long(f.program, f.copy, "cache", 5);
    NAOS_CHECK_NULL(ref.option);
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_long_is_exact_case_sensitive_and_uses_name_len)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    NAOS_CHECK_NULL(find_long(f.program, f.build, "Output", 6).option);
    NAOS_CHECK_NULL(find_long(f.program, f.build, "out", 3).option);
    NAOS_CHECK_NULL(find_long(f.program, f.build, "outputs", 7).option);
    NAOS_CHECK_NULL(find_long(f.program, f.build, "", 0).option);

    /* name_len bytes only: "output=x" with name_len 6 */
    ref = find_long(f.program, f.build, "output=x", 6);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_output);
    NAOS_CHECK_NULL(find_long(f.program, f.build, "output=x", 5).option);
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_long_negation_only_for_negatable_flags_with_long_name)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    ref = find_long(f.program, NULL, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 1);
    NAOS_CHECK_TRUE(ref.negated);

    ref = find_long(f.program, f.build, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_cache);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 1);
    NAOS_CHECK_TRUE(ref.negated);

    ref = find_long(f.program, f.copy, "no-color=1", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);
    NAOS_CHECK_TRUE(ref.negated);

    /* not negatable, not a FLAG, not selected, malformed */
    NAOS_CHECK_NULL(find_long(f.program, NULL, "no-quiet", 8).option);
    NAOS_CHECK_NULL(find_long(f.program, f.build, "no-output", 9).option);
    NAOS_CHECK_NULL(find_long(f.program, NULL, "no-", 3).option);
    NAOS_CHECK_NULL(find_long(f.program, NULL, "No-color", 8).option);
    NAOS_CHECK_NULL(find_long(f.program, NULL, "no-Color", 8).option);
    NAOS_CHECK_NULL(find_long(f.program, NULL, "no-col", 6).option);
    NAOS_CHECK_NULL(find_long(f.program, NULL, "nocolor", 7).option);
    /* a negatable FLAG without a long name has no negation */
    NAOS_CHECK_NULL(find_long(f.program, f.build, "no-r", 4).option);
    clopts_program_destroy(f.program);
}

/*
 * The scopes are searched one after the other, each one completely (exact
 * name, then negation): a command name, exact or negated, shadows the global one.
 */
NAOS_TEST(find_long_resolves_each_scope_completely)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    /* the exact no-color of build beats the negation of the global color */
    ref = find_long(f.program, f.build, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_no_color);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_FALSE(ref.negated);

    /* the negation of the cache of build beats the exact global no-cache */
    ref = find_long(f.program, f.build, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_cache);
    NAOS_CHECK_TRUE(ref.negated);

    /* without a command, and from another command, the global names */
    ref = find_long(f.program, NULL, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.no_cache);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_FALSE(ref.negated);
    ref = find_long(f.program, f.copy, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.no_cache);
    ref = find_long(f.program, f.copy, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);
    NAOS_CHECK_TRUE(ref.negated);
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_short_searches_command_then_global)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    ref = find_short(f.program, f.build, 'o');
    NAOS_CHECK_PTR_EQ(ref.option, f.build_output);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 0);
    NAOS_CHECK_FALSE(ref.negated);

    ref = find_short(f.program, f.build, 'r');
    NAOS_CHECK_PTR_EQ(ref.option, f.build_release);
    NAOS_CHECK_UINT_EQ(ref.index, 2);

    ref = find_short(f.program, f.copy, 'v');
    NAOS_CHECK_PTR_EQ(ref.option, f.verbose);
    NAOS_CHECK_TRUE(ref.global);

    ref = find_short(f.program, NULL, 'q');
    NAOS_CHECK_PTR_EQ(ref.option, f.quiet);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 2);

    NAOS_CHECK_NULL(find_short(f.program, NULL, 'o').option);
    NAOS_CHECK_NULL(find_short(f.program, f.copy, 'c').option);
    NAOS_CHECK_NULL(find_short(f.program, f.build, 'V').option);
    NAOS_CHECK_NULL(find_short(f.program, f.build, 'O').option);
    NAOS_CHECK_NULL(find_short(f.program, f.build, '\0').option);
    clopts_program_destroy(f.program);
}

NAOS_SUITE(lookup,
    NAOS_ADD_TEST(find_long_searches_command_then_global),
    NAOS_ADD_TEST(find_long_is_exact_case_sensitive_and_uses_name_len),
    NAOS_ADD_TEST(find_long_negation_only_for_negatable_flags_with_long_name),
    NAOS_ADD_TEST(find_long_resolves_each_scope_completely),
    NAOS_ADD_TEST(find_short_searches_command_then_global)
)
