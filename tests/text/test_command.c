/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite command: the help command (clopts_help_command_create() and
 * clopts_help_find_target()). The targets are read from results of parsing
 * real argv against the tool fixture, whose help command is an ordinary
 * subcommand.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

/* A value no clopts function returns, to see that an out pointer is written. */
static const clopts_option_t *const command_stale_option = (const clopts_option_t *)&command_stale_option;

NAOS_TEST(help_command_new_declaration)
{
    clopts_command_t *command = NULL;
    const clopts_positional_t *positional;

    NAOS_REQUIRE_INT_EQ(clopts_help_command_create(&command), CLOPTS_DECL_ERROR_NONE);
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_STR_EQ(clopts_command_get_name(command), "help");
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(command), "Show help for a command");
    NAOS_CHECK_NULL(clopts_command_get_description(command));
    NAOS_CHECK_UINT_EQ(clopts_command_get_option_count(command), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_command_get_positional_count(command), 1);
    positional = clopts_command_get_positional_at(command, 0);
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(positional), "COMMAND");
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(positional), "Command to show help for");
    NAOS_CHECK_FALSE(clopts_positional_is_required(positional));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(positional));
    /* It is unattached: the caller owns it. */
    clopts_command_destroy(command);
}

NAOS_TEST(help_command_new_gives_a_new_command_each_time)
{
    /*
     * Two of them cannot be in one program (the second is a DUPLICATE_COMMAND
     * and stays with the caller), but each is a separate command.
     */
    clopts_command_t *first = NULL;
    clopts_command_t *second = NULL;
    clopts_program_t *program = test_decl_begin("t");

    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_INT_EQ(clopts_help_command_create(&first), CLOPTS_DECL_ERROR_NONE);
    NAOS_REQUIRE_INT_EQ(clopts_help_command_create(&second), CLOPTS_DECL_ERROR_NONE);
    NAOS_CHECK_TRUE(first != second);
    NAOS_REQUIRE_INT_EQ(clopts_program_add_command(program, first), CLOPTS_DECL_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, second), CLOPTS_DECL_ERROR_DUPLICATE_COMMAND);
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 1);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), first);
    clopts_command_destroy(second);
    test_decl_discard(program);
}

/* Environment lookup that finds no variable, so the parses do not depend on the environment. */
static clopts_env_status_t command_env_unset(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_UNSET;
}

/* Number of elements of argv before its terminating NULL. */
#define COMMAND_ARGC(argv) ((int)(sizeof(argv) / sizeof((argv)[0])) - 1)

/*
 * Parses argv against program, requires the status, and checks that
 * clopts_help_find_target() returns expected and sets the target to
 * expected_target (from a stale value).
 */
static void command_check_target(const clopts_program_t *program, int argc, char *const *argv,
                                clopts_parse_status_t status, bool expected, const clopts_command_t *expected_target)
{
    clopts_result_t *result = clopts_parse_with_env(program, argc, argv, command_env_unset, NULL);
    const clopts_command_t *target = (const clopts_command_t *)&command_stale_option;

    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), status);
    NAOS_CHECK_INT_EQ(clopts_help_find_target(program, result, &target), expected);
    NAOS_CHECK_PTR_EQ(target, expected_target);
    clopts_result_destroy(result);
}

NAOS_TEST(help_target_without_a_positional_is_the_program)
{
    char *argv[] = { "tool", "help", NULL };
    text_fixture_tool_t f;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    command_check_target(f.program, COMMAND_ARGC(argv), argv, CLOPTS_PARSE_OK, true, NULL);
    text_fixture_tool_free(&f);
}

NAOS_TEST(help_target_names_a_known_command)
{
    char *argv_build[] = { "tool", "help", "build", NULL };
    char *argv_help[] = { "tool", "help", "help", NULL };
    char *argv_global[] = { "tool", "-v", "help", "copy", NULL };
    text_fixture_tool_t f;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    command_check_target(f.program, COMMAND_ARGC(argv_build), argv_build, CLOPTS_PARSE_OK, true, f.build);
    command_check_target(f.program, COMMAND_ARGC(argv_help), argv_help, CLOPTS_PARSE_OK, true, f.help_command);
    command_check_target(f.program, COMMAND_ARGC(argv_global), argv_global, CLOPTS_PARSE_OK, true, f.copy);
    text_fixture_tool_free(&f);
}

NAOS_TEST(help_target_of_an_unknown_command_is_false)
{
    char *argv[] = { "tool", "help", "bulid", NULL };
    /* The program name is not a subcommand. */
    char *argv_program[] = { "tool", "help", "tool", NULL };
    text_fixture_tool_t f;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    command_check_target(f.program, COMMAND_ARGC(argv), argv, CLOPTS_PARSE_OK, false, NULL);
    command_check_target(f.program, COMMAND_ARGC(argv_program), argv_program, CLOPTS_PARSE_OK, false, NULL);
    text_fixture_tool_free(&f);
}

NAOS_TEST(help_target_of_a_status_other_than_ok_is_false)
{
    char *argv_terminated[] = { "tool", "help", "build", "--help", NULL };
    char *argv_no_command[] = { "tool", NULL };
    char *argv_usage[] = { "tool", "help", "build", "copy", NULL };
    const clopts_command_t *target = (const clopts_command_t *)&command_stale_option;
    text_fixture_tool_t f;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    command_check_target(f.program, COMMAND_ARGC(argv_terminated), argv_terminated, CLOPTS_PARSE_TERMINATED, false, NULL);
    command_check_target(f.program, COMMAND_ARGC(argv_no_command), argv_no_command, CLOPTS_PARSE_NO_COMMAND, false, NULL);
    command_check_target(f.program, COMMAND_ARGC(argv_usage), argv_usage, CLOPTS_PARSE_USAGE_ERROR, false, NULL);

    /* A NULL result reads as out of memory. */
    NAOS_CHECK_FALSE(clopts_help_find_target(f.program, NULL, &target));
    NAOS_CHECK_NULL(target);
    text_fixture_tool_free(&f);
}

NAOS_SUITE(command,
    NAOS_ADD_TEST(help_command_new_declaration),
    NAOS_ADD_TEST(help_command_new_gives_a_new_command_each_time),
    NAOS_ADD_TEST(help_target_without_a_positional_is_the_program),
    NAOS_ADD_TEST(help_target_names_a_known_command),
    NAOS_ADD_TEST(help_target_of_an_unknown_command_is_false),
    NAOS_ADD_TEST(help_target_of_a_status_other_than_ok_is_false)
)
