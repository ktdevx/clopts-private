/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite build: the builder functions, the releases, string copies, the
 * last-call-wins and reset rules, handle stability, out of memory in every
 * allocating builder function, the one-shot creation of a program from a root
 * and its commands (its errors, their order, and the sealing on success), the
 * read functions, and the contents of the shared fixtures (test_fixtures.c)
 * compared with the first-version declarations.
 *
 * A declaration is built from the bottom up: options and positionals are
 * attached to commands, and clopts_program_create() takes the root and the
 * commands in one call. Every builder function returns its error right away
 * and a failing call changes nothing; the caller still owns what it passed.
 * The declaration rules within a command (and the NULL arguments of the
 * option and positional setters) are tested in the decl
 * suites. Stored values are read through the internal model or the public
 * read functions, and the fault allocator (test_alloc.h) shows which blocks
 * are alive.
 */

#include "parser/parser_internal.h"
#include "test_alloc.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

/* Checks that call returns CLOPTS_ERROR_NONE. */
#define EXPECT_OK(call) NAOS_CHECK_INT_EQ((call), CLOPTS_ERROR_NONE)

/*
 * Objects whose address an out pointer holds before a call, so that a test
 * can see the call set it to NULL. They are never read or written.
 */
static clopts_program_t sentinel_program;
static clopts_command_t sentinel_command;
static char sentinel_option_storage;
#define SENTINEL_OPTION ((clopts_option_t *)&sentinel_option_storage)
static char sentinel_positional_storage;
#define SENTINEL_POSITIONAL ((clopts_positional_t *)&sentinel_positional_storage)

/* Returns option index of scope (an element of scope->options). */
static clopts_option_t *option_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_option_t **)clopts_core_vec_at(&scope->options, index);
}

/* Returns positional index of scope. */
static clopts_positional_t *positional_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, index);
}

/* Returns command index of program. */
static clopts_command_t *command_at(const clopts_program_t *program, size_t index)
{
    NAOS_REQUIRE_UINT_GT(program->command_count, index);
    return program->commands[index];
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

/* A new unattached command (a root or a subcommand); the test stops when it cannot be made. */
static clopts_command_t *make_command(const char *name)
{
    clopts_command_t *command = test_decl_root(name);

    NAOS_REQUIRE_NOT_NULL(command);
    return command;
}

/* A new unattached option; the test stops when it cannot be made. */
static clopts_option_t *make(clopts_option_kind_t kind, const char *long_name, char short_name)
{
    clopts_option_t *option = test_decl_option(kind, long_name, short_name);

    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/* A new unattached positional; the test stops when it cannot be made. */
static clopts_positional_t *make_positional(const char *name)
{
    clopts_positional_t *positional = test_decl_positional(name);

    NAOS_REQUIRE_NOT_NULL(positional);
    return positional;
}

/* clopts_program_create(root, commands, count); the test stops when it fails. */
static clopts_program_t *make_program(clopts_command_t *root, clopts_command_t *const *commands, size_t count)
{
    clopts_program_t *program = &sentinel_program;

    NAOS_REQUIRE_INT_EQ(clopts_program_create(root, commands, count, &program), CLOPTS_ERROR_NONE);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(program != &sentinel_program);
    return program;
}

/*
 * Calls clopts_program_create(root, commands, count) expecting error, and checks
 * that the out pointer is NULL and that nothing was allocated (the checks
 * come before every allocation).
 */
static void expect_program_new_fails(clopts_command_t *root, clopts_command_t *const *commands, size_t count,
                                     clopts_error_t error)
{
    clopts_program_t *program = &sentinel_program;
    long live = test_alloc.live;
    size_t calls = test_alloc.calls;

    NAOS_CHECK_INT_EQ(clopts_program_create(root, commands, count, &program), error);
    NAOS_CHECK_NULL(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
}

/* Parses argv = { name } with program, checks the status, and releases the result. */
static void parse_name_only(const clopts_program_t *program, const char *name, clopts_status_t status)
{
    char arg0[16];
    char *argv[2];
    clopts_result_t *result;

    (void)snprintf(arg0, sizeof(arg0), "%s", name);
    argv[0] = arg0;
    argv[1] = NULL;
    result = clopts_parse(program, 1, argv);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), status);
    clopts_result_free(result);
}

/* ---- NULL ------------------------------------------------------------------ */

NAOS_TEST(free_null_does_nothing)
{
    clopts_program_destroy(NULL);
    clopts_command_destroy(NULL);
    clopts_option_destroy(NULL);
    clopts_positional_destroy(NULL);
    NAOS_CHECK_NULL(clopts_program_root(NULL));
}

/* ---- commands ---------------------------------------------------------------- */

/* clopts_command_create() copies the name and starts with no texts, options or positionals, unattached. */
NAOS_TEST(command_new_copies_name_and_starts_empty)
{
    char name[] = "build";
    clopts_command_t *command = &sentinel_command;

    EXPECT_OK(clopts_command_create(name, &command));
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_REQUIRE_TRUE(command != &sentinel_command);
    name[0] = 'X';

    NAOS_REQUIRE_NOT_NULL(command->name);
    NAOS_CHECK_TRUE(command->name != name);
    NAOS_CHECK_STR_EQ(command->name, "build");
    NAOS_CHECK_NULL(command->summary);
    NAOS_CHECK_NULL(command->description);
    NAOS_CHECK_UINT_EQ(command->options.len, 0);
    NAOS_CHECK_UINT_EQ(command->positionals.len, 0);
    clopts_command_destroy(command);
}

/*
 * A malformed name gives INVALID_NAME and allocates nothing. There are no
 * reserved names: "help", "h" and "version" are ordinary command names.
 */
NAOS_TEST(command_new_checks_name)
{
    static const char *const bad[] = { "", "-x", "--x", "a b", "a\tb", "ab\n" };
    static const char *const good[] = { "help", "h", "version", "x", "a-b", "a=b", "B" };
    clopts_command_t *command;
    size_t i;

    test_alloc_begin(0);
    command = &sentinel_command;
    NAOS_CHECK_INT_EQ(clopts_command_create(NULL, &command), CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_NULL(command);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", bad[i]);
        command = &sentinel_command;
        NAOS_CHECK_INT_EQ(clopts_command_create(bad[i], &command), CLOPTS_ERROR_INVALID_NAME);
        NAOS_CHECK_NULL(command);
    }
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 0);

    for (i = 0; i < sizeof(good) / sizeof(good[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", good[i]);
        command = &sentinel_command;
        EXPECT_OK(clopts_command_create(good[i], &command));
        NAOS_REQUIRE_NOT_NULL(command);
        NAOS_REQUIRE_TRUE(command != &sentinel_command);
        NAOS_CHECK_STR_EQ(command->name, good[i]);
        clopts_command_destroy(command);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- constructors ------------------------------------------------------ */

NAOS_TEST(new_option_and_positional_start_reset_and_unattached)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE,
                                                  CLOPTS_OPTION_LIST };
    char long_name[] = "output";
    char pos_name[] = "src";
    clopts_option_t *option;
    clopts_positional_t *positional;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = SENTINEL_OPTION;
        EXPECT_OK(clopts_option_create(kinds[i], long_name, 'o', &option));
        NAOS_REQUIRE_NOT_NULL(option);
        NAOS_REQUIRE_TRUE(option != SENTINEL_OPTION);
        NAOS_CHECK_INT_EQ(clopts_option_kind_of(option), kinds[i]);
        NAOS_CHECK_STR_EQ(clopts_option_long_name(option), "output");
        NAOS_CHECK_TRUE(clopts_option_long_name(option) != long_name);
        NAOS_CHECK_INT_EQ(clopts_option_short_name(option), 'o');
        NAOS_CHECK_NULL(clopts_option_help(option));
        NAOS_CHECK_NULL(clopts_option_value_name(option));
        NAOS_CHECK_NULL(clopts_option_default(option));
        NAOS_CHECK_NULL(clopts_option_env(option));
        NAOS_CHECK_INT_EQ(clopts_option_delimiter(option), '\0');
        NAOS_CHECK_UINT_EQ(clopts_option_count_max(option), 0);
        NAOS_CHECK_FALSE(clopts_option_required(option));
        NAOS_CHECK_FALSE(clopts_option_negatable(option));
        NAOS_CHECK_FALSE(clopts_option_terminal(option));
        clopts_option_destroy(option);
    }

    option = make(CLOPTS_OPTION_VALUE, NULL, 'o');
    NAOS_CHECK_NULL(clopts_option_long_name(option));
    clopts_option_destroy(option);
    option = make(CLOPTS_OPTION_FLAG, "force", '\0');
    NAOS_CHECK_INT_EQ(clopts_option_short_name(option), '\0');
    clopts_option_destroy(option);

    positional = SENTINEL_POSITIONAL;
    EXPECT_OK(clopts_positional_create(pos_name, &positional));
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_REQUIRE_TRUE(positional != SENTINEL_POSITIONAL);
    pos_name[0] = 'X';
    NAOS_CHECK_STR_EQ(clopts_positional_name(positional), "src");
    NAOS_CHECK_NULL(clopts_positional_help(positional));
    NAOS_CHECK_FALSE(clopts_positional_required(positional));
    NAOS_CHECK_FALSE(clopts_positional_variadic(positional));
    clopts_positional_destroy(positional);
}

/* ---- release ------------------------------------------------------------- */

/*
 * An option, positional or command that is never handed on is released by its
 * own free, block by block; clopts_command_destroy() also releases the options
 * and positionals attached to the command.
 */
NAOS_TEST(unattached_objects_are_released_by_their_free)
{
    clopts_command_t *command;
    clopts_option_t *option;
    clopts_positional_t *positional;

    test_alloc_begin(0);
    option = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(option, "Output file."));
    EXPECT_OK(clopts_option_set_value_name(option, "FILE"));
    EXPECT_OK(clopts_option_set_env(option, "TOOL_OUTPUT"));
    EXPECT_OK(clopts_option_set_default(option, "a.out"));
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_help(positional, "Source."));
    NAOS_CHECK_INT_GT(test_alloc.live, 0);

    clopts_option_destroy(option);
    clopts_positional_destroy(positional);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);

    command = make_command("build");
    EXPECT_OK(clopts_command_set_summary(command, "Builds."));
    EXPECT_OK(clopts_command_set_description(command, "Builds the tree."));
    option = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(option, "Output file."));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(command, option));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(command, CLOPTS_OPTION_FLAG, "force", 'f'));
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_help(positional, "Source."));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach_positional(command, positional));
    NAOS_CHECK_INT_GT(test_alloc.live, 0);

    clopts_command_destroy(command);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_UINT_EQ(test_alloc.bad_frees, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- attach ------------------------------------------------------------ */

NAOS_TEST(attach_links_objects_in_order)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *flag;
    clopts_option_t *count;
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *local;
    clopts_positional_t *src;
    clopts_positional_t *dst;

    root = make_command("tool");
    flag = test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    count = test_decl_add(root, CLOPTS_OPTION_COUNT, "verbose", 'v');
    value = test_decl_add(root, CLOPTS_OPTION_VALUE, NULL, 'o');
    list = test_decl_add(root, CLOPTS_OPTION_LIST, "include", '\0');
    build = test_decl_command(root, "build");
    copy = test_decl_command(root, "copy");
    local = test_decl_add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    src = test_decl_add_positional(copy, "src");
    dst = test_decl_add_positional(copy, "dst");

    NAOS_REQUIRE_NOT_NULL(flag);
    NAOS_REQUIRE_NOT_NULL(count);
    NAOS_REQUIRE_NOT_NULL(value);
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_REQUIRE_NOT_NULL(build);
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_REQUIRE_NOT_NULL(local);
    NAOS_REQUIRE_NOT_NULL(src);
    NAOS_REQUIRE_NOT_NULL(dst);

    program = test_decl_finish(root);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_CHECK_PTR_EQ(program->root, root);

    /* root options in order of attachment */
    NAOS_REQUIRE_UINT_EQ(root->options.len, 4);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), flag);
    NAOS_CHECK_PTR_EQ(option_at(root, 1), count);
    NAOS_CHECK_PTR_EQ(option_at(root, 2), value);
    NAOS_CHECK_PTR_EQ(option_at(root, 3), list);

    NAOS_CHECK_INT_EQ(clopts_option_kind_of(flag), CLOPTS_OPTION_FLAG);
    NAOS_CHECK_INT_EQ(clopts_option_kind_of(count), CLOPTS_OPTION_COUNT);
    NAOS_CHECK_INT_EQ(clopts_option_kind_of(value), CLOPTS_OPTION_VALUE);
    NAOS_CHECK_INT_EQ(clopts_option_kind_of(list), CLOPTS_OPTION_LIST);
    NAOS_REQUIRE_NOT_NULL(clopts_option_long_name(flag));
    NAOS_CHECK_STR_EQ(clopts_option_long_name(flag), "force");
    NAOS_CHECK_INT_EQ(clopts_option_short_name(flag), 'f');
    NAOS_CHECK_NULL(clopts_option_long_name(value));
    NAOS_CHECK_INT_EQ(clopts_option_short_name(value), 'o');
    NAOS_CHECK_INT_EQ(clopts_option_short_name(list), '\0');

    /* commands in the order given to clopts_program_create() */
    NAOS_REQUIRE_UINT_EQ(program->command_count, 2);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), build);
    NAOS_CHECK_PTR_EQ(command_at(program, 1), copy);
    NAOS_REQUIRE_NOT_NULL(build->name);
    NAOS_CHECK_STR_EQ(build->name, "build");
    NAOS_CHECK_STR_EQ(copy->name, "copy");
    NAOS_CHECK_NULL(build->summary);
    NAOS_CHECK_NULL(build->description);

    /* command options and positionals */
    NAOS_REQUIRE_UINT_EQ(build->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(build, 0), local);
    NAOS_CHECK_UINT_EQ(build->positionals.len, 0);

    NAOS_REQUIRE_UINT_EQ(copy->positionals.len, 2);
    NAOS_CHECK_PTR_EQ(positional_at(copy, 0), src);
    NAOS_CHECK_PTR_EQ(positional_at(copy, 1), dst);
    NAOS_REQUIRE_NOT_NULL(clopts_positional_name(src));
    NAOS_CHECK_STR_EQ(clopts_positional_name(src), "src");
    NAOS_CHECK_UINT_EQ(copy->options.len, 0);

    clopts_program_destroy(program);
}

/* A root without subcommands takes positionals, and its description is the program description. */
NAOS_TEST(root_takes_positionals)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_positional_t *file;

    root = make_command("filt");
    file = test_decl_add_positional(root, "file");
    EXPECT_OK(clopts_command_set_description(root, "Filters lines."));
    NAOS_REQUIRE_NOT_NULL(file);

    program = make_program(root, NULL, 0);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);
    NAOS_CHECK_UINT_EQ(program->command_count, 0);
    NAOS_CHECK_NULL(program->commands);
    NAOS_REQUIRE_UINT_EQ(root->positionals.len, 1);
    NAOS_CHECK_PTR_EQ(positional_at(root, 0), file);
    NAOS_REQUIRE_NOT_NULL(root->description);
    NAOS_CHECK_STR_EQ(root->description, "Filters lines.");
    NAOS_CHECK_STR_EQ(clopts_command_name(clopts_program_root(program)), "filt");

    clopts_program_destroy(program);
}

/* ---- clopts_program_create() ---------------------------------------------------- */

/*
 * The program takes the root and the commands as they are: same handles, in
 * the order given (the array itself is copied, so the caller's array may
 * change afterwards), and they become attached. commands may be NULL or not
 * when command_count is 0.
 */
NAOS_TEST(program_new_takes_root_and_commands)
{
    clopts_command_t *root;
    clopts_command_t *commands[3];
    clopts_command_t *unused[1];
    clopts_program_t *program;

    test_alloc_begin(0);
    root = make_command("tool");
    commands[0] = make_command("copy");
    commands[1] = make_command("build");
    commands[2] = make_command("cat");
    program = make_program(root, commands, 3);
    NAOS_CHECK_PTR_EQ(program->root, root);
    NAOS_REQUIRE_UINT_EQ(program->command_count, 3);
    NAOS_CHECK_TRUE(program->commands != commands);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), commands[0]);
    NAOS_CHECK_PTR_EQ(command_at(program, 1), commands[1]);
    NAOS_CHECK_PTR_EQ(command_at(program, 2), commands[2]);

    /* the caller's array is not kept */
    commands[0] = NULL;
    NAOS_CHECK_STR_EQ(command_at(program, 0)->name, "copy");
    clopts_program_destroy(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);

    /* no commands: NULL or an array that is not read */
    root = make_command("tool");
    program = make_program(root, NULL, 0);
    NAOS_CHECK_UINT_EQ(program->command_count, 0);
    clopts_program_destroy(program);

    root = make_command("tool");
    unused[0] = NULL;
    program = make_program(root, unused, 0);
    NAOS_CHECK_UINT_EQ(program->command_count, 0);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* DUPLICATE_COMMAND: two subcommands share a name (exact, case-sensitive). Nothing changes. */
NAOS_TEST(program_new_duplicate_command_changes_nothing)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_command_t *again;
    clopts_command_t *upper;
    clopts_command_t *commands[4];

    test_alloc_begin(0);
    root = make_command("tool");
    build = make_command("build");
    copy = make_command("copy");
    again = make_command("build");
    upper = make_command("Build");

    commands[0] = build;
    commands[1] = copy;
    commands[2] = again;
    expect_program_new_fails(root, commands, 3, CLOPTS_ERROR_DUPLICATE_COMMAND);
    commands[0] = again;
    commands[1] = build;
    expect_program_new_fails(root, commands, 2, CLOPTS_ERROR_DUPLICATE_COMMAND);
    clopts_command_destroy(again);

    /* a subcommand named like the root and names differing in case are fine */
    clopts_command_destroy(copy);
    copy = make_command("tool");
    commands[0] = build;
    commands[1] = upper;
    commands[2] = copy;
    clopts_program_destroy(make_program(root, commands, 3));
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * MIXED_LAYOUT: a root with positionals cannot have subcommands. Root
 * options with subcommands, and root positionals without, are fine.
 */
NAOS_TEST(program_new_mixed_layout_changes_nothing)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_program_t *program;

    test_alloc_begin(0);
    root = make_command("tool");
    build = make_command("build");
    NAOS_REQUIRE_NOT_NULL(test_decl_add_positional(root, "file"));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f'));

    expect_program_new_fails(root, &build, 1, CLOPTS_ERROR_MIXED_LAYOUT);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 1);

    /* the same root without subcommands */
    program = make_program(root, &build, 0);
    clopts_program_destroy(program);

    /* root options with a subcommand */
    root = make_command("tool");
    NAOS_REQUIRE_NOT_NULL(test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f'));
    clopts_program_destroy(make_program(root, &build, 1));
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The errors of clopts_program_create() come in a fixed order:
 * DUPLICATE_COMMAND, MIXED_LAYOUT. Each case below breaks a later rule earlier in the arguments.
 */
NAOS_TEST(program_new_reports_the_first_error)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *again;
    clopts_command_t *commands[4];

    test_alloc_begin(0);
    root = make_command("tool");
    build = make_command("build");
    again = make_command("build");
    NAOS_REQUIRE_NOT_NULL(test_decl_add_positional(root, "file"));

    /* DUPLICATE_COMMAND before MIXED_LAYOUT */
    commands[0] = build;
    commands[1] = again;
    expect_program_new_fails(root, commands, 2, CLOPTS_ERROR_DUPLICATE_COMMAND);

    /* MIXED_LAYOUT last */
    expect_program_new_fails(root, commands, 1, CLOPTS_ERROR_MIXED_LAYOUT);

    clopts_command_destroy(again);
    clopts_command_destroy(build);
    clopts_command_destroy(root);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Option names are checked within one command only: the root and a
 * subcommand, or two subcommands, may declare the same names. There are no
 * reserved names either.
 */
NAOS_TEST(program_new_does_not_check_names_between_commands)
{
    clopts_command_t *root;
    clopts_command_t *commands[2];
    clopts_program_t *program;
    clopts_option_t *color;

    root = make_command("tool");
    commands[0] = make_command("build");
    commands[1] = make_command("help");
    color = make(CLOPTS_OPTION_FLAG, "color", 'c');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, color));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(root, CLOPTS_OPTION_FLAG, "help", 'h'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(root, CLOPTS_OPTION_FLAG, "version", 'V'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[0], CLOPTS_OPTION_VALUE, "color", 'c'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[0], CLOPTS_OPTION_FLAG, "no-color", '\0'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[0], CLOPTS_OPTION_COUNT, "help", 'h'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[1], CLOPTS_OPTION_COUNT, "help", 'h'));

    program = make_program(root, commands, 2);
    NAOS_CHECK_UINT_EQ(root->options.len, 3);
    NAOS_CHECK_UINT_EQ(commands[0]->options.len, 3);
    NAOS_CHECK_UINT_EQ(commands[1]->options.len, 1);
    clopts_program_destroy(program);
}

/* ---- read functions ---------------------------------------------------------- */

/*
 * The read functions of a program: the root, the subcommands in the order
 * given, and the lookup by exact, case-sensitive name (the root is not a
 * subcommand). A NULL program or name reads as empty.
 */
NAOS_TEST(program_read_functions)
{
    clopts_command_t *root;
    clopts_command_t *commands[3];
    clopts_program_t *program;

    root = make_command("tool");
    commands[0] = make_command("copy");
    commands[1] = make_command("build");
    commands[2] = make_command("Build");
    program = make_program(root, commands, 3);

    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);
    NAOS_REQUIRE_UINT_EQ(clopts_program_command_count(program), 3);
    NAOS_CHECK_PTR_EQ(clopts_program_command_at(program, 0), commands[0]);
    NAOS_CHECK_PTR_EQ(clopts_program_command_at(program, 1), commands[1]);
    NAOS_CHECK_PTR_EQ(clopts_program_command_at(program, 2), commands[2]);

    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "copy"), commands[0]);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "build"), commands[1]);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "Build"), commands[2]);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "BUILD"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, "bui"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, "builds"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, "tool"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, ""));
    NAOS_CHECK_NULL(clopts_program_find_command(program, NULL));
    clopts_program_destroy(program);

    /* a program without subcommands */
    program = make_program(make_command("filt"), NULL, 0);
    NAOS_CHECK_STR_EQ(clopts_command_name(clopts_program_root(program)), "filt");
    NAOS_CHECK_UINT_EQ(clopts_program_command_count(program), 0);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "filt"));
    clopts_program_destroy(program);

    /* NULL */
    NAOS_CHECK_NULL(clopts_program_root(NULL));
    NAOS_CHECK_UINT_EQ(clopts_program_command_count(NULL), 0);
    NAOS_CHECK_NULL(clopts_program_find_command(NULL, "build"));
}

/*
 * The read functions of a command, unattached and attached alike: name,
 * texts, and the options and positionals in order of attachment. A NULL
 * command reads as empty.
 */
NAOS_TEST(command_read_functions)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *force;
    clopts_positional_t *src;
    clopts_positional_t *dst;
    clopts_program_t *program;
    int pass;

    root = make_command("tool");
    build = make_command("build");
    NAOS_CHECK_STR_EQ(clopts_command_name(build), "build");
    NAOS_CHECK_NULL(clopts_command_summary(build));
    NAOS_CHECK_NULL(clopts_command_description(build));
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(build), 0);
    NAOS_CHECK_UINT_EQ(clopts_command_positional_count(build), 0);

    EXPECT_OK(clopts_command_set_summary(build, "Builds."));
    EXPECT_OK(clopts_command_set_description(build, "Builds the tree."));
    EXPECT_OK(clopts_command_set_description(root, "A tool."));
    output = test_decl_add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    force = test_decl_add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    src = test_decl_add_positional(build, "src");
    dst = test_decl_add_positional(build, "dst");
    NAOS_REQUIRE_NOT_NULL(output);
    NAOS_REQUIRE_NOT_NULL(force);
    NAOS_REQUIRE_NOT_NULL(src);
    NAOS_REQUIRE_NOT_NULL(dst);

    program = NULL;
    for (pass = 0; pass < 2; pass++)
    {
        NAOS_NOTE("pass %d (%s)", pass, (pass == 0) ? "unattached" : "attached");
        NAOS_CHECK_STR_EQ(clopts_command_name(build), "build");
        check_str_or_null(clopts_command_summary(build), "Builds.");
        check_str_or_null(clopts_command_description(build), "Builds the tree.");
        NAOS_REQUIRE_UINT_EQ(clopts_command_option_count(build), 2);
        NAOS_CHECK_PTR_EQ(clopts_command_option_at(build, 0), output);
        NAOS_CHECK_PTR_EQ(clopts_command_option_at(build, 1), force);
        NAOS_REQUIRE_UINT_EQ(clopts_command_positional_count(build), 2);
        NAOS_CHECK_PTR_EQ(clopts_command_positional_at(build, 0), src);
        NAOS_CHECK_PTR_EQ(clopts_command_positional_at(build, 1), dst);
        NAOS_CHECK_STR_EQ(clopts_command_name(root), "tool");
        check_str_or_null(clopts_command_description(root), "A tool.");
        NAOS_CHECK_NULL(clopts_command_summary(root));
        NAOS_CHECK_UINT_EQ(clopts_command_option_count(root), 0);
        if (pass == 0)
        {
            program = make_program(root, &build, 1);
        }
    }
    NAOS_CHECK_PTR_EQ(clopts_program_command_at(program, 0), build);
    clopts_program_destroy(program);

    /* NULL */
    NAOS_CHECK_NULL(clopts_command_name(NULL));
    NAOS_CHECK_NULL(clopts_command_summary(NULL));
    NAOS_CHECK_NULL(clopts_command_description(NULL));
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(NULL), 0);
    NAOS_CHECK_UINT_EQ(clopts_command_positional_count(NULL), 0);
}

/*
 * The read functions of an option give back every attribute, before and
 * after the attach. Attributes not set read as NULL, '\0', 0 or false.
 */
NAOS_TEST(option_read_functions)
{
    clopts_command_t *root;
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *count;
    clopts_option_t *flag;
    clopts_option_t *bare;
    clopts_program_t *program;
    int pass;

    root = make_command("tool");
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(value, "Output file."));
    EXPECT_OK(clopts_option_set_value_name(value, "FILE"));
    EXPECT_OK(clopts_option_set_env(value, "TOOL_OUTPUT"));
    EXPECT_OK(clopts_option_set_default(value, "a.out"));
    list = make(CLOPTS_OPTION_LIST, "include", 'I');
    EXPECT_OK(clopts_option_set_delimiter(list, ','));
    EXPECT_OK(clopts_option_set_required(list, true));
    count = make(CLOPTS_OPTION_COUNT, NULL, 'v');
    EXPECT_OK(clopts_option_set_count_max(count, 3));
    flag = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(flag, true));
    EXPECT_OK(clopts_option_set_terminal(flag, true));
    bare = make(CLOPTS_OPTION_VALUE, "bare", '\0');

    program = NULL;
    for (pass = 0; pass < 2; pass++)
    {
        NAOS_NOTE("pass %d (%s)", pass, (pass == 0) ? "unattached" : "attached");
        NAOS_CHECK_INT_EQ(clopts_option_kind_of(value), CLOPTS_OPTION_VALUE);
        check_str_or_null(clopts_option_long_name(value), "output");
        NAOS_CHECK_INT_EQ(clopts_option_short_name(value), 'o');
        check_str_or_null(clopts_option_help(value), "Output file.");
        check_str_or_null(clopts_option_value_name(value), "FILE");
        check_str_or_null(clopts_option_env(value), "TOOL_OUTPUT");
        check_str_or_null(clopts_option_default(value), "a.out");
        NAOS_CHECK_INT_EQ(clopts_option_delimiter(value), '\0');
        NAOS_CHECK_UINT_EQ(clopts_option_count_max(value), 0);
        NAOS_CHECK_FALSE(clopts_option_required(value));
        NAOS_CHECK_FALSE(clopts_option_negatable(value));
        NAOS_CHECK_FALSE(clopts_option_terminal(value));

        NAOS_CHECK_INT_EQ(clopts_option_kind_of(list), CLOPTS_OPTION_LIST);
        NAOS_CHECK_INT_EQ(clopts_option_short_name(list), 'I');
        NAOS_CHECK_INT_EQ(clopts_option_delimiter(list), ',');
        NAOS_CHECK_TRUE(clopts_option_required(list));
        NAOS_CHECK_NULL(clopts_option_default(list));

        NAOS_CHECK_INT_EQ(clopts_option_kind_of(count), CLOPTS_OPTION_COUNT);
        NAOS_CHECK_NULL(clopts_option_long_name(count));
        NAOS_CHECK_INT_EQ(clopts_option_short_name(count), 'v');
        NAOS_CHECK_UINT_EQ(clopts_option_count_max(count), 3);

        NAOS_CHECK_INT_EQ(clopts_option_kind_of(flag), CLOPTS_OPTION_FLAG);
        NAOS_CHECK_INT_EQ(clopts_option_short_name(flag), '\0');
        NAOS_CHECK_TRUE(clopts_option_negatable(flag));
        NAOS_CHECK_TRUE(clopts_option_terminal(flag));
        NAOS_CHECK_NULL(clopts_option_help(flag));

        NAOS_CHECK_NULL(clopts_option_help(bare));
        NAOS_CHECK_NULL(clopts_option_value_name(bare));
        NAOS_CHECK_NULL(clopts_option_env(bare));
        NAOS_CHECK_NULL(clopts_option_default(bare));
        if (pass == 0)
        {
            NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, value));
            NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, list));
            NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, count));
            NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, flag));
            NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, bare));
            program = make_program(root, NULL, 0);
        }
    }
    NAOS_CHECK_PTR_EQ(clopts_command_option_at(clopts_program_root(program), 3), flag);
    clopts_program_destroy(program);
}

/* The read functions of a positional, before and after the attach; a NULL positional reads as empty. */
NAOS_TEST(positional_read_functions)
{
    clopts_command_t *root;
    clopts_positional_t *src;
    clopts_positional_t *rest;
    clopts_program_t *program;
    int pass;

    root = make_command("tool");
    src = make_positional("SRC");
    EXPECT_OK(clopts_positional_set_help(src, "Source."));
    clopts_positional_set_required(src, true);
    rest = make_positional("REST");
    clopts_positional_set_variadic(rest, true);

    program = NULL;
    for (pass = 0; pass < 2; pass++)
    {
        NAOS_NOTE("pass %d (%s)", pass, (pass == 0) ? "unattached" : "attached");
        check_str_or_null(clopts_positional_name(src), "SRC");
        check_str_or_null(clopts_positional_help(src), "Source.");
        NAOS_CHECK_TRUE(clopts_positional_required(src));
        NAOS_CHECK_FALSE(clopts_positional_variadic(src));
        check_str_or_null(clopts_positional_name(rest), "REST");
        NAOS_CHECK_NULL(clopts_positional_help(rest));
        NAOS_CHECK_FALSE(clopts_positional_required(rest));
        NAOS_CHECK_TRUE(clopts_positional_variadic(rest));
        if (pass == 0)
        {
            NAOS_REQUIRE_NOT_NULL(test_decl_attach_positional(root, src));
            NAOS_REQUIRE_NOT_NULL(test_decl_attach_positional(root, rest));
            program = make_program(root, NULL, 0);
        }
    }
    clopts_program_destroy(program);

    NAOS_CHECK_NULL(clopts_positional_name(NULL));
    NAOS_CHECK_NULL(clopts_positional_help(NULL));
    NAOS_CHECK_FALSE(clopts_positional_required(NULL));
    NAOS_CHECK_FALSE(clopts_positional_variadic(NULL));
}

/* ---- terminal ------------------------------------------------------------------ */

/*
 * clopts_option_set_terminal(): FLAG only (ATTRIBUTE_KIND for the other
 * kinds, while false resets on any kind).
 */
NAOS_TEST(set_terminal_is_for_flags)
{
    static const clopts_option_kind_t others[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_command_t *root;
    clopts_option_t *flag;
    clopts_option_t *option;
    clopts_program_t *program;
    size_t i;

    root = make_command("tool");
    for (i = 0; i < sizeof(others) / sizeof(others[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)others[i]);
        option = make(others[i], "other", 'o');
        NAOS_CHECK_INT_EQ(clopts_option_set_terminal(option, true), CLOPTS_ERROR_ATTRIBUTE_KIND);
        NAOS_CHECK_FALSE(clopts_option_terminal(option));
        EXPECT_OK(clopts_option_set_terminal(option, false));
        NAOS_CHECK_FALSE(clopts_option_terminal(option));
        if (i == 0)
        {
            NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, option));
        }
        else
        {
            clopts_option_destroy(option);
        }
    }

    flag = make(CLOPTS_OPTION_FLAG, "help", 'h');
    EXPECT_OK(clopts_option_set_terminal(flag, true));
    NAOS_CHECK_TRUE(clopts_option_terminal(flag));
    EXPECT_OK(clopts_option_set_terminal(flag, true));
    NAOS_CHECK_TRUE(clopts_option_terminal(flag));
    EXPECT_OK(clopts_option_set_terminal(flag, false));
    NAOS_CHECK_FALSE(clopts_option_terminal(flag));
    EXPECT_OK(clopts_option_set_terminal(flag, true));

    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, flag));
    program = make_program(root, NULL, 0);
    NAOS_CHECK_TRUE(clopts_option_terminal(flag));
    clopts_program_destroy(program);
}

/* ---- string copies --------------------------------------------------------- */

NAOS_TEST(strings_are_copied)
{
    char root_name[] = "tool";
    char cmd_name[] = "build";
    char long_name[] = "output";
    char pos_name[] = "src";
    char text[32];
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_positional_t *src;

    root = make_command(root_name);
    build = make_command(cmd_name);
    output = make(CLOPTS_OPTION_VALUE, long_name, 'o');
    src = make_positional(pos_name);
    memset(root_name, 'X', sizeof(root_name) - 1);
    memset(cmd_name, 'X', sizeof(cmd_name) - 1);
    memset(long_name, 'X', sizeof(long_name) - 1);
    memset(pos_name, 'X', sizeof(pos_name) - 1);

    strcpy(text, "Builds.");
    EXPECT_OK(clopts_command_set_summary(build, text));
    strcpy(text, "XXXXXXX");
    strcpy(text, "Builds the tree.");
    EXPECT_OK(clopts_command_set_description(build, text));
    strcpy(text, "XXXXXXXXXXXXXXXX");
    strcpy(text, "Program text.");
    EXPECT_OK(clopts_command_set_description(root, text));
    strcpy(text, "XXXXXXXXXXXXX");
    strcpy(text, "Output file.");
    EXPECT_OK(clopts_option_set_help(output, text));
    strcpy(text, "XXXXXXXXXXXX");
    strcpy(text, "FILE");
    EXPECT_OK(clopts_option_set_value_name(output, text));
    strcpy(text, "XXXX");
    strcpy(text, "TOOL_OUTPUT");
    EXPECT_OK(clopts_option_set_env(output, text));
    strcpy(text, "XXXXXXXXXXX");
    strcpy(text, "a.out");
    EXPECT_OK(clopts_option_set_default(output, text));
    strcpy(text, "XXXXX");
    strcpy(text, "Source.");
    EXPECT_OK(clopts_positional_set_help(src, text));
    strcpy(text, "XXXXXXX");

    NAOS_REQUIRE_NOT_NULL(test_decl_attach(build, output));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach_positional(build, src));
    program = make_program(root, &build, 1);

    NAOS_CHECK_STR_EQ(root->name, "tool");
    NAOS_CHECK_STR_EQ(build->name, "build");
    NAOS_CHECK_STR_EQ(clopts_option_long_name(output), "output");
    NAOS_CHECK_STR_EQ(clopts_positional_name(src), "src");
    NAOS_REQUIRE_NOT_NULL(build->summary);
    NAOS_CHECK_STR_EQ(build->summary, "Builds.");
    NAOS_REQUIRE_NOT_NULL(build->description);
    NAOS_CHECK_STR_EQ(build->description, "Builds the tree.");
    NAOS_REQUIRE_NOT_NULL(root->description);
    NAOS_CHECK_STR_EQ(root->description, "Program text.");
    NAOS_REQUIRE_NOT_NULL(clopts_option_help(output));
    NAOS_CHECK_STR_EQ(clopts_option_help(output), "Output file.");
    NAOS_REQUIRE_NOT_NULL(clopts_option_value_name(output));
    NAOS_CHECK_STR_EQ(clopts_option_value_name(output), "FILE");
    NAOS_REQUIRE_NOT_NULL(clopts_option_env(output));
    NAOS_CHECK_STR_EQ(clopts_option_env(output), "TOOL_OUTPUT");
    NAOS_REQUIRE_NOT_NULL(clopts_option_default(output));
    NAOS_CHECK_STR_EQ(clopts_option_default(output), "a.out");
    NAOS_REQUIRE_NOT_NULL(clopts_positional_help(src));
    NAOS_CHECK_STR_EQ(clopts_positional_help(src), "Source.");

    clopts_program_destroy(program);
}

/* A string attribute set from its own current copy is copied before the old copy is released. */
NAOS_TEST(string_set_from_its_own_copy)
{
    clopts_command_t *build;
    clopts_option_t *output;

    build = make_command("build");
    EXPECT_OK(clopts_command_set_summary(build, "Builds."));
    EXPECT_OK(clopts_command_set_summary(build, build->summary));
    NAOS_CHECK_STR_EQ(build->summary, "Builds.");
    EXPECT_OK(clopts_command_set_description(build, "Builds the tree."));
    EXPECT_OK(clopts_command_set_description(build, clopts_command_description(build)));
    NAOS_CHECK_STR_EQ(build->description, "Builds the tree.");
    clopts_command_destroy(build);

    output = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(output, "Output file."));
    EXPECT_OK(clopts_option_set_help(output, clopts_option_help(output)));
    NAOS_CHECK_STR_EQ(clopts_option_help(output), "Output file.");
    clopts_option_destroy(output);
}

/* ---- setting again --------------------------------------------------------- */

NAOS_TEST(setting_again_keeps_the_last_value)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *count;
    clopts_option_t *flag;
    clopts_positional_t *src;

    root = make_command("tool");
    build = make_command("build");
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    list = make(CLOPTS_OPTION_LIST, "include", 'I');
    count = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    flag = make(CLOPTS_OPTION_FLAG, "color", '\0');
    src = make_positional("src");

    EXPECT_OK(clopts_command_set_summary(build, "first"));
    EXPECT_OK(clopts_command_set_summary(build, "second"));
    EXPECT_OK(clopts_command_set_description(build, "first"));
    EXPECT_OK(clopts_command_set_description(build, "second"));
    EXPECT_OK(clopts_option_set_help(value, "first"));
    EXPECT_OK(clopts_option_set_help(value, "second"));
    EXPECT_OK(clopts_option_set_value_name(value, "A"));
    EXPECT_OK(clopts_option_set_value_name(value, "B"));
    EXPECT_OK(clopts_option_set_env(value, "ENV_A"));
    EXPECT_OK(clopts_option_set_env(value, "ENV_B"));
    EXPECT_OK(clopts_option_set_default(value, "x"));
    EXPECT_OK(clopts_option_set_default(value, "y"));
    EXPECT_OK(clopts_option_set_delimiter(list, ','));
    EXPECT_OK(clopts_option_set_delimiter(list, ':'));
    EXPECT_OK(clopts_option_set_count_max(count, 3));
    EXPECT_OK(clopts_option_set_count_max(count, 8));
    EXPECT_OK(clopts_option_set_negatable(flag, true));
    EXPECT_OK(clopts_option_set_terminal(flag, true));
    EXPECT_OK(clopts_option_set_required(list, true));
    EXPECT_OK(clopts_positional_set_help(src, "first"));
    EXPECT_OK(clopts_positional_set_help(src, "second"));
    clopts_positional_set_required(src, true);
    clopts_positional_set_variadic(src, true);

    NAOS_CHECK_STR_EQ(build->summary, "second");
    NAOS_CHECK_STR_EQ(build->description, "second");
    NAOS_CHECK_STR_EQ(clopts_option_help(value), "second");
    NAOS_CHECK_STR_EQ(clopts_option_value_name(value), "B");
    NAOS_CHECK_STR_EQ(clopts_option_env(value), "ENV_B");
    NAOS_CHECK_STR_EQ(clopts_option_default(value), "y");
    NAOS_CHECK_INT_EQ(clopts_option_delimiter(list), ':');
    NAOS_CHECK_UINT_EQ(clopts_option_count_max(count), 8);
    NAOS_CHECK_TRUE(clopts_option_negatable(flag));
    NAOS_CHECK_TRUE(clopts_option_terminal(flag));
    NAOS_CHECK_TRUE(clopts_option_required(list));
    NAOS_CHECK_STR_EQ(clopts_positional_help(src), "second");
    NAOS_CHECK_TRUE(clopts_positional_required(src));
    NAOS_CHECK_TRUE(clopts_positional_variadic(src));

    /* NULL, false, '\0' and 0 reset */
    EXPECT_OK(clopts_command_set_summary(build, NULL));
    EXPECT_OK(clopts_command_set_description(build, NULL));
    EXPECT_OK(clopts_option_set_help(value, NULL));
    EXPECT_OK(clopts_option_set_value_name(value, NULL));
    EXPECT_OK(clopts_option_set_env(value, NULL));
    EXPECT_OK(clopts_option_set_default(value, NULL));
    EXPECT_OK(clopts_option_set_delimiter(list, '\0'));
    EXPECT_OK(clopts_option_set_count_max(count, 0));
    EXPECT_OK(clopts_option_set_negatable(flag, false));
    EXPECT_OK(clopts_option_set_terminal(flag, false));
    EXPECT_OK(clopts_option_set_required(list, false));
    EXPECT_OK(clopts_positional_set_help(src, NULL));
    clopts_positional_set_variadic(src, false);
    clopts_positional_set_required(src, false);

    NAOS_CHECK_NULL(build->summary);
    NAOS_CHECK_NULL(build->description);
    NAOS_CHECK_NULL(clopts_option_help(value));
    NAOS_CHECK_NULL(clopts_option_value_name(value));
    NAOS_CHECK_NULL(clopts_option_env(value));
    NAOS_CHECK_NULL(clopts_option_default(value));
    NAOS_CHECK_INT_EQ(clopts_option_delimiter(list), '\0');
    NAOS_CHECK_UINT_EQ(clopts_option_count_max(count), 0);
    NAOS_CHECK_FALSE(clopts_option_negatable(flag));
    NAOS_CHECK_FALSE(clopts_option_terminal(flag));
    NAOS_CHECK_FALSE(clopts_option_required(list));
    NAOS_CHECK_NULL(clopts_positional_help(src));
    NAOS_CHECK_FALSE(clopts_positional_variadic(src));
    NAOS_CHECK_FALSE(clopts_positional_required(src));

    /* the attach and the program keep the attributes as they are */
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, value));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, list));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, count));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, flag));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach_positional(build, src));
    program = make_program(root, &build, 1);
    NAOS_CHECK_NULL(clopts_option_help(value));
    NAOS_CHECK_INT_EQ(clopts_option_delimiter(list), '\0');
    NAOS_CHECK_FALSE(clopts_option_terminal(flag));
    NAOS_CHECK_FALSE(clopts_positional_required(src));
    NAOS_CHECK_NULL(build->summary);

    clopts_program_destroy(program);
}

/* ---- handle stability ---------------------------------------------------- */

NAOS_TEST(hundred_options_keep_the_first_handle)
{
    char name[16];
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_option_t *first;
    clopts_option_t *option;
    clopts_positional_t *positional;
    int i;

    root = make_command("tool");

    first = make(CLOPTS_OPTION_VALUE, "o0", 'a');
    EXPECT_OK(clopts_option_set_help(first, "first help"));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, first));
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "o%d", i);
        NAOS_NOTE("i=%d", i);
        option = test_decl_add(root, CLOPTS_OPTION_FLAG, name, '\0');
        NAOS_REQUIRE_NOT_NULL(option);
        NAOS_CHECK_PTR_EQ(option_at(root, (size_t)i), option);
    }
    for (i = 0; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "p%d", i);
        NAOS_NOTE("i=%d", i);
        positional = test_decl_add_positional(root, name);
        NAOS_REQUIRE_NOT_NULL(positional);
    }
    program = make_program(root, NULL, 0);

    NAOS_REQUIRE_UINT_EQ(root->options.len, 100);
    NAOS_REQUIRE_UINT_EQ(root->positionals.len, 100);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), first);
    NAOS_CHECK_INT_EQ(clopts_option_kind_of(first), CLOPTS_OPTION_VALUE);
    NAOS_CHECK_STR_EQ(clopts_option_long_name(first), "o0");
    NAOS_CHECK_INT_EQ(clopts_option_short_name(first), 'a');
    NAOS_CHECK_STR_EQ(clopts_option_help(first), "first help");
    NAOS_CHECK_STR_EQ(clopts_option_long_name(option_at(root, 99)), "o99");
    NAOS_CHECK_STR_EQ(clopts_positional_name(positional_at(root, 99)), "p99");

    clopts_program_destroy(program);
}

NAOS_TEST(hundred_commands_keep_the_first_handle)
{
    char name[16];
    clopts_command_t *commands[100];
    clopts_program_t *program;
    clopts_command_t *first;
    int i;

    first = make_command("c0");
    EXPECT_OK(clopts_command_set_summary(first, "first summary"));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(first, CLOPTS_OPTION_FLAG, "force", 'f'));
    commands[0] = first;
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "c%d", i);
        NAOS_NOTE("i=%d", i);
        commands[i] = make_command(name);
    }
    program = make_program(make_command("tool"), commands, 100);

    NAOS_REQUIRE_UINT_EQ(program->command_count, 100);
    for (i = 0; i < 100; i++)
    {
        NAOS_NOTE("i=%d", i);
        NAOS_CHECK_PTR_EQ(command_at(program, (size_t)i), commands[i]);
    }
    NAOS_CHECK_PTR_EQ(command_at(program, 0), first);
    NAOS_CHECK_STR_EQ(first->name, "c0");
    NAOS_CHECK_STR_EQ(first->summary, "first summary");
    NAOS_CHECK_UINT_EQ(first->options.len, 1);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(program, "c99"), command_at(program, 99));
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "c50"), commands[50]);

    clopts_program_destroy(program);
}

/* ---- out of memory --------------------------------------------------------- */

/* One builder call under fault injection, with its context. */
typedef struct oom_call
{
    /* Makes the call. */
    clopts_error_t (*call)(struct oom_call *self);
    /* Checks after a failed call that nothing changed (may be NULL). */
    void (*unchanged)(struct oom_call *self);
    clopts_command_t *root;
    clopts_command_t *command;
    clopts_option_t *option;
    clopts_positional_t *positional;
    clopts_program_t *program_out;
    clopts_command_t *command_out;
    clopts_option_t *option_out;
    clopts_positional_t *positional_out;
} oom_call_t;

/*
 * Makes call->call() with its n-th fault point failing, for n = 1, 2, ...
 * until it succeeds. Each failing run must return OUT_OF_MEMORY, leave the
 * number of live blocks as it was, and pass call->unchanged(). Returns the
 * number of failing runs; the last run succeeds (failure is not sticky).
 */
static size_t fail_each_point(oom_call_t *call)
{
    clopts_error_t error;
    long live;
    size_t n;

    for (n = 1; n < 20; n++)
    {
        NAOS_NOTE("fault point %u", (unsigned)n);
        live = test_alloc.live;
        test_alloc_set_fail_at(test_alloc.calls + n);
        error = call->call(call);
        test_alloc_set_fail_at(0);
        if (error == CLOPTS_ERROR_NONE)
        {
            return n - 1;
        }
        NAOS_CHECK_INT_EQ(error, CLOPTS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_INT_EQ(test_alloc.live, live);
        if (call->unchanged != NULL)
        {
            call->unchanged(call);
        }
    }
    /*
     * The call never succeeded. Reported with a check that fails here (n is 20)
     * rather than NAOS_FAIL, which does not return and would make the return
     * unreachable (MSVC C4702).
     */
    NAOS_CHECK_TRUE(n < 20);
    return n;
}

static clopts_error_t oom_command_new(oom_call_t *c)
{
    c->command_out = &sentinel_command;
    return clopts_command_create("build", &c->command_out);
}

static void oom_command_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->command_out);
}

static clopts_error_t oom_set_summary(oom_call_t *c)
{
    return clopts_command_set_summary(c->command, "new");
}

static clopts_error_t oom_set_description(oom_call_t *c)
{
    return clopts_command_set_description(c->command, "new");
}

static void oom_command_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(c->command->summary, "old");
    NAOS_CHECK_STR_EQ(c->command->description, "old");
}

static clopts_error_t oom_option_new(oom_call_t *c)
{
    c->option_out = SENTINEL_OPTION;
    return clopts_option_create(CLOPTS_OPTION_VALUE, "output", 'o', &c->option_out);
}

static void oom_option_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->option_out);
}

static clopts_error_t oom_option_help(oom_call_t *c)
{
    return clopts_option_set_help(c->option, "new");
}

static clopts_error_t oom_option_value_name(oom_call_t *c)
{
    return clopts_option_set_value_name(c->option, "new");
}

static clopts_error_t oom_option_env(oom_call_t *c)
{
    return clopts_option_set_env(c->option, "new");
}

static clopts_error_t oom_option_default(oom_call_t *c)
{
    return clopts_option_set_default(c->option, "new");
}

static void oom_option_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(clopts_option_help(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_value_name(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_env(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_default(c->option), "old");
}

static clopts_error_t oom_add_option(oom_call_t *c)
{
    return clopts_command_add_option(c->command, c->option);
}

static void oom_add_option_unchanged(oom_call_t *c)
{
    NAOS_CHECK_UINT_EQ(c->command->options.len, 0);
}

static clopts_error_t oom_positional_new(oom_call_t *c)
{
    c->positional_out = SENTINEL_POSITIONAL;
    return clopts_positional_create("src", &c->positional_out);
}

static void oom_positional_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->positional_out);
}

static clopts_error_t oom_positional_help(oom_call_t *c)
{
    return clopts_positional_set_help(c->positional, "new");
}

static void oom_positional_help_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(clopts_positional_help(c->positional), "old");
}

static clopts_error_t oom_add_positional(oom_call_t *c)
{
    return clopts_command_add_positional(c->command, c->positional);
}

static void oom_add_positional_unchanged(oom_call_t *c)
{
    NAOS_CHECK_UINT_EQ(c->command->positionals.len, 0);
}

static clopts_error_t oom_program_new(oom_call_t *c)
{
    c->program_out = &sentinel_program;
    return clopts_program_create(c->root, &c->command, 1, &c->program_out);
}

static void oom_program_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->program_out);
}

/*
 * Every fault point of every allocating builder function: the call returns
 * OUT_OF_MEMORY, allocates nothing for good and changes nothing, and the same
 * call succeeds once allocation works again.
 */
NAOS_TEST(out_of_memory_changes_nothing)
{
    oom_call_t c;
    clopts_option_t *color;

    memset(&c, 0, sizeof(c));
    test_alloc_begin(0);

    c.call = oom_command_new;
    c.unchanged = oom_command_new_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_NOT_NULL(c.command_out);
    NAOS_REQUIRE_TRUE(c.command_out != &sentinel_command);
    c.command = c.command_out;

    EXPECT_OK(clopts_command_set_summary(c.command, "old"));
    EXPECT_OK(clopts_command_set_description(c.command, "old"));
    c.call = oom_set_summary;
    c.unchanged = oom_command_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    EXPECT_OK(clopts_command_set_summary(c.command, "old"));
    c.call = oom_set_description;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_STR_EQ(c.command->description, "new");

    c.call = oom_option_new;
    c.unchanged = oom_option_new_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_NOT_NULL(c.option_out);
    NAOS_REQUIRE_TRUE(c.option_out != SENTINEL_OPTION);
    c.option = c.option_out;

    EXPECT_OK(clopts_option_set_help(c.option, "old"));
    EXPECT_OK(clopts_option_set_value_name(c.option, "old"));
    EXPECT_OK(clopts_option_set_env(c.option, "old"));
    EXPECT_OK(clopts_option_set_default(c.option, "old"));
    c.unchanged = oom_option_unchanged;
    c.call = oom_option_help;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    EXPECT_OK(clopts_option_set_help(c.option, "old"));
    c.call = oom_option_value_name;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    EXPECT_OK(clopts_option_set_value_name(c.option, "old"));
    c.call = oom_option_env;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    EXPECT_OK(clopts_option_set_env(c.option, "old"));
    c.call = oom_option_default;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_STR_EQ(clopts_option_default(c.option), "new");

    c.call = oom_add_option;
    c.unchanged = oom_add_option_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_UINT_EQ(c.command->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(c.command, 0), c.option);

    /* the attach of a negatable flag allocates for its no-x lookup too */
    c.root = make_command("tool");
    color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    c.option = color;
    c.command = c.root;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    c.command = c.command_out;

    c.call = oom_positional_new;
    c.unchanged = oom_positional_new_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_NOT_NULL(c.positional_out);
    NAOS_REQUIRE_TRUE(c.positional_out != SENTINEL_POSITIONAL);
    c.positional = c.positional_out;

    EXPECT_OK(clopts_positional_set_help(c.positional, "old"));
    c.call = oom_positional_help;
    c.unchanged = oom_positional_help_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_STR_EQ(clopts_positional_help(c.positional), "new");

    c.call = oom_add_positional;
    c.unchanged = oom_add_positional_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_UINT_EQ(c.command->positionals.len, 1);

    /* the program and its command array: on failure the caller still owns root and command */
    c.call = oom_program_new;
    c.unchanged = oom_program_new_unchanged;
    NAOS_CHECK_UINT_EQ(fail_each_point(&c), 2);
    NAOS_REQUIRE_NOT_NULL(c.program_out);
    NAOS_REQUIRE_TRUE(c.program_out != &sentinel_program);
    NAOS_CHECK_PTR_EQ(clopts_program_command_at(c.program_out, 0), c.command);

    clopts_program_destroy(c.program_out);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Builds a small root and one subcommand through every allocating builder
 * function, with every call checked by test_decl_check() or made by a helper,
 * so that test_decl_failures counts the failed ones. The subcommand is
 * remembered for root (test_decl_finish() makes the program).
 */
static void build_sample(clopts_command_t *root)
{
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *include;
    clopts_option_t *color;
    clopts_positional_t *src;

    if (root != NULL)
    {
        test_decl_check(clopts_command_set_description(root, "A tool."));
    }
    test_decl_add(root, CLOPTS_OPTION_COUNT, "verbose", 'v');
    color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    if (color != NULL)
    {
        test_decl_check(clopts_option_set_negatable(color, true));
    }
    test_decl_attach(root, color);
    build = test_decl_command(root, "build");
    if (build != NULL)
    {
        test_decl_check(clopts_command_set_summary(build, "Builds."));
    }
    if (build != NULL)
    {
        test_decl_check(clopts_command_set_description(build, "Builds the tree."));
    }
    output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    if (output != NULL)
    {
        test_decl_check(clopts_option_set_help(output, "Output file."));
    }
    if (output != NULL)
    {
        test_decl_check(clopts_option_set_value_name(output, "FILE"));
    }
    if (output != NULL)
    {
        test_decl_check(clopts_option_set_env(output, "TOOL_OUTPUT"));
    }
    if (output != NULL)
    {
        test_decl_check(clopts_option_set_default(output, "a.out"));
    }
    test_decl_attach(build, output);
    include = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    if (include != NULL)
    {
        test_decl_check(clopts_option_set_delimiter(include, ','));
    }
    test_decl_attach(build, include);
    test_decl_add(build, CLOPTS_OPTION_FLAG, "release", '\0');
    src = test_decl_positional("src");
    if (src != NULL)
    {
        test_decl_check(clopts_positional_set_help(src, "Source."));
    }
    test_decl_attach_positional(build, src);
}

/*
 * Moves the failing fault point fail_at = 1, 2, ... over test_decl_root(),
 * build_sample() and test_decl_finish() until a run passes no failing fault
 * point (test_alloc.calls < fail_at). The first two fault points are in the
 * root's clopts_command_create(), which then gives NULL. A run that passes a
 * failing fault point has failed calls; whatever was made is released by
 * clopts_program_destroy(), by test_decl_finish() when the program could not be
 * made, or by the helper for a failed attach.
 */
NAOS_TEST(every_failure_point_is_released)
{
    clopts_command_t *root;
    clopts_program_t *program;
    size_t fail_at;
    bool succeeded = false;

    for (fail_at = 1; (fail_at < 200) && (!succeeded); fail_at++)
    {
        NAOS_NOTE("fail_at=%u", (unsigned)fail_at);
        test_alloc_begin(fail_at);
        test_decl_reset();
        root = test_decl_root("tool");
        if (root == NULL)
        {
            /* Only the first two fault points are in test_decl_root(). */
            NAOS_CHECK_UINT_LE(fail_at, 2);
            NAOS_CHECK_UINT_EQ(test_alloc.calls, fail_at);
            NAOS_CHECK_INT_EQ(test_decl_last, CLOPTS_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_TRUE(test_alloc_end());
            continue;
        }

        build_sample(root);
        program = test_decl_finish(root);
        if (test_alloc.calls < fail_at)
        {
            NAOS_CHECK_UINT_EQ(test_decl_failures, 0);
            NAOS_CHECK_NOT_NULL(program);
            succeeded = true;
        }
        else
        {
            NAOS_CHECK_UINT_GT(test_decl_failures, 0);
        }

        clopts_program_destroy(program);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(fail_at, 3);
}

/* ---- release --------------------------------------------------------------- */

/* clopts_program_destroy() releases every block: root, commands, attached options and positionals, and their
 * strings. */
NAOS_TEST(program_free_releases_everything)
{
    clopts_command_t *root;
    clopts_program_t *program;

    test_alloc_begin(0);
    test_decl_reset();
    root = test_decl_root("tool");
    NAOS_REQUIRE_NOT_NULL(root);
    build_sample(root);
    program = test_decl_finish(root);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    NAOS_CHECK_INT_GT(test_alloc.live, 1);

    clopts_program_destroy(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_UINT_EQ(test_alloc.bad_frees, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- parse ------------------------------------------------------------------- */

/*
 * A parse reads a program and never changes it (there is no freezing): the
 * same program can be parsed again, also after a parse ran out of memory,
 * and its handles and contents stay as they were. The program was read-only
 * already, since clopts_program_create().
 */
NAOS_TEST(parse_leaves_the_program_as_it_is)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_program_t *program;
    clopts_option_t *flag;

    test_alloc_begin(0);
    root = make_command("tool");
    build = make_command("build");
    flag = test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_REQUIRE_NOT_NULL(flag);
    program = make_program(root, &build, 1);

    parse_name_only(program, "tool", CLOPTS_NO_COMMAND);
    parse_name_only(program, "tool", CLOPTS_NO_COMMAND);
    test_alloc_set_fail_at(test_alloc.calls + 1);
    parse_name_only(program, "tool", CLOPTS_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    parse_name_only(program, "tool", CLOPTS_NO_COMMAND);

    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);
    NAOS_CHECK_UINT_EQ(clopts_program_command_count(program), 1);
    NAOS_CHECK_PTR_EQ(clopts_program_command_at(program, 0), build);
    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), flag);
    NAOS_CHECK_NULL(clopts_option_help(flag));

    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- shared fixtures (test_fixtures.c) ------------------------------------- */

/* Expected attributes of one option, as declared by the first-version fixtures. */
typedef struct expected_option
{
    const clopts_command_t *scope;
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

/* Checks every model field of option, and that it is attached at index of its scope. */
static void check_option(const clopts_option_t *option, const expected_option_t *expected)
{
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_REQUIRE_UINT_GT(expected->scope->options.len, expected->index);
    NAOS_CHECK_PTR_EQ(option_at(expected->scope, expected->index), option);
    NAOS_CHECK_INT_EQ(clopts_option_kind_of(option), expected->kind);
    check_str_or_null(clopts_option_long_name(option), expected->long_name);
    NAOS_CHECK_INT_EQ(clopts_option_short_name(option), expected->short_name);
    check_str_or_null(clopts_option_help(option), expected->help);
    check_str_or_null(clopts_option_value_name(option), expected->value_name);
    check_str_or_null(clopts_option_default(option), expected->default_value);
    check_str_or_null(clopts_option_env(option), expected->env);
    NAOS_CHECK_INT_EQ(clopts_option_delimiter(option), expected->delimiter);
    NAOS_CHECK_UINT_EQ(clopts_option_count_max(option), expected->count_max);
    NAOS_CHECK_INT_EQ(clopts_option_required(option), expected->required);
    NAOS_CHECK_INT_EQ(clopts_option_negatable(option), expected->negatable);
    NAOS_CHECK_INT_EQ(clopts_option_terminal(option), expected->terminal);
}

/* Checks the model fields of positional, and that it is attached at index of scope. */
static void check_positional(const clopts_positional_t *positional, const clopts_command_t *scope, size_t index,
                             const char *name, const char *help, bool required, bool variadic)
{
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_REQUIRE_UINT_GT(scope->positionals.len, index);
    NAOS_CHECK_PTR_EQ(positional_at(scope, index), positional);
    NAOS_CHECK_STR_EQ(clopts_positional_name(positional), name);
    NAOS_CHECK_STR_EQ(clopts_positional_help(positional), help);
    NAOS_CHECK_INT_EQ(clopts_positional_required(positional), required);
    NAOS_CHECK_INT_EQ(clopts_positional_variadic(positional), variadic);
}

/* Checks a subcommand: its place in program, its name and summary, and no description. */
static void check_command(const clopts_command_t *command, const clopts_program_t *program, size_t index,
                          const char *name, const char *summary)
{
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_PTR_EQ(command_at(program, index), command);
    NAOS_CHECK_STR_EQ(command->name, name);
    NAOS_CHECK_STR_EQ(command->summary, summary);
    NAOS_CHECK_NULL(command->description);
}

/* Checks the root of a fixture program: the program's root, its name and description, no summary. */
static void check_root(const clopts_command_t *root, const clopts_program_t *program, const char *name,
                       const char *description)
{
    NAOS_REQUIRE_NOT_NULL(root);
    NAOS_CHECK_PTR_EQ(program->root, root);
    NAOS_CHECK_STR_EQ(root->name, name);
    NAOS_CHECK_STR_EQ(root->description, description);
    NAOS_CHECK_NULL(root->summary);
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
    clopts_command_t *root;

    fixture_tool_init(&f);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(f.ok);

    root = f.root;
    check_root(root, program, "tool", "A fictional tool with subcommands.");
    NAOS_CHECK_UINT_EQ(root->options.len, 5);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 0);

    {
        const expected_option_t verbose = {root, 0, CLOPTS_OPTION_COUNT, "verbose", 'v', "Increase verbosity.",
                                           NULL, NULL, NULL, '\0', 0, false, false, false};
        const expected_option_t color = {root, 1, CLOPTS_OPTION_FLAG, "color", '\0', "Colorize the output.",
                                         NULL, NULL, NULL, '\0', 0, false, true, false};
        const expected_option_t config = {root, 2, CLOPTS_OPTION_VALUE, "config", '\0', "Configuration file.",
                                          "PATH", NULL, "TOOL_CONFIG", '\0', 0, false, false, false};
        const expected_option_t help = {root, 3, CLOPTS_OPTION_FLAG, "help", 'h', "Show this help",
                                        NULL, NULL, NULL, '\0', 0, false, false, true};
        const expected_option_t version = {root, 4, CLOPTS_OPTION_FLAG, "version", '\0', "Show version information",
                                           NULL, NULL, NULL, '\0', 0, false, false, true};

        check_option(f.verbose, &verbose);
        check_option(f.color, &color);
        check_option(f.config, &config);
        check_option(f.help, &help);
        check_option(f.version, &version);
    }

    NAOS_REQUIRE_UINT_EQ(program->command_count, 4);
    check_command(f.build, program, 0, "build", "Build a source.");
    check_command(f.copy, program, 1, "copy", "Copy a file.");
    check_command(f.cat, program, 2, "cat", "Show files.");
    check_command(f.help_command, program, 3, "help", "Show help for a command");

    NAOS_CHECK_UINT_EQ(f.build->options.len, 3);
    NAOS_CHECK_UINT_EQ(f.build->positionals.len, 2);
    {
        const expected_option_t output = {f.build, 0, CLOPTS_OPTION_VALUE, "output", 'o', "Output file.",
                                          "FILE", NULL, NULL, '\0', 0, true, false, false};
        const expected_option_t keys = {f.build, 1, CLOPTS_OPTION_LIST, "keys", 'k', "Keys to build.",
                                        "KEY", NULL, NULL, ',', 0, false, false, false};
        const expected_option_t jobs = {f.build, 2, CLOPTS_OPTION_COUNT, NULL, 'j', "Add a parallel job.",
                                        NULL, NULL, NULL, '\0', 8, false, false, false};

        check_option(f.output, &output);
        check_option(f.keys, &keys);
        check_option(f.jobs, &jobs);
    }
    check_positional(f.build_src, f.build, 0, "SRC", "Source to build.", true, false);
    check_positional(f.build_extra, f.build, 1, "EXTRA", "Extra inputs.", false, true);

    NAOS_CHECK_UINT_EQ(f.copy->options.len, 0);
    NAOS_CHECK_UINT_EQ(f.copy->positionals.len, 2);
    check_positional(f.copy_src, f.copy, 0, "SRC", "Source.", true, false);
    check_positional(f.copy_dst, f.copy, 1, "DST", "Destination.", true, false);

    NAOS_CHECK_UINT_EQ(f.cat->options.len, 1);
    NAOS_CHECK_UINT_EQ(f.cat->positionals.len, 1);
    {
        const expected_option_t tags = {f.cat, 0, CLOPTS_OPTION_LIST, "tags", '\0', "Tags to show.",
                                        "TAG", "a,b", "TOOL_TAGS", ',', 0, false, false, false};

        check_option(f.tags, &tags);
    }
    check_positional(f.cat_file, f.cat, 0, "FILE", "Files to show.", false, true);

    NAOS_CHECK_UINT_EQ(f.help_command->options.len, 0);
    NAOS_REQUIRE_UINT_EQ(f.help_command->positionals.len, 1);
    check_positional(positional_at(f.help_command, 0), f.help_command, 0, "COMMAND", "Command to show help for",
                     false, false);

    fixture_tool_free(&f);
}

NAOS_TEST(fixture_filt_declaration)
{
    fixture_filt_t f;
    clopts_program_t *program;
    clopts_command_t *root;

    fixture_filt_init(&f);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(f.ok);

    root = f.root;
    check_root(root, program, "filt", "A fictional filter without subcommands.");
    NAOS_CHECK_UINT_EQ(program->command_count, 0);
    NAOS_CHECK_UINT_EQ(root->options.len, 2);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 1);
    {
        const expected_option_t lines = {root, 0, CLOPTS_OPTION_VALUE, "lines", 'n', "Number of lines.",
                                         "N", "10", "FILT_LINES", '\0', 0, false, false, false};
        const expected_option_t help = {root, 1, CLOPTS_OPTION_FLAG, "help", 'h', "Show this help",
                                        NULL, NULL, NULL, '\0', 0, false, false, true};

        check_option(f.lines, &lines);
        check_option(f.help, &help);
    }
    check_positional(f.file, root, 0, "FILE", "Files to filter.", false, true);

    fixture_filt_free(&f);
}

NAOS_TEST(fixture_strict_declaration)
{
    fixture_strict_t f;
    clopts_program_t *program;
    clopts_command_t *root;

    fixture_strict_init(&f);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(f.ok);

    root = f.root;
    check_root(root, program, "strict", "A fictional program with a required global option.");
    NAOS_CHECK_UINT_EQ(root->options.len, 2);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 0);
    {
        const expected_option_t token = {root, 0, CLOPTS_OPTION_VALUE, "token", '\0', "Access token.",
                                         "TOKEN", NULL, NULL, '\0', 0, true, false, false};
        const expected_option_t help = {root, 1, CLOPTS_OPTION_FLAG, "help", 'h', "Show this help",
                                        NULL, NULL, NULL, '\0', 0, false, false, true};

        check_option(f.token, &token);
        check_option(f.help, &help);
    }

    NAOS_REQUIRE_UINT_EQ(program->command_count, 1);
    check_command(f.run, program, 0, "run", "Run it.");
    NAOS_CHECK_UINT_EQ(f.run->options.len, 0);
    NAOS_CHECK_UINT_EQ(f.run->positionals.len, 0);

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
    NAOS_CHECK_NULL(tool->root);
    NAOS_CHECK_NULL(tool->build);
    NAOS_CHECK_NULL(tool->help_command);
    NAOS_CHECK_NULL(tool->verbose);
    NAOS_CHECK_NULL(tool->help);
}

/*
 * A failing fault point fails every later one too (test_alloc.h), so the
 * clopts_program_create() at the end of the assembly fails whichever fault
 * point fails first: no program is made, everything made so far is released
 * (by the helpers and test_decl_finish()), and the fixture is cleared with ok
 * false. The first two fault points are in the root's clopts_command_create(),
 * the last two in clopts_program_create() (the program and its command array).
 */
NAOS_TEST(fixture_out_of_memory_is_reported_and_released)
{
    fixture_tool_t tool;
    size_t total;
    size_t fail_at;

    /* number of fault points of a full assembly, clopts_program_create() included */
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
        check_tool_cleared(&tool);
        NAOS_CHECK_UINT_GE(test_alloc.calls, fail_at);
        if (fail_at >= total - 1)
        {
            /* only clopts_program_create() failed, and it stopped at its failing fault point */
            NAOS_CHECK_UINT_EQ(test_alloc.calls, fail_at);
        }
        fixture_tool_free(&tool);
        NAOS_CHECK_TRUE(test_alloc_end());
    }

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

NAOS_SUITE(build,
    NAOS_ADD_TEST(free_null_does_nothing),
    NAOS_ADD_TEST(command_new_copies_name_and_starts_empty),
    NAOS_ADD_TEST(command_new_checks_name),
    NAOS_ADD_TEST(new_option_and_positional_start_reset_and_unattached),
    NAOS_ADD_TEST(unattached_objects_are_released_by_their_free),
    NAOS_ADD_TEST(attach_links_objects_in_order),
    NAOS_ADD_TEST(root_takes_positionals),
    NAOS_ADD_TEST(program_new_takes_root_and_commands),
    NAOS_ADD_TEST(program_new_duplicate_command_changes_nothing),
    NAOS_ADD_TEST(program_new_mixed_layout_changes_nothing),
    NAOS_ADD_TEST(program_new_reports_the_first_error),
    NAOS_ADD_TEST(program_new_does_not_check_names_between_commands),
    NAOS_ADD_TEST(program_read_functions),
    NAOS_ADD_TEST(command_read_functions),
    NAOS_ADD_TEST(option_read_functions),
    NAOS_ADD_TEST(positional_read_functions),
    NAOS_ADD_TEST(set_terminal_is_for_flags),
    NAOS_ADD_TEST(strings_are_copied),
    NAOS_ADD_TEST(string_set_from_its_own_copy),
    NAOS_ADD_TEST(setting_again_keeps_the_last_value),
    NAOS_ADD_TEST(hundred_options_keep_the_first_handle),
    NAOS_ADD_TEST(hundred_commands_keep_the_first_handle),
    NAOS_ADD_TEST(out_of_memory_changes_nothing),
    NAOS_ADD_TEST(every_failure_point_is_released),
    NAOS_ADD_TEST(program_free_releases_everything),
    NAOS_ADD_TEST(parse_leaves_the_program_as_it_is),
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
