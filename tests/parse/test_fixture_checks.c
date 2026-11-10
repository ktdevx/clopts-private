/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite fixtures: the contents of the shared fixtures of the parser tests
 * (test_fixtures.c) compared with the first-version declarations, their
 * release, their behavior under fault injection and their lookup contexts.
 */

#include "parse/result.h"
#include "test_alloc.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Returns command index of program. */
static const clopts_command_t *command_at(const clopts_program_t *program, size_t index)
{
    NAOS_REQUIRE_UINT_GT(clopts_program_get_command_count(program), index);
    return clopts_program_get_command_at(program, index);
}

/* The global option index of program; the test stops when there is none. */
static const clopts_option_t *global_at(const clopts_program_t *program, size_t index)
{
    NAOS_REQUIRE_UINT_GT(clopts_scope_get_option_count(clopts_program_get_scope(program)), index);
    return clopts_scope_get_option_at(clopts_program_get_scope(program), index);
}

/* Checks that actual is NULL when expected is NULL, and the same string otherwise. */
static void check_str_or_null(const char *actual, const char *expected)
{
    if (expected == NULL)
    {
        NAOS_CHECK_NULL(actual);
    }
    else
    {
        NAOS_REQUIRE_NOT_NULL(actual);
        NAOS_CHECK_STR_EQ(actual, expected);
    }
}

/* ---- fixtures ------------------------------------------------------------ */

/*
 * Expected attributes of one option, as declared by the first-version
 * fixtures. command is the command it is attached to, or NULL for a global
 * option of the program.
 */
typedef struct expected_option
{
    const clopts_command_t *command;
    size_t index;
    clopts_option_kind_t kind;
    const char *long_name;
    char short_name;
    const char *help;
    const char *value_name;
    const char *default_value;
    const char *env;
    char delimiter;
    unsigned count_max;
    bool required;
    bool negatable;
    bool terminal;
} expected_option_t;

/*
 * Checks every model field of option, and that it is attached at index of
 * expected->command, or among the global options of program at index when
 * expected->command is NULL.
 */
static void check_option(const clopts_program_t *program, const clopts_option_t *option,
                         const expected_option_t *expected)
{
    NAOS_REQUIRE_NOT_NULL(option);
    if (expected->command == NULL)
    {
        NAOS_CHECK_PTR_EQ(global_at(program, expected->index), option);
    }
    else
    {
        NAOS_REQUIRE_UINT_GT(clopts_scope_get_option_count(clopts_command_get_scope(expected->command)), expected->index);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_command_get_scope(expected->command), expected->index), option);
    }
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(option), expected->kind);
    check_str_or_null(clopts_option_get_long_name(option), expected->long_name);
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(option), expected->short_name);
    check_str_or_null(clopts_option_get_help(option), expected->help);
    check_str_or_null(clopts_option_get_value_name(option), expected->value_name);
    check_str_or_null(clopts_option_get_default(option), expected->default_value);
    check_str_or_null(clopts_option_get_env(option), expected->env);
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(option), expected->delimiter);
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(option), expected->count_max);
    NAOS_CHECK_INT_EQ(clopts_option_is_required(option), expected->required);
    NAOS_CHECK_INT_EQ(clopts_option_is_negatable(option), expected->negatable);
    NAOS_CHECK_INT_EQ(clopts_option_is_terminal(option), expected->terminal);
}

/*
 * Checks the model fields of positional, and that it is attached at index of
 * command, or among the positionals of program when command is NULL.
 */
static void check_positional(const clopts_positional_t *positional, const clopts_program_t *program,
                             const clopts_command_t *command, size_t index, const char *name, const char *help,
                             bool required, bool variadic)
{
    NAOS_REQUIRE_NOT_NULL(positional);
    if (command == NULL)
    {
        NAOS_REQUIRE_UINT_GT(clopts_scope_get_positional_count(clopts_program_get_scope(program)), index);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(program), index), positional);
    }
    else
    {
        NAOS_REQUIRE_UINT_GT(clopts_scope_get_positional_count(clopts_command_get_scope(command)), index);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_command_get_scope(command), index), positional);
    }
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(positional), name);
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(positional), help);
    NAOS_CHECK_INT_EQ(clopts_positional_is_required(positional), required);
    NAOS_CHECK_INT_EQ(clopts_positional_is_variadic(positional), variadic);
}

/* Checks a subcommand: its place in program, its name and summary, and no description. */
static void check_command(const clopts_command_t *command, const clopts_program_t *program, size_t index,
                          const char *name, const char *summary)
{
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_PTR_EQ(command_at(program, index), command);
    NAOS_CHECK_STR_EQ(clopts_command_get_name(command), name);
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(command), summary);
    NAOS_CHECK_NULL(clopts_command_get_description(command));
}

/* Checks the name and description of a fixture program. */
static void check_program(const clopts_program_t *program, const char *name, const char *description)
{
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), name);
    check_str_or_null(clopts_program_get_description(program), description);
}

NAOS_TEST(fixtures_are_built_completely)
{
    fixture_tool_t tool;
    fixture_filt_t filt;
    fixture_strict_t strict;

    fixture_tool_init(&tool);
    NAOS_CHECK_NOT_NULL(tool.program);
    NAOS_CHECK_TRUE(tool.ok);
    fixture_tool_free(&tool);
    NAOS_CHECK_NULL(tool.program);

    fixture_filt_init(&filt);
    NAOS_CHECK_NOT_NULL(filt.program);
    NAOS_CHECK_TRUE(filt.ok);
    fixture_filt_free(&filt);
    NAOS_CHECK_NULL(filt.program);

    fixture_strict_init(&strict);
    NAOS_CHECK_NOT_NULL(strict.program);
    NAOS_CHECK_TRUE(strict.ok);
    fixture_strict_free(&strict);
    NAOS_CHECK_NULL(strict.program);
}

NAOS_TEST(fixture_tool_declaration)
{
    fixture_tool_t f;
    clopts_program_t *program;

    fixture_tool_init(&f);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(f.ok);

    check_program(program, "tool", "A fictional tool with subcommands.");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 5);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);

    {
        const expected_option_t verbose = {NULL, 0, CLOPTS_OPTION_COUNT, "verbose", 'v', "Increase verbosity.",
                                           NULL, NULL, NULL, '\0', 0, false, false, false};
        const expected_option_t color = {NULL, 1, CLOPTS_OPTION_FLAG, "color", '\0', "Colorize the output.",
                                         NULL, NULL, NULL, '\0', 0, false, true, false};
        const expected_option_t config = {NULL, 2, CLOPTS_OPTION_VALUE, "config", '\0', "Configuration file.",
                                          "PATH", NULL, "TOOL_CONFIG", '\0', 0, false, false, false};
        const expected_option_t help = {NULL, 3, CLOPTS_OPTION_FLAG, "help", 'h', "Show this help",
                                        NULL, NULL, NULL, '\0', 0, false, false, true};
        const expected_option_t version = {NULL, 4, CLOPTS_OPTION_FLAG, "version", '\0', "Show version information",
                                           NULL, NULL, NULL, '\0', 0, false, false, true};

        check_option(program, f.verbose, &verbose);
        check_option(program, f.color, &color);
        check_option(program, f.config, &config);
        check_option(program, f.help, &help);
        check_option(program, f.version, &version);
    }

    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 4);
    check_command(f.build, program, 0, "build", "Build a source.");
    check_command(f.copy, program, 1, "copy", "Copy a file.");
    check_command(f.cat, program, 2, "cat", "Show files.");
    check_command(f.help_command, program, 3, "help", "Show help for a command");

    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(f.build)), 3);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(f.build)), 2);
    {
        const expected_option_t output = {f.build, 0, CLOPTS_OPTION_VALUE, "output", 'o', "Output file.",
                                          "FILE", NULL, NULL, '\0', 0, true, false, false};
        const expected_option_t keys = {f.build, 1, CLOPTS_OPTION_LIST, "keys", 'k', "Keys to build.",
                                        "KEY", NULL, NULL, ',', 0, false, false, false};
        const expected_option_t jobs = {f.build, 2, CLOPTS_OPTION_COUNT, NULL, 'j', "Add a parallel job.",
                                        NULL, NULL, NULL, '\0', 8, false, false, false};

        check_option(program, f.output, &output);
        check_option(program, f.keys, &keys);
        check_option(program, f.jobs, &jobs);
    }
    check_positional(f.build_src, program, f.build, 0, "SRC", "Source to build.", true, false);
    check_positional(f.build_extra, program, f.build, 1, "EXTRA", "Extra inputs.", false, true);

    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(f.copy)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(f.copy)), 2);
    check_positional(f.copy_src, program, f.copy, 0, "SRC", "Source.", true, false);
    check_positional(f.copy_dst, program, f.copy, 1, "DST", "Destination.", true, false);

    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(f.cat)), 1);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(f.cat)), 1);
    {
        const expected_option_t tags = {f.cat, 0, CLOPTS_OPTION_LIST, "tags", '\0', "Tags to show.",
                                        "TAG", "a,b", "TOOL_TAGS", ',', 0, false, false, false};

        check_option(program, f.tags, &tags);
    }
    check_positional(f.cat_file, program, f.cat, 0, "FILE", "Files to show.", false, true);

    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(f.help_command)), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(f.help_command)), 1);
    check_positional(clopts_scope_get_positional_at(clopts_command_get_scope(f.help_command), 0), program, f.help_command, 0, "COMMAND",
                     "Command to show help for", false, false);

    fixture_tool_free(&f);
}

NAOS_TEST(fixture_filt_declaration)
{
    fixture_filt_t f;
    clopts_program_t *program;

    fixture_filt_init(&f);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(f.ok);

    check_program(program, "filt", "A fictional filter without subcommands.");
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 1);
    {
        const expected_option_t lines = {NULL, 0, CLOPTS_OPTION_VALUE, "lines", 'n', "Number of lines.",
                                         "N", "10", "FILT_LINES", '\0', 0, false, false, false};
        const expected_option_t help = {NULL, 1, CLOPTS_OPTION_FLAG, "help", 'h', "Show this help",
                                        NULL, NULL, NULL, '\0', 0, false, false, true};

        check_option(program, f.lines, &lines);
        check_option(program, f.help, &help);
    }
    check_positional(f.file, program, NULL, 0, "FILE", "Files to filter.", false, true);

    fixture_filt_free(&f);
}

NAOS_TEST(fixture_strict_declaration)
{
    fixture_strict_t f;
    clopts_program_t *program;

    fixture_strict_init(&f);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(f.ok);

    check_program(program, "strict", "A fictional program with a required global option.");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    {
        const expected_option_t token = {NULL, 0, CLOPTS_OPTION_VALUE, "token", '\0', "Access token.",
                                         "TOKEN", NULL, NULL, '\0', 0, true, false, false};
        const expected_option_t help = {NULL, 1, CLOPTS_OPTION_FLAG, "help", 'h', "Show this help",
                                        NULL, NULL, NULL, '\0', 0, false, false, true};

        check_option(program, f.token, &token);
        check_option(program, f.help, &help);
    }

    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 1);
    check_command(f.run, program, 0, "run", "Run it.");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(f.run)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(f.run)), 0);

    fixture_strict_free(&f);
}

NAOS_TEST(fixtures_release_every_block)
{
    fixture_tool_t tool;
    fixture_filt_t filt;
    fixture_strict_t strict;

    test_alloc_begin(0);
    fixture_tool_init(&tool);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_CHECK_TRUE(tool.ok);
    NAOS_CHECK_UINT_GT(test_alloc.calls, 0);
    NAOS_CHECK_INT_GT(test_alloc.live, 0);
    fixture_tool_free(&tool);
    NAOS_CHECK_TRUE(test_alloc_end());

    test_alloc_begin(0);
    fixture_filt_init(&filt);
    NAOS_REQUIRE_NOT_NULL(filt.program);
    NAOS_CHECK_TRUE(filt.ok);
    NAOS_CHECK_INT_GT(test_alloc.live, 0);
    fixture_filt_free(&filt);
    NAOS_CHECK_TRUE(test_alloc_end());

    test_alloc_begin(0);
    fixture_strict_init(&strict);
    NAOS_REQUIRE_NOT_NULL(strict.program);
    NAOS_CHECK_TRUE(strict.ok);
    NAOS_CHECK_INT_GT(test_alloc.live, 0);
    fixture_strict_free(&strict);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* Checks that a fixture_tool_t whose program could not be made is all NULL. */
static void check_tool_cleared(const fixture_tool_t *tool)
{
    NAOS_CHECK_NULL(tool->program);
    NAOS_CHECK_FALSE(tool->ok);
    NAOS_CHECK_NULL(tool->build);
    NAOS_CHECK_NULL(tool->help_command);
    NAOS_CHECK_NULL(tool->verbose);
    NAOS_CHECK_NULL(tool->help);
}

/*
 * A failing fault point fails every later one too (test_alloc.h). The first
 * two fault points are in clopts_program_create(), and the last one is in the
 * first clopts_program_add_command() of test_decl_finish() (the command
 * array; the later commands fit in it). So when a subcommand was made before
 * the failing fault point, adding it fails: no program is made, everything
 * made so far is released (by the helpers and test_decl_finish()), and the
 * fixture is cleared with ok false. When the failing fault point comes before
 * every subcommand, none is made and the program is kept without
 * subcommands, with ok false; fixture_tool_free() releases it.
 */
NAOS_TEST(fixture_out_of_memory_is_reported_and_released)
{
    fixture_tool_t tool;
    size_t total;
    size_t fail_at;
    size_t kept = 0;

    /* number of fault points of a full assembly, test_decl_finish() included */
    test_alloc_begin(0);
    fixture_tool_init(&tool);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_REQUIRE_TRUE(tool.ok);
    total = test_alloc.calls;
    fixture_tool_free(&tool);
    NAOS_CHECK_TRUE(test_alloc_end());
    NAOS_REQUIRE_UINT_GT(total, 4);

    for (fail_at = 1; fail_at <= total; fail_at++)
    {
        NAOS_NOTE("fail_at=%u of %u", (unsigned)fail_at, (unsigned)total);
        test_alloc_begin(fail_at);
        fixture_tool_init(&tool);
        NAOS_CHECK_FALSE(tool.ok);
        NAOS_CHECK_UINT_GE(test_alloc.calls, fail_at);
        if (tool.program != NULL)
        {
            /* the program was made, and no subcommand */
            NAOS_CHECK_UINT_GT(fail_at, 2);
            NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(tool.program), 0);
            NAOS_CHECK_NULL(tool.build);
            NAOS_CHECK_NULL(tool.help_command);
            kept++;
        }
        else
        {
            check_tool_cleared(&tool);
        }
        if (fail_at == total)
        {
            /* only the add of the first subcommand failed, and it stopped at its failing fault point */
            NAOS_CHECK_NULL(tool.program);
            NAOS_CHECK_UINT_EQ(test_alloc.calls, fail_at);
        }
        fixture_tool_free(&tool);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
    NAOS_CHECK_UINT_GT(kept, 0);

    /* one fault point more: nothing fails */
    test_alloc_begin(total + 1);
    fixture_tool_init(&tool);
    NAOS_CHECK_NOT_NULL(tool.program);
    NAOS_CHECK_TRUE(tool.ok);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, total);
    fixture_tool_free(&tool);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(fixture_env_lends_from_its_buffers)
{
    static const fixture_env_var_t vars[] = {
        {"TOOL_CONFIG", "env.conf"},
        {"TOOL_TAGS", ""},
    };
    fixture_env_t env;
    const char *value = NULL;

    fixture_env_init(&env, vars, 2);
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL_CONFIG"));

    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "TOOL_CONFIG", &value), CLOPTS_ENV_FOUND);
    NAOS_REQUIRE_NOT_NULL(value);
    NAOS_CHECK_STR_EQ(value, "env.conf");
    NAOS_CHECK_TRUE(value != vars[0].value);
    NAOS_CHECK_PTR_EQ(value, env.buffers[0]);

    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "TOOL_TAGS", &value), CLOPTS_ENV_FOUND);
    NAOS_CHECK_STR_EQ(value, "");

    value = NULL;
    NAOS_CHECK_INT_EQ(fixture_env_getenv(&env, "FILT_LINES", &value), CLOPTS_ENV_UNSET);
    NAOS_CHECK_NULL(value);

    NAOS_CHECK_UINT_EQ(env.query_count, 3);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "FILT_LINES"));
    NAOS_CHECK_FALSE(fixture_env_was_queried(&env, "TOOL"));
    NAOS_CHECK_UINT_EQ(fixture_env_query_count(&env, "TOOL_TAGS"), 1);
}

NAOS_TEST(fixture_env_fail_next_is_one_shot)
{
    static const fixture_env_var_t vars[] = {
        {"TOOL_CONFIG", "env.conf"},
    };
    fixture_env_t env;
    const char *value = NULL;

    fixture_env_init(&env, vars, 1);
    env.fail_next = true;
    NAOS_CHECK_INT_EQ(fixture_env_getenv(&env, "TOOL_CONFIG", &value), CLOPTS_ENV_FAILED);
    NAOS_CHECK_FALSE(env.fail_next);
    NAOS_CHECK_TRUE(fixture_env_was_queried(&env, "TOOL_CONFIG"));
    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "TOOL_CONFIG", &value), CLOPTS_ENV_FOUND);
    NAOS_CHECK_STR_EQ(value, "env.conf");
    NAOS_CHECK_UINT_EQ(fixture_env_query_count(&env, "TOOL_CONFIG"), 2);
}

NAOS_TEST(fixture_env_clobber_overwrites_lent_values)
{
    static const fixture_env_var_t vars[] = {
        {"TOOL_CONFIG", "env.conf"},
        {"TOOL_TAGS", "x,y"},
    };
    fixture_env_t env;
    const char *config = NULL;
    const char *tags = NULL;

    fixture_env_init(&env, vars, 2);
    env.clobber = true;
    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "TOOL_CONFIG", &config), CLOPTS_ENV_FOUND);
    NAOS_CHECK_STR_EQ(config, "env.conf");

    /* the next call destroys the earlier loan */
    NAOS_REQUIRE_INT_EQ(fixture_env_getenv(&env, "TOOL_TAGS", &tags), CLOPTS_ENV_FOUND);
    NAOS_CHECK_STR_EQ(tags, "x,y");
    NAOS_CHECK_TRUE(strcmp(config, "env.conf") != 0);
    NAOS_CHECK_INT_EQ(config[0], '#');

    /* destroyed on demand as well */
    fixture_env_clobber(&env);
    NAOS_CHECK_TRUE(strcmp(tags, "x,y") != 0);
    NAOS_CHECK_INT_EQ(tags[0], '#');

    /* the table itself is untouched */
    NAOS_CHECK_STR_EQ(vars[0].value, "env.conf");
    NAOS_CHECK_STR_EQ(vars[1].value, "x,y");
}

NAOS_SUITE(fixtures,
    NAOS_ADD_TEST(fixtures_are_built_completely),
    NAOS_ADD_TEST(fixture_tool_declaration),
    NAOS_ADD_TEST(fixture_filt_declaration),
    NAOS_ADD_TEST(fixture_strict_declaration),
    NAOS_ADD_TEST(fixtures_release_every_block),
    NAOS_ADD_TEST(fixture_out_of_memory_is_reported_and_released),
    NAOS_ADD_TEST(fixture_env_lends_from_its_buffers),
    NAOS_ADD_TEST(fixture_env_fail_next_is_one_shot),
    NAOS_ADD_TEST(fixture_env_clobber_overwrites_lent_values)
)
