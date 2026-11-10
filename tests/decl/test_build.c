/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite build: the builder functions, the releases, string copies, the
 * last-call-wins and reset rules, handle stability, out of memory in every
 * allocating builder function, the program builder (its creation by name, its
 * description, and the attach of global options, positionals and subcommands
 * with their errors and their order) and the read functions. The contents
 * of the shared fixtures of the parser tests are checked in the fixtures
 * suite (tests/parser/test_fixture_checks.c).
 *
 * A program is created by name and filled: global options and positionals are
 * attached to it, and subcommands, made and filled on their own, are added to
 * it. Every builder function returns its error right away and a failing call
 * changes nothing; the caller still owns what it passed.
 * The declaration rules within a command (and the NULL arguments of the
 * option and positional setters) are tested in the decl
 * suites. Stored values are read through the internal model or the public
 * read functions, and the fault allocator (test_alloc.h) shows which blocks
 * are alive.
 */

#include "clopts/parse.h"
#include "clopts/decl/program.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

/* Checks that call returns CLOPTS_DECL_ERROR_NONE. */
#define EXPECT_OK(call) NAOS_CHECK_INT_EQ((call), CLOPTS_DECL_ERROR_NONE)

/*
 * Objects whose address an out pointer holds before a call, so that a test
 * can see the call set it to NULL. They are never read or written.
 */
static char sentinel_program_storage;
#define SENTINEL_PROGRAM ((clopts_program_t *)&sentinel_program_storage)
static char sentinel_command_storage;
#define SENTINEL_COMMAND ((clopts_command_t *)&sentinel_command_storage)
static char sentinel_option_storage;
#define SENTINEL_OPTION ((clopts_option_t *)&sentinel_option_storage)
static char sentinel_positional_storage;
#define SENTINEL_POSITIONAL ((clopts_positional_t *)&sentinel_positional_storage)

/* Returns command index of program. */
static const clopts_command_t *command_at(const clopts_program_t *program, size_t index)
{
    NAOS_REQUIRE_UINT_GT(clopts_program_get_command_count(program), index);
    return clopts_program_get_command_at(program, index);
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

/* A new unattached command; the test stops when it cannot be made. */
static clopts_command_t *make_command(const char *name)
{
    clopts_command_t *command = SENTINEL_COMMAND;

    NAOS_REQUIRE_INT_EQ(clopts_command_create(name, &command), CLOPTS_DECL_ERROR_NONE);
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_REQUIRE_TRUE(command != SENTINEL_COMMAND);
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

/* clopts_program_create(name); the test stops when it fails. */
static clopts_program_t *make_program(const char *name)
{
    clopts_program_t *program = SENTINEL_PROGRAM;

    NAOS_REQUIRE_INT_EQ(clopts_program_create(name, &program), CLOPTS_DECL_ERROR_NONE);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(program != SENTINEL_PROGRAM);
    return program;
}

/* clopts_program_add_command(program, command); the test stops when it fails. */
static void add_command(clopts_program_t *program, clopts_command_t *command)
{
    NAOS_REQUIRE_INT_EQ(clopts_program_add_command(program, command), CLOPTS_DECL_ERROR_NONE);
}

/* The global option index of program; the test stops when there is none. */
static const clopts_option_t *global_at(const clopts_program_t *program, size_t index)
{
    NAOS_REQUIRE_UINT_GT(clopts_scope_get_option_count(clopts_program_get_scope(program)), index);
    return clopts_scope_get_option_at(clopts_program_get_scope(program), index);
}

/* The number of live blocks and fault points passed, to check that a call allocated nothing. */
typedef struct alloc_mark
{
    long live;
    size_t calls;
} alloc_mark_t;

static alloc_mark_t alloc_mark(void)
{
    alloc_mark_t mark;

    mark.live = test_alloc.live;
    mark.calls = test_alloc.calls;
    return mark;
}

/* Checks that nothing was allocated or released since mark (the checks come before every allocation). */
static void check_nothing_allocated(alloc_mark_t mark)
{
    NAOS_CHECK_INT_EQ(test_alloc.live, mark.live);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, mark.calls);
}

/*
 * The global options, positionals and subcommands of program, to check that a
 * failing call changed nothing. Up to SHAPE_MAX of each are recorded.
 */
#define SHAPE_MAX 4
typedef struct program_shape
{
    size_t option_count;
    size_t positional_count;
    size_t command_count;
    const clopts_option_t *options[SHAPE_MAX];
    const clopts_positional_t *positionals[SHAPE_MAX];
    const clopts_command_t *commands[SHAPE_MAX];
} program_shape_t;

static program_shape_t program_shape(const clopts_program_t *program)
{
    program_shape_t shape;
    size_t i;

    memset(&shape, 0, sizeof(shape));
    shape.option_count = clopts_scope_get_option_count(clopts_program_get_scope(program));
    shape.positional_count = clopts_scope_get_positional_count(clopts_program_get_scope(program));
    shape.command_count = clopts_program_get_command_count(program);
    for (i = 0; (i < shape.option_count) && (i < SHAPE_MAX); i++)
    {
        shape.options[i] = clopts_scope_get_option_at(clopts_program_get_scope(program), i);
    }
    for (i = 0; (i < shape.positional_count) && (i < SHAPE_MAX); i++)
    {
        shape.positionals[i] = clopts_scope_get_positional_at(clopts_program_get_scope(program), i);
    }
    for (i = 0; (i < shape.command_count) && (i < SHAPE_MAX); i++)
    {
        shape.commands[i] = clopts_program_get_command_at(program, i);
    }
    return shape;
}

/* Checks that program still has the options, positionals and subcommands of shape, in the same order. */
static void check_shape(const clopts_program_t *program, const program_shape_t *shape)
{
    program_shape_t now = program_shape(program);
    size_t i;

    NAOS_CHECK_UINT_EQ(now.option_count, shape->option_count);
    NAOS_CHECK_UINT_EQ(now.positional_count, shape->positional_count);
    NAOS_CHECK_UINT_EQ(now.command_count, shape->command_count);
    for (i = 0; i < SHAPE_MAX; i++)
    {
        NAOS_CHECK_PTR_EQ(now.options[i], shape->options[i]);
        NAOS_CHECK_PTR_EQ(now.positionals[i], shape->positionals[i]);
        NAOS_CHECK_PTR_EQ(now.commands[i], shape->commands[i]);
    }
}

/*
 * Calls clopts_program_add_option(program, option) expecting error, and checks
 * that nothing was allocated and program did not change. The caller still
 * owns option and releases it.
 */
static void expect_add_option_fails(clopts_program_t *program, clopts_option_t *option, clopts_decl_error_t error)
{
    program_shape_t shape = program_shape(program);
    alloc_mark_t mark = alloc_mark();

    NAOS_CHECK_INT_EQ(clopts_program_add_option(program, option), error);
    check_nothing_allocated(mark);
    check_shape(program, &shape);
}

/* As expect_add_option_fails(), with clopts_program_add_positional(). */
static void expect_add_positional_fails(clopts_program_t *program, clopts_positional_t *positional,
                                        clopts_decl_error_t error)
{
    program_shape_t shape = program_shape(program);
    alloc_mark_t mark = alloc_mark();

    NAOS_CHECK_INT_EQ(clopts_program_add_positional(program, positional), error);
    check_nothing_allocated(mark);
    check_shape(program, &shape);
}

/* As expect_add_option_fails(), with clopts_program_add_command(). */
static void expect_add_command_fails(clopts_program_t *program, clopts_command_t *command, clopts_decl_error_t error)
{
    program_shape_t shape = program_shape(program);
    alloc_mark_t mark = alloc_mark();

    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, command), error);
    check_nothing_allocated(mark);
    check_shape(program, &shape);
}

/* Parses argv = { name } with program, checks the status, and releases the result. */
static void parse_name_only(const clopts_program_t *program, const char *name, clopts_parse_status_t status)
{
    char arg0[16];
    char *argv[2];
    clopts_result_t *result;

    (void)snprintf(arg0, sizeof(arg0), "%s", name);
    argv[0] = arg0;
    argv[1] = NULL;
    result = clopts_parse(program, 1, argv);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), status);
    clopts_result_destroy(result);
}

/* ---- NULL ------------------------------------------------------------------ */

NAOS_TEST(free_null_does_nothing)
{
    clopts_program_destroy(NULL);
    clopts_command_destroy(NULL);
    clopts_option_destroy(NULL);
    clopts_positional_destroy(NULL);
    NAOS_PASS();
}

/* ---- commands ---------------------------------------------------------------- */

/* clopts_command_create() copies the name and starts with no texts, options or positionals, unattached. */
NAOS_TEST(command_new_copies_name_and_starts_empty)
{
    char name[] = "build";
    clopts_command_t *command = SENTINEL_COMMAND;

    EXPECT_OK(clopts_command_create(name, &command));
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_REQUIRE_TRUE(command != SENTINEL_COMMAND);
    name[0] = 'X';

    NAOS_REQUIRE_NOT_NULL(clopts_command_get_name(command));
    NAOS_CHECK_TRUE(clopts_command_get_name(command) != name);
    NAOS_CHECK_STR_EQ(clopts_command_get_name(command), "build");
    NAOS_CHECK_NULL(clopts_command_get_summary(command));
    NAOS_CHECK_NULL(clopts_command_get_description(command));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(command)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(command)), 0);
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
    command = SENTINEL_COMMAND;
    NAOS_CHECK_INT_EQ(clopts_command_create(NULL, &command), CLOPTS_DECL_ERROR_INVALID_NAME);
    NAOS_CHECK_NULL(command);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", bad[i]);
        command = SENTINEL_COMMAND;
        NAOS_CHECK_INT_EQ(clopts_command_create(bad[i], &command), CLOPTS_DECL_ERROR_INVALID_NAME);
        NAOS_CHECK_NULL(command);
    }
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 0);

    for (i = 0; i < sizeof(good) / sizeof(good[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", good[i]);
        command = SENTINEL_COMMAND;
        EXPECT_OK(clopts_command_create(good[i], &command));
        NAOS_REQUIRE_NOT_NULL(command);
        NAOS_REQUIRE_TRUE(command != SENTINEL_COMMAND);
        NAOS_CHECK_STR_EQ(clopts_command_get_name(command), good[i]);
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
        NAOS_CHECK_INT_EQ(clopts_option_get_kind(option), kinds[i]);
        NAOS_CHECK_STR_EQ(clopts_option_get_long_name(option), "output");
        NAOS_CHECK_TRUE(clopts_option_get_long_name(option) != long_name);
        NAOS_CHECK_INT_EQ(clopts_option_get_short_name(option), 'o');
        NAOS_CHECK_NULL(clopts_option_get_help(option));
        NAOS_CHECK_NULL(clopts_option_get_value_name(option));
        NAOS_CHECK_NULL(clopts_option_get_default(option));
        NAOS_CHECK_NULL(clopts_option_get_env(option));
        NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(option), '\0');
        NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(option), 0);
        NAOS_CHECK_FALSE(clopts_option_is_required(option));
        NAOS_CHECK_FALSE(clopts_option_is_negatable(option));
        NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
        clopts_option_destroy(option);
    }

    option = make(CLOPTS_OPTION_VALUE, NULL, 'o');
    NAOS_CHECK_NULL(clopts_option_get_long_name(option));
    clopts_option_destroy(option);
    option = make(CLOPTS_OPTION_FLAG, "force", '\0');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(option), '\0');
    clopts_option_destroy(option);

    positional = SENTINEL_POSITIONAL;
    EXPECT_OK(clopts_positional_create(pos_name, &positional));
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_REQUIRE_TRUE(positional != SENTINEL_POSITIONAL);
    pos_name[0] = 'X';
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(positional), "src");
    NAOS_CHECK_NULL(clopts_positional_get_help(positional));
    NAOS_CHECK_FALSE(clopts_positional_is_required(positional));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(positional));
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
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *flag;
    clopts_option_t *count;
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *local;
    clopts_positional_t *src;
    clopts_positional_t *dst;

    program = test_decl_begin("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    flag = test_decl_global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    count = test_decl_global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    value = test_decl_global_add(program, CLOPTS_OPTION_VALUE, NULL, 'o');
    list = test_decl_global_add(program, CLOPTS_OPTION_LIST, "include", '\0');
    build = test_decl_command(program, "build");
    copy = test_decl_command(program, "copy");
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

    NAOS_REQUIRE_PTR_EQ(test_decl_finish(program), program);

    /* global options in order of attachment */
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), flag);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 1), count);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 2), value);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 3), list);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);

    NAOS_CHECK_INT_EQ(clopts_option_get_kind(flag), CLOPTS_OPTION_FLAG);
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(count), CLOPTS_OPTION_COUNT);
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(value), CLOPTS_OPTION_VALUE);
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(list), CLOPTS_OPTION_LIST);
    NAOS_REQUIRE_NOT_NULL(clopts_option_get_long_name(flag));
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(flag), "force");
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(flag), 'f');
    NAOS_CHECK_NULL(clopts_option_get_long_name(value));
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(value), 'o');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(list), '\0');

    /* commands in the order they were added */
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 2);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), build);
    NAOS_CHECK_PTR_EQ(command_at(program, 1), copy);
    NAOS_REQUIRE_NOT_NULL(clopts_command_get_name(build));
    NAOS_CHECK_STR_EQ(clopts_command_get_name(build), "build");
    NAOS_CHECK_STR_EQ(clopts_command_get_name(copy), "copy");
    NAOS_CHECK_NULL(clopts_command_get_summary(build));
    NAOS_CHECK_NULL(clopts_command_get_description(build));

    /* command options and positionals */
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 1);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_command_get_scope(build), 0), local);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(build)), 0);

    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(copy)), 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_command_get_scope(copy), 0), src);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_command_get_scope(copy), 1), dst);
    NAOS_REQUIRE_NOT_NULL(clopts_positional_get_name(src));
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(src), "src");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(copy)), 0);

    clopts_program_destroy(program);
}

/* A program without subcommands takes positionals, in order of attachment, and has its own description. */
NAOS_TEST(program_takes_positionals)
{
    clopts_program_t *program;
    clopts_positional_t *file;
    clopts_positional_t *rest;

    program = make_program("filt");
    file = make_positional("file");
    rest = make_positional("rest");
    EXPECT_OK(clopts_program_add_positional(program, file));
    EXPECT_OK(clopts_program_add_positional(program, rest));
    EXPECT_OK(clopts_program_set_description(program, "Filters lines."));

    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(program), 0), file);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(program), 1), rest);
    NAOS_REQUIRE_NOT_NULL(clopts_program_get_description(program));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "Filters lines.");
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "filt");

    clopts_program_destroy(program);
}

/* ---- clopts_program_create() ------------------------------------------------- */

/* clopts_program_create() copies the name and starts with no description, options, positionals or subcommands. */
NAOS_TEST(program_create_copies_name_and_starts_empty)
{
    char name[] = "tool";
    clopts_program_t *program = SENTINEL_PROGRAM;

    test_alloc_begin(0);
    EXPECT_OK(clopts_program_create(name, &program));
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(program != SENTINEL_PROGRAM);
    name[0] = 'X';

    NAOS_REQUIRE_NOT_NULL(clopts_program_get_name(program));
    NAOS_CHECK_TRUE(clopts_program_get_name(program) != name);
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "tool"));
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * A malformed name gives INVALID_NAME, sets the out pointer to NULL and
 * allocates nothing. The rules are those of a command name, and there are no
 * reserved names.
 */
NAOS_TEST(program_create_checks_name)
{
    static const char *const bad[] = { "", "-x", "--x", "a b", "a\tb", "ab\n" };
    static const char *const good[] = { "help", "h", "version", "x", "a-b", "a=b", "B", "tool.exe" };
    clopts_program_t *program;
    size_t i;

    test_alloc_begin(0);
    program = SENTINEL_PROGRAM;
    NAOS_CHECK_INT_EQ(clopts_program_create(NULL, &program), CLOPTS_DECL_ERROR_INVALID_NAME);
    NAOS_CHECK_NULL(program);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", bad[i]);
        program = SENTINEL_PROGRAM;
        NAOS_CHECK_INT_EQ(clopts_program_create(bad[i], &program), CLOPTS_DECL_ERROR_INVALID_NAME);
        NAOS_CHECK_NULL(program);
    }
    NAOS_CHECK_UINT_EQ(test_alloc.calls, 0);

    for (i = 0; i < sizeof(good) / sizeof(good[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", good[i]);
        program = SENTINEL_PROGRAM;
        EXPECT_OK(clopts_program_create(good[i], &program));
        NAOS_REQUIRE_NOT_NULL(program);
        NAOS_REQUIRE_TRUE(program != SENTINEL_PROGRAM);
        NAOS_CHECK_STR_EQ(clopts_program_get_name(program), good[i]);
        clopts_program_destroy(program);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* clopts_program_set_description(): the last call wins, NULL resets, and the text is copied. */
NAOS_TEST(program_set_description)
{
    char text[16];
    clopts_program_t *program;

    test_alloc_begin(0);
    program = make_program("tool");
    strcpy(text, "first");
    EXPECT_OK(clopts_program_set_description(program, text));
    strcpy(text, "XXXXX");
    NAOS_REQUIRE_NOT_NULL(clopts_program_get_description(program));
    NAOS_CHECK_TRUE(clopts_program_get_description(program) != text);
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "first");

    EXPECT_OK(clopts_program_set_description(program, "second"));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "second");
    EXPECT_OK(clopts_program_set_description(program, ""));
    NAOS_REQUIRE_NOT_NULL(clopts_program_get_description(program));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "");
    EXPECT_OK(clopts_program_set_description(program, NULL));
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    EXPECT_OK(clopts_program_set_description(program, NULL));
    NAOS_CHECK_NULL(clopts_program_get_description(program));

    /* the name and the rest of the program do not change */
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_program_add_option() --------------------------------------------- */

/* Global options keep the order of attachment and the program owns them (released with it). */
NAOS_TEST(program_add_option_keeps_the_order)
{
    clopts_program_t *program;
    clopts_option_t *options[3];

    test_alloc_begin(0);
    program = make_program("tool");
    options[0] = make(CLOPTS_OPTION_FLAG, "force", 'f');
    options[1] = make(CLOPTS_OPTION_COUNT, NULL, 'v');
    options[2] = make(CLOPTS_OPTION_VALUE, "output", '\0');
    EXPECT_OK(clopts_program_add_option(program, options[0]));
    EXPECT_OK(clopts_program_add_option(program, options[1]));
    EXPECT_OK(clopts_program_add_option(program, options[2]));

    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), options[0]);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 1), options[1]);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 2), options[2]);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);

    /* global options go with subcommands and with positionals alike */
    add_command(program, make_command("build"));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "late", '\0'));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    clopts_program_destroy(program);

    program = make_program("filt");
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add_positional(program, "file"));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "late", '\0'));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 1);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * DUPLICATE_OPTION: a global option with the same long or short name
 * (exact, case-sensitive). The failing call changes nothing and the caller
 * still owns the option.
 */
NAOS_TEST(program_add_option_duplicate_changes_nothing)
{
    clopts_program_t *program;
    clopts_option_t *option;

    test_alloc_begin(0);
    program = make_program("tool");
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_VALUE, "output", '\0'));

    option = make(CLOPTS_OPTION_COUNT, "force", 'x');
    expect_add_option_fails(program, option, CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(option), "force");
    clopts_option_destroy(option);

    option = make(CLOPTS_OPTION_COUNT, "other", 'f');
    expect_add_option_fails(program, option, CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    clopts_option_destroy(option);

    option = make(CLOPTS_OPTION_FLAG, "output", 'o');
    expect_add_option_fails(program, option, CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    /* the caller still owns it: it can be attached elsewhere */
    {
        clopts_command_t *build = make_command("build");

        EXPECT_OK(clopts_command_add_option(build, option));
        add_command(program, build);
    }

    /* names differing in case are different names */
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "Force", 'F'));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * NEGATION_CONFLICT: --no-x next to a negatable --x, in either order of
 * attachment. A --no-x next to a --x that is not negatable is fine. The
 * failing call changes nothing and the caller still owns the option.
 */
NAOS_TEST(program_add_option_negation_conflict_changes_nothing)
{
    clopts_program_t *program;
    clopts_option_t *option;

    test_alloc_begin(0);
    program = make_program("tool");
    option = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    EXPECT_OK(clopts_program_add_option(program, option));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0'));

    /* --no-color after a negatable --color */
    option = make(CLOPTS_OPTION_FLAG, "no-color", '\0');
    expect_add_option_fails(program, option, CLOPTS_DECL_ERROR_NEGATION_CONFLICT);
    clopts_option_destroy(option);

    /* a negatable --cache after --no-cache */
    option = make(CLOPTS_OPTION_FLAG, "cache", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    expect_add_option_fails(program, option, CLOPTS_DECL_ERROR_NEGATION_CONFLICT);
    NAOS_CHECK_TRUE(clopts_option_is_negatable(option));
    clopts_option_destroy(option);

    /* --cache that is not negatable, and --no-x next to a --x that is not negatable */
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "cache", '\0'));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "no-cache-x", '\0'));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* The errors of clopts_program_add_option() come in a fixed order: DUPLICATE_OPTION, NEGATION_CONFLICT. */
NAOS_TEST(program_add_option_reports_the_first_error)
{
    clopts_program_t *program;
    clopts_option_t *option;

    test_alloc_begin(0);
    program = make_program("tool");
    option = make(CLOPTS_OPTION_FLAG, "color", 'c');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    EXPECT_OK(clopts_program_add_option(program, option));

    /* --no-color breaks both rules; its short name is taken */
    option = make(CLOPTS_OPTION_FLAG, "no-color", 'c');
    expect_add_option_fails(program, option, CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    clopts_option_destroy(option);

    /* NEGATION_CONFLICT last */
    option = make(CLOPTS_OPTION_FLAG, "no-color", 'n');
    expect_add_option_fails(program, option, CLOPTS_DECL_ERROR_NEGATION_CONFLICT);
    clopts_option_destroy(option);

    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_program_add_positional() ----------------------------------------- */

/*
 * MIXED_LAYOUT: a program with subcommands takes no positionals. The failing
 * call changes nothing and the caller still owns the positional.
 */
NAOS_TEST(program_add_positional_mixed_layout_changes_nothing)
{
    clopts_program_t *program;
    clopts_positional_t *positional;

    test_alloc_begin(0);
    program = make_program("tool");
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "force", 'f'));
    add_command(program, make_command("build"));

    positional = make_positional("file");
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(positional), "file");

    /* the caller still owns it: a program without subcommands takes it */
    clopts_program_destroy(program);
    program = make_program("filt");
    EXPECT_OK(clopts_program_add_positional(program, positional));
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(program), 0), positional);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * VARIADIC_POSITION: nothing after a variadic positional. POSITIONAL_ORDER:
 * no required positional after an optional one. Each failing call changes
 * nothing and the caller still owns the positional.
 */
NAOS_TEST(program_add_positional_order_changes_nothing)
{
    clopts_program_t *program;
    clopts_positional_t *positional;

    test_alloc_begin(0);
    program = make_program("filt");
    positional = make_positional("SRC");
    clopts_positional_set_required(positional, true);
    EXPECT_OK(clopts_program_add_positional(program, positional));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add_positional(program, "DST"));

    /* required after optional */
    positional = make_positional("MORE");
    clopts_positional_set_required(positional, true);
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_POSITIONAL_ORDER);
    clopts_positional_destroy(positional);

    /* optional after optional, then anything after the variadic one */
    positional = make_positional("REST");
    clopts_positional_set_variadic(positional, true);
    EXPECT_OK(clopts_program_add_positional(program, positional));
    positional = make_positional("LAST");
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_VARIADIC_POSITION);
    clopts_positional_set_variadic(positional, true);
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_VARIADIC_POSITION);
    clopts_positional_destroy(positional);

    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 3);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The errors of clopts_program_add_positional() come in a fixed order:
 * MIXED_LAYOUT, VARIADIC_POSITION, POSITIONAL_ORDER. A program never has
 * positionals and subcommands at the same time, so MIXED_LAYOUT can only meet
 * a positional that would be fine otherwise; it is returned for every kind of
 * positional. A required positional after a variadic one breaks both later
 * rules.
 */
NAOS_TEST(program_add_positional_reports_the_first_error)
{
    clopts_program_t *program;
    clopts_positional_t *positional;

    test_alloc_begin(0);
    program = make_program("tool");
    add_command(program, make_command("build"));
    positional = make_positional("file");
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    clopts_positional_set_required(positional, true);
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    clopts_positional_set_variadic(positional, true);
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    clopts_positional_destroy(positional);
    clopts_program_destroy(program);

    /* VARIADIC_POSITION before POSITIONAL_ORDER */
    program = make_program("filt");
    positional = make_positional("REST");
    clopts_positional_set_variadic(positional, true);
    EXPECT_OK(clopts_program_add_positional(program, positional));
    positional = make_positional("LAST");
    clopts_positional_set_required(positional, true);
    expect_add_positional_fails(program, positional, CLOPTS_DECL_ERROR_VARIADIC_POSITION);
    clopts_positional_destroy(positional);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_program_add_command() -------------------------------------------- */

/*
 * The program takes the subcommands as they are: same handles, in the order
 * they were added, and releases them with itself.
 */
NAOS_TEST(program_add_command_keeps_the_order)
{
    clopts_command_t *commands[3];
    clopts_program_t *program;

    test_alloc_begin(0);
    program = make_program("tool");
    commands[0] = make_command("copy");
    commands[1] = make_command("build");
    commands[2] = make_command("cat");
    add_command(program, commands[0]);
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 1);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), commands[0]);
    add_command(program, commands[1]);
    add_command(program, commands[2]);
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 3);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), commands[0]);
    NAOS_CHECK_PTR_EQ(command_at(program, 1), commands[1]);
    NAOS_CHECK_PTR_EQ(command_at(program, 2), commands[2]);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_STR_EQ(clopts_command_get_name(command_at(program, 0)), "copy");
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * DUPLICATE_COMMAND: a subcommand of the same name (exact, case-sensitive).
 * The failing call changes nothing and the caller still owns the command.
 */
NAOS_TEST(program_add_command_duplicate_changes_nothing)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_command_t *again;

    test_alloc_begin(0);
    program = make_program("tool");
    build = make_command("build");
    add_command(program, build);
    add_command(program, make_command("copy"));

    again = make_command("build");
    NAOS_REQUIRE_NOT_NULL(test_decl_add(again, CLOPTS_OPTION_FLAG, "force", 'f'));
    expect_add_command_fails(program, again, CLOPTS_DECL_ERROR_DUPLICATE_COMMAND);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "build"), build);
    /* the caller still owns it, with what is attached to it */
    NAOS_CHECK_STR_EQ(clopts_command_get_name(again), "build");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(again)), 1);
    clopts_command_destroy(again);

    /* a subcommand named like the program and names differing in case are fine */
    add_command(program, make_command("Build"));
    add_command(program, make_command("tool"));
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 4);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * MIXED_LAYOUT: a program with positionals takes no subcommands. Global
 * options with subcommands, and positionals without, are fine. The failing
 * call changes nothing and the caller still owns the command.
 */
NAOS_TEST(program_add_command_mixed_layout_changes_nothing)
{
    clopts_program_t *program;
    clopts_command_t *build;

    test_alloc_begin(0);
    program = make_program("tool");
    build = make_command("build");
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add_positional(program, "file"));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "force", 'f'));

    expect_add_command_fails(program, build, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 1);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "build"));
    clopts_program_destroy(program);

    /* the caller still owns it: global options with a subcommand */
    program = make_program("tool");
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "force", 'f'));
    add_command(program, build);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), build);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The errors of clopts_program_add_command() come in a fixed order:
 * DUPLICATE_COMMAND, MIXED_LAYOUT. A program never has positionals and
 * subcommands at the same time, so both cannot apply to one call; the order
 * is seen as each error being given on its own, and MIXED_LAYOUT for every
 * name, also the name of the program.
 */
NAOS_TEST(program_add_command_reports_the_first_error)
{
    static const char *const names[] = { "build", "filt", "help" };
    clopts_program_t *program;
    clopts_command_t *command;
    size_t i;

    test_alloc_begin(0);
    program = make_program("filt");
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add_positional(program, "file"));
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", names[i]);
        command = make_command(names[i]);
        expect_add_command_fails(program, command, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
        clopts_command_destroy(command);
    }
    clopts_program_destroy(program);

    program = make_program("tool");
    add_command(program, make_command("build"));
    command = make_command("build");
    expect_add_command_fails(program, command, CLOPTS_DECL_ERROR_DUPLICATE_COMMAND);
    clopts_command_destroy(command);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Option names are checked within one scope only: the global options and a
 * subcommand, or two subcommands, may declare the same names. There are no
 * reserved names either.
 */
NAOS_TEST(program_does_not_check_names_between_scopes)
{
    clopts_command_t *commands[2];
    clopts_program_t *program;
    clopts_option_t *color;

    program = make_program("tool");
    commands[0] = make_command("build");
    commands[1] = make_command("help");
    color = make(CLOPTS_OPTION_FLAG, "color", 'c');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, color));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "help", 'h'));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_add(program, CLOPTS_OPTION_FLAG, "version", 'V'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[0], CLOPTS_OPTION_VALUE, "color", 'c'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[0], CLOPTS_OPTION_FLAG, "no-color", '\0'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[0], CLOPTS_OPTION_COUNT, "help", 'h'));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(commands[1], CLOPTS_OPTION_COUNT, "help", 'h'));

    add_command(program, commands[0]);
    add_command(program, commands[1]);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(commands[0])), 3);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(commands[1])), 1);
    clopts_program_destroy(program);
}

/* ---- read functions ---------------------------------------------------------- */

/*
 * The read functions of a program: name, description, the global options and
 * positionals in order of attachment, the subcommands in the order they were
 * added, and the lookup by exact, case-sensitive name (the program itself is
 * not a subcommand).
 */
NAOS_TEST(program_read_functions)
{
    clopts_command_t *commands[3];
    clopts_option_t *force;
    clopts_option_t *verbose;
    clopts_positional_t *file;
    clopts_program_t *program;

    program = make_program("tool");
    EXPECT_OK(clopts_program_set_description(program, "A tool."));
    force = test_decl_global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    verbose = test_decl_global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    NAOS_REQUIRE_NOT_NULL(force);
    NAOS_REQUIRE_NOT_NULL(verbose);
    commands[0] = make_command("copy");
    commands[1] = make_command("build");
    commands[2] = make_command("Build");
    add_command(program, commands[0]);
    add_command(program, commands[1]);
    add_command(program, commands[2]);

    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
    check_str_or_null(clopts_program_get_description(program), "A tool.");
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), force);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 1), verbose);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 3);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), commands[0]);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 1), commands[1]);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 2), commands[2]);

    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "copy"), commands[0]);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "build"), commands[1]);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "Build"), commands[2]);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "BUILD"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, "bui"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, "builds"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, "tool"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, ""));
    clopts_program_destroy(program);

    /* a program without subcommands */
    program = make_program("filt");
    file = make_positional("file");
    EXPECT_OK(clopts_program_add_positional(program, file));
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "filt");
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 1);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(program), 0), file);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "filt"));
    NAOS_CHECK_NULL(clopts_program_find_command(program, "file"));
    clopts_program_destroy(program);
}

/*
 * The read functions of a command, unattached and attached alike: name,
 * texts, and the options and positionals in order of attachment.
 */
NAOS_TEST(command_read_functions)
{
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *force;
    clopts_positional_t *src;
    clopts_positional_t *dst;
    clopts_program_t *program;
    int pass;

    program = make_program("tool");
    build = make_command("build");
    NAOS_CHECK_STR_EQ(clopts_command_get_name(build), "build");
    NAOS_CHECK_NULL(clopts_command_get_summary(build));
    NAOS_CHECK_NULL(clopts_command_get_description(build));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(build)), 0);

    EXPECT_OK(clopts_command_set_summary(build, "Builds."));
    EXPECT_OK(clopts_command_set_description(build, "Builds the tree."));
    EXPECT_OK(clopts_program_set_description(program, "A tool."));
    output = test_decl_add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    force = test_decl_add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    src = test_decl_add_positional(build, "src");
    dst = test_decl_add_positional(build, "dst");
    NAOS_REQUIRE_NOT_NULL(output);
    NAOS_REQUIRE_NOT_NULL(force);
    NAOS_REQUIRE_NOT_NULL(src);
    NAOS_REQUIRE_NOT_NULL(dst);

    for (pass = 0; pass < 2; pass++)
    {
        NAOS_NOTE("pass %d (%s)", pass, (pass == 0) ? "unattached" : "attached");
        NAOS_CHECK_STR_EQ(clopts_command_get_name(build), "build");
        check_str_or_null(clopts_command_get_summary(build), "Builds.");
        check_str_or_null(clopts_command_get_description(build), "Builds the tree.");
        NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 2);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_command_get_scope(build), 0), output);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_command_get_scope(build), 1), force);
        NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(build)), 2);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_command_get_scope(build), 0), src);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_command_get_scope(build), 1), dst);
        NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
        check_str_or_null(clopts_program_get_description(program), "A tool.");
        NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
        if (pass == 0)
        {
            add_command(program, build);
        }
    }
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);
    clopts_program_destroy(program);
}

/*
 * The read functions of an option give back every attribute, before and
 * after the attach. Attributes not set read as NULL, '\0', 0 or false.
 */
NAOS_TEST(option_read_functions)
{
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *count;
    clopts_option_t *flag;
    clopts_option_t *bare;
    clopts_program_t *program;
    int pass;

    program = make_program("tool");
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

    for (pass = 0; pass < 2; pass++)
    {
        NAOS_NOTE("pass %d (%s)", pass, (pass == 0) ? "unattached" : "attached");
        NAOS_CHECK_INT_EQ(clopts_option_get_kind(value), CLOPTS_OPTION_VALUE);
        check_str_or_null(clopts_option_get_long_name(value), "output");
        NAOS_CHECK_INT_EQ(clopts_option_get_short_name(value), 'o');
        check_str_or_null(clopts_option_get_help(value), "Output file.");
        check_str_or_null(clopts_option_get_value_name(value), "FILE");
        check_str_or_null(clopts_option_get_env(value), "TOOL_OUTPUT");
        check_str_or_null(clopts_option_get_default(value), "a.out");
        NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(value), '\0');
        NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(value), 0);
        NAOS_CHECK_FALSE(clopts_option_is_required(value));
        NAOS_CHECK_FALSE(clopts_option_is_negatable(value));
        NAOS_CHECK_FALSE(clopts_option_is_terminal(value));

        NAOS_CHECK_INT_EQ(clopts_option_get_kind(list), CLOPTS_OPTION_LIST);
        NAOS_CHECK_INT_EQ(clopts_option_get_short_name(list), 'I');
        NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(list), ',');
        NAOS_CHECK_TRUE(clopts_option_is_required(list));
        NAOS_CHECK_NULL(clopts_option_get_default(list));

        NAOS_CHECK_INT_EQ(clopts_option_get_kind(count), CLOPTS_OPTION_COUNT);
        NAOS_CHECK_NULL(clopts_option_get_long_name(count));
        NAOS_CHECK_INT_EQ(clopts_option_get_short_name(count), 'v');
        NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(count), 3);

        NAOS_CHECK_INT_EQ(clopts_option_get_kind(flag), CLOPTS_OPTION_FLAG);
        NAOS_CHECK_INT_EQ(clopts_option_get_short_name(flag), '\0');
        NAOS_CHECK_TRUE(clopts_option_is_negatable(flag));
        NAOS_CHECK_TRUE(clopts_option_is_terminal(flag));
        NAOS_CHECK_NULL(clopts_option_get_help(flag));

        NAOS_CHECK_NULL(clopts_option_get_help(bare));
        NAOS_CHECK_NULL(clopts_option_get_value_name(bare));
        NAOS_CHECK_NULL(clopts_option_get_env(bare));
        NAOS_CHECK_NULL(clopts_option_get_default(bare));
        if (pass == 0)
        {
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, value));
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, list));
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, count));
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, flag));
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, bare));
        }
    }
    NAOS_CHECK_PTR_EQ(global_at(program, 3), flag);
    clopts_program_destroy(program);
}

/* The read functions of a positional, before and after the attach. */
NAOS_TEST(positional_read_functions)
{
    clopts_positional_t *src;
    clopts_positional_t *rest;
    clopts_program_t *program;
    int pass;

    program = make_program("tool");
    src = make_positional("SRC");
    EXPECT_OK(clopts_positional_set_help(src, "Source."));
    clopts_positional_set_required(src, true);
    rest = make_positional("REST");
    clopts_positional_set_variadic(rest, true);

    for (pass = 0; pass < 2; pass++)
    {
        NAOS_NOTE("pass %d (%s)", pass, (pass == 0) ? "unattached" : "attached");
        check_str_or_null(clopts_positional_get_name(src), "SRC");
        check_str_or_null(clopts_positional_get_help(src), "Source.");
        NAOS_CHECK_TRUE(clopts_positional_is_required(src));
        NAOS_CHECK_FALSE(clopts_positional_is_variadic(src));
        check_str_or_null(clopts_positional_get_name(rest), "REST");
        NAOS_CHECK_NULL(clopts_positional_get_help(rest));
        NAOS_CHECK_FALSE(clopts_positional_is_required(rest));
        NAOS_CHECK_TRUE(clopts_positional_is_variadic(rest));
        if (pass == 0)
        {
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach_positional(program, src));
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach_positional(program, rest));
        }
    }
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(program), 1), rest);
    clopts_program_destroy(program);
}

/* ---- terminal ------------------------------------------------------------------ */

/*
 * clopts_option_set_terminal(): FLAG only (ATTRIBUTE_KIND for the other
 * kinds, while false resets on any kind).
 */
NAOS_TEST(set_terminal_is_for_flags)
{
    static const clopts_option_kind_t others[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_option_t *flag;
    clopts_option_t *option;
    clopts_program_t *program;
    size_t i;

    program = make_program("tool");
    for (i = 0; i < sizeof(others) / sizeof(others[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)others[i]);
        option = make(others[i], "other", 'o');
        NAOS_CHECK_INT_EQ(clopts_option_set_terminal(option, true), CLOPTS_DECL_ERROR_ATTRIBUTE_KIND);
        NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
        EXPECT_OK(clopts_option_set_terminal(option, false));
        NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
        if (i == 0)
        {
            NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, option));
        }
        else
        {
            clopts_option_destroy(option);
        }
    }

    flag = make(CLOPTS_OPTION_FLAG, "help", 'h');
    EXPECT_OK(clopts_option_set_terminal(flag, true));
    NAOS_CHECK_TRUE(clopts_option_is_terminal(flag));
    EXPECT_OK(clopts_option_set_terminal(flag, true));
    NAOS_CHECK_TRUE(clopts_option_is_terminal(flag));
    EXPECT_OK(clopts_option_set_terminal(flag, false));
    NAOS_CHECK_FALSE(clopts_option_is_terminal(flag));
    EXPECT_OK(clopts_option_set_terminal(flag, true));

    NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, flag));
    NAOS_CHECK_TRUE(clopts_option_is_terminal(global_at(program, 1)));
    clopts_program_destroy(program);
}

/* ---- string copies --------------------------------------------------------- */

NAOS_TEST(strings_are_copied)
{
    char program_name[] = "tool";
    char cmd_name[] = "build";
    char long_name[] = "output";
    char pos_name[] = "src";
    char text[32];
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_positional_t *src;

    program = make_program(program_name);
    build = make_command(cmd_name);
    output = make(CLOPTS_OPTION_VALUE, long_name, 'o');
    src = make_positional(pos_name);
    memset(program_name, 'X', sizeof(program_name) - 1);
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
    EXPECT_OK(clopts_program_set_description(program, text));
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
    add_command(program, build);

    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
    NAOS_CHECK_STR_EQ(clopts_command_get_name(build), "build");
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(output), "output");
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(src), "src");
    NAOS_REQUIRE_NOT_NULL(clopts_command_get_summary(build));
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(build), "Builds.");
    NAOS_REQUIRE_NOT_NULL(clopts_command_get_description(build));
    NAOS_CHECK_STR_EQ(clopts_command_get_description(build), "Builds the tree.");
    NAOS_REQUIRE_NOT_NULL(clopts_program_get_description(program));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "Program text.");
    NAOS_REQUIRE_NOT_NULL(clopts_option_get_help(output));
    NAOS_CHECK_STR_EQ(clopts_option_get_help(output), "Output file.");
    NAOS_REQUIRE_NOT_NULL(clopts_option_get_value_name(output));
    NAOS_CHECK_STR_EQ(clopts_option_get_value_name(output), "FILE");
    NAOS_REQUIRE_NOT_NULL(clopts_option_get_env(output));
    NAOS_CHECK_STR_EQ(clopts_option_get_env(output), "TOOL_OUTPUT");
    NAOS_REQUIRE_NOT_NULL(clopts_option_get_default(output));
    NAOS_CHECK_STR_EQ(clopts_option_get_default(output), "a.out");
    NAOS_REQUIRE_NOT_NULL(clopts_positional_get_help(src));
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(src), "Source.");

    clopts_program_destroy(program);
}

/* A string attribute set from its own current copy is copied before the old copy is released. */
NAOS_TEST(string_set_from_its_own_copy)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *output;

    build = make_command("build");
    EXPECT_OK(clopts_command_set_summary(build, "Builds."));
    EXPECT_OK(clopts_command_set_summary(build, clopts_command_get_summary(build)));
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(build), "Builds.");
    EXPECT_OK(clopts_command_set_description(build, "Builds the tree."));
    EXPECT_OK(clopts_command_set_description(build, clopts_command_get_description(build)));
    NAOS_CHECK_STR_EQ(clopts_command_get_description(build), "Builds the tree.");
    clopts_command_destroy(build);

    output = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(output, "Output file."));
    EXPECT_OK(clopts_option_set_help(output, clopts_option_get_help(output)));
    NAOS_CHECK_STR_EQ(clopts_option_get_help(output), "Output file.");
    clopts_option_destroy(output);

    program = make_program("tool");
    EXPECT_OK(clopts_program_set_description(program, "A tool."));
    EXPECT_OK(clopts_program_set_description(program, clopts_program_get_description(program)));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "A tool.");
    clopts_program_destroy(program);
}

/* ---- setting again --------------------------------------------------------- */

NAOS_TEST(setting_again_keeps_the_last_value)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *count;
    clopts_option_t *flag;
    clopts_positional_t *src;

    program = make_program("tool");
    build = make_command("build");
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    list = make(CLOPTS_OPTION_LIST, "include", 'I');
    count = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    flag = make(CLOPTS_OPTION_FLAG, "color", '\0');
    src = make_positional("src");

    EXPECT_OK(clopts_program_set_description(program, "first"));
    EXPECT_OK(clopts_program_set_description(program, "second"));
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

    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "second");
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(build), "second");
    NAOS_CHECK_STR_EQ(clopts_command_get_description(build), "second");
    NAOS_CHECK_STR_EQ(clopts_option_get_help(value), "second");
    NAOS_CHECK_STR_EQ(clopts_option_get_value_name(value), "B");
    NAOS_CHECK_STR_EQ(clopts_option_get_env(value), "ENV_B");
    NAOS_CHECK_STR_EQ(clopts_option_get_default(value), "y");
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(list), ':');
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(count), 8);
    NAOS_CHECK_TRUE(clopts_option_is_negatable(flag));
    NAOS_CHECK_TRUE(clopts_option_is_terminal(flag));
    NAOS_CHECK_TRUE(clopts_option_is_required(list));
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(src), "second");
    NAOS_CHECK_TRUE(clopts_positional_is_required(src));
    NAOS_CHECK_TRUE(clopts_positional_is_variadic(src));

    /* NULL, false, '\0' and 0 reset */
    EXPECT_OK(clopts_program_set_description(program, NULL));
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

    NAOS_CHECK_NULL(clopts_program_get_description(program));
    NAOS_CHECK_NULL(clopts_command_get_summary(build));
    NAOS_CHECK_NULL(clopts_command_get_description(build));
    NAOS_CHECK_NULL(clopts_option_get_help(value));
    NAOS_CHECK_NULL(clopts_option_get_value_name(value));
    NAOS_CHECK_NULL(clopts_option_get_env(value));
    NAOS_CHECK_NULL(clopts_option_get_default(value));
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(list), '\0');
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(count), 0);
    NAOS_CHECK_FALSE(clopts_option_is_negatable(flag));
    NAOS_CHECK_FALSE(clopts_option_is_terminal(flag));
    NAOS_CHECK_FALSE(clopts_option_is_required(list));
    NAOS_CHECK_NULL(clopts_positional_get_help(src));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(src));
    NAOS_CHECK_FALSE(clopts_positional_is_required(src));

    /* the attach and the program keep the attributes as they are */
    NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, value));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, list));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, count));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, flag));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach_positional(build, src));
    add_command(program, build);
    NAOS_CHECK_NULL(clopts_option_get_help(value));
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(list), '\0');
    NAOS_CHECK_FALSE(clopts_option_is_terminal(flag));
    NAOS_CHECK_FALSE(clopts_positional_is_required(src));
    NAOS_CHECK_NULL(clopts_command_get_summary(build));

    clopts_program_destroy(program);
}

/* ---- handle stability ---------------------------------------------------- */

NAOS_TEST(hundred_options_keep_the_first_handle)
{
    char name[16];
    clopts_program_t *program;
    clopts_option_t *first;
    clopts_option_t *option;
    clopts_positional_t *positional;
    int i;

    program = make_program("tool");

    first = make(CLOPTS_OPTION_VALUE, "o0", 'a');
    EXPECT_OK(clopts_option_set_help(first, "first help"));
    NAOS_REQUIRE_NOT_NULL(test_decl_global_attach(program, first));
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "o%d", i);
        NAOS_NOTE("i=%d", i);
        option = test_decl_global_add(program, CLOPTS_OPTION_FLAG, name, '\0');
        NAOS_REQUIRE_NOT_NULL(option);
        NAOS_CHECK_PTR_EQ(global_at(program, (size_t)i), option);
    }
    for (i = 0; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "p%d", i);
        NAOS_NOTE("i=%d", i);
        positional = test_decl_global_add_positional(program, name);
        NAOS_REQUIRE_NOT_NULL(positional);
    }

    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 100);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 100);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), first);
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(first), CLOPTS_OPTION_VALUE);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(first), "o0");
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(first), 'a');
    NAOS_CHECK_STR_EQ(clopts_option_get_help(first), "first help");
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(clopts_scope_get_option_at(clopts_program_get_scope(program), 99)), "o99");
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(clopts_scope_get_positional_at(clopts_program_get_scope(program), 99)), "p99");

    clopts_program_destroy(program);
}

NAOS_TEST(hundred_commands_keep_the_first_handle)
{
    char name[16];
    clopts_command_t *commands[100];
    clopts_program_t *program;
    clopts_command_t *first;
    int i;

    program = make_program("tool");
    first = make_command("c0");
    EXPECT_OK(clopts_command_set_summary(first, "first summary"));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(first, CLOPTS_OPTION_FLAG, "force", 'f'));
    commands[0] = first;
    add_command(program, first);
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "c%d", i);
        NAOS_NOTE("i=%d", i);
        commands[i] = make_command(name);
        add_command(program, commands[i]);
        NAOS_CHECK_PTR_EQ(command_at(program, 0), first);
    }

    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 100);
    for (i = 0; i < 100; i++)
    {
        NAOS_NOTE("i=%d", i);
        NAOS_CHECK_PTR_EQ(command_at(program, (size_t)i), commands[i]);
    }
    NAOS_CHECK_PTR_EQ(command_at(program, 0), first);
    NAOS_CHECK_STR_EQ(clopts_command_get_name(first), "c0");
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(first), "first summary");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(first)), 1);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "c99"), command_at(program, 99));
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "c50"), commands[50]);

    clopts_program_destroy(program);
}

/* ---- out of memory --------------------------------------------------------- */

/* One builder call under fault injection, with its context. */
typedef struct oom_call
{
    /* Makes the call. */
    clopts_decl_error_t (*call)(struct oom_call *self);
    /* Checks after a failed call that nothing changed (may be NULL). */
    void (*unchanged)(struct oom_call *self);
    clopts_program_t *program;
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
    clopts_decl_error_t error;
    long live;
    size_t n;

    for (n = 1; n < 20; n++)
    {
        NAOS_NOTE("fault point %u", (unsigned)n);
        live = test_alloc.live;
        test_alloc_set_fail_at(test_alloc.calls + n);
        error = call->call(call);
        test_alloc_set_fail_at(0);
        if (error == CLOPTS_DECL_ERROR_NONE)
        {
            return n - 1;
        }
        NAOS_CHECK_INT_EQ(error, CLOPTS_DECL_ERROR_OUT_OF_MEMORY);
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

static clopts_decl_error_t oom_command_new(oom_call_t *c)
{
    c->command_out = SENTINEL_COMMAND;
    return clopts_command_create("build", &c->command_out);
}

static void oom_command_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->command_out);
}

static clopts_decl_error_t oom_set_summary(oom_call_t *c)
{
    return clopts_command_set_summary(c->command, "new");
}

static clopts_decl_error_t oom_set_description(oom_call_t *c)
{
    return clopts_command_set_description(c->command, "new");
}

static void oom_command_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(c->command), "old");
    NAOS_CHECK_STR_EQ(clopts_command_get_description(c->command), "old");
}

static clopts_decl_error_t oom_option_new(oom_call_t *c)
{
    c->option_out = SENTINEL_OPTION;
    return clopts_option_create(CLOPTS_OPTION_VALUE, "output", 'o', &c->option_out);
}

static void oom_option_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->option_out);
}

static clopts_decl_error_t oom_option_help(oom_call_t *c)
{
    return clopts_option_set_help(c->option, "new");
}

static clopts_decl_error_t oom_option_value_name(oom_call_t *c)
{
    return clopts_option_set_value_name(c->option, "new");
}

static clopts_decl_error_t oom_option_env(oom_call_t *c)
{
    return clopts_option_set_env(c->option, "new");
}

static clopts_decl_error_t oom_option_default(oom_call_t *c)
{
    return clopts_option_set_default(c->option, "new");
}

static void oom_option_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(clopts_option_get_help(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_get_value_name(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_get_env(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_get_default(c->option), "old");
}

static clopts_decl_error_t oom_add_option(oom_call_t *c)
{
    return clopts_command_add_option(c->command, c->option);
}

static void oom_add_option_unchanged(oom_call_t *c)
{
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(c->command)), 0);
}

static clopts_decl_error_t oom_positional_new(oom_call_t *c)
{
    c->positional_out = SENTINEL_POSITIONAL;
    return clopts_positional_create("src", &c->positional_out);
}

static void oom_positional_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->positional_out);
}

static clopts_decl_error_t oom_positional_help(oom_call_t *c)
{
    return clopts_positional_set_help(c->positional, "new");
}

static void oom_positional_help_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(c->positional), "old");
}

static clopts_decl_error_t oom_add_positional(oom_call_t *c)
{
    return clopts_command_add_positional(c->command, c->positional);
}

static void oom_add_positional_unchanged(oom_call_t *c)
{
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(c->command)), 0);
}

static clopts_decl_error_t oom_program_create(oom_call_t *c)
{
    c->program_out = SENTINEL_PROGRAM;
    return clopts_program_create("tool", &c->program_out);
}

static void oom_program_create_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->program_out);
}

static clopts_decl_error_t oom_program_description(oom_call_t *c)
{
    return clopts_program_set_description(c->program, "new");
}

static void oom_program_description_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(clopts_program_get_description(c->program), "old");
}

static clopts_decl_error_t oom_program_add_option(oom_call_t *c)
{
    return clopts_program_add_option(c->program, c->option);
}

static void oom_program_add_option_unchanged(oom_call_t *c)
{
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(c->program)), 0);
}

static clopts_decl_error_t oom_program_add_positional(oom_call_t *c)
{
    return clopts_program_add_positional(c->program, c->positional);
}

static void oom_program_add_positional_unchanged(oom_call_t *c)
{
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(c->program)), 0);
}

static clopts_decl_error_t oom_program_add_command(oom_call_t *c)
{
    return clopts_program_add_command(c->program, c->command);
}

/* The program has no subcommand, and the caller still owns the command with its contents. */
static void oom_program_add_command_unchanged(oom_call_t *c)
{
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(c->program), 0);
    NAOS_CHECK_NULL(clopts_program_find_command(c->program, "build"));
    NAOS_CHECK_STR_EQ(clopts_command_get_name(c->command), "build");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(c->command)), 1);
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
    NAOS_REQUIRE_TRUE(c.command_out != SENTINEL_COMMAND);
    c.command = c.command_out;

    EXPECT_OK(clopts_command_set_summary(c.command, "old"));
    EXPECT_OK(clopts_command_set_description(c.command, "old"));
    c.call = oom_set_summary;
    c.unchanged = oom_command_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    EXPECT_OK(clopts_command_set_summary(c.command, "old"));
    c.call = oom_set_description;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_STR_EQ(clopts_command_get_description(c.command), "new");

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
    NAOS_CHECK_STR_EQ(clopts_option_get_default(c.option), "new");

    c.call = oom_add_option;
    c.unchanged = oom_add_option_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(c.command)), 1);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_command_get_scope(c.command), 0), c.option);

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
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(c.positional), "new");

    c.call = oom_add_positional;
    c.unchanged = oom_add_positional_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(c.command)), 1);

    /* the program and its name */
    c.call = oom_program_create;
    c.unchanged = oom_program_create_unchanged;
    NAOS_CHECK_UINT_EQ(fail_each_point(&c), 2);
    NAOS_REQUIRE_NOT_NULL(c.program_out);
    NAOS_REQUIRE_TRUE(c.program_out != SENTINEL_PROGRAM);
    c.program = c.program_out;

    EXPECT_OK(clopts_program_set_description(c.program, "old"));
    c.call = oom_program_description;
    c.unchanged = oom_program_description_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_STR_EQ(clopts_program_get_description(c.program), "new");

    /* a global option, here a negatable flag: on failure the caller still owns it */
    color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    c.option = color;
    c.call = oom_program_add_option;
    c.unchanged = oom_program_add_option_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(c.program)), 1);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(c.program), 0), color);

    /* a positional of a program without subcommands: on failure the caller still owns it */
    c.program = make_program("filt");
    c.positional = make_positional("file");
    c.call = oom_program_add_positional;
    c.unchanged = oom_program_add_positional_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(c.program)), 1);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(c.program), 0), c.positional);
    clopts_program_destroy(c.program);
    c.program = c.program_out;

    /* a subcommand: on failure the caller still owns it */
    c.call = oom_program_add_command;
    c.unchanged = oom_program_add_command_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(c.program), 1);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(c.program, 0), c.command);

    clopts_program_destroy(c.program_out);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Fills a small program and one subcommand through every allocating builder
 * function, with every call checked by test_decl_check() or made by a helper,
 * so that test_decl_failures counts the failed ones. The subcommand is
 * remembered for program (test_decl_finish() adds it).
 */
static void build_sample(clopts_program_t *program)
{
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *include;
    clopts_option_t *color;
    clopts_positional_t *src;

    if (program != NULL)
    {
        test_decl_check(clopts_program_set_description(program, "A tool."));
    }
    test_decl_global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    if (color != NULL)
    {
        test_decl_check(clopts_option_set_negatable(color, true));
    }
    test_decl_global_attach(program, color);
    build = test_decl_command(program, "build");
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
 * Moves the failing fault point fail_at = 1, 2, ... over test_decl_begin(),
 * build_sample() and test_decl_finish() until a run passes no failing fault
 * point (test_alloc.calls < fail_at). The first two fault points are in
 * clopts_program_create(), which then gives NULL. A run that passes a
 * failing fault point has failed calls; whatever was made is released by
 * clopts_program_destroy(), by test_decl_finish() when a subcommand could not
 * be added, or by the helper for a failed attach.
 */
NAOS_TEST(every_failure_point_is_released)
{
    clopts_program_t *program;
    size_t fail_at;
    bool succeeded = false;

    for (fail_at = 1; (fail_at < 200) && (!succeeded); fail_at++)
    {
        NAOS_NOTE("fail_at=%u", (unsigned)fail_at);
        test_alloc_begin(fail_at);
        test_decl_reset();
        program = test_decl_begin("tool");
        if (program == NULL)
        {
            /* Only the first two fault points are in test_decl_begin(). */
            NAOS_CHECK_UINT_LE(fail_at, 2);
            NAOS_CHECK_UINT_EQ(test_alloc.calls, fail_at);
            NAOS_CHECK_INT_EQ(test_decl_last, CLOPTS_DECL_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_TRUE(test_alloc_end());
            continue;
        }

        build_sample(program);
        program = test_decl_finish(program);
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

/*
 * clopts_program_destroy() releases every block: the program, its global
 * options, the commands, their options and positionals, and their strings.
 */
NAOS_TEST(program_free_releases_everything)
{
    clopts_program_t *program;

    test_alloc_begin(0);
    test_decl_reset();
    program = test_decl_begin("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    build_sample(program);
    NAOS_REQUIRE_PTR_EQ(test_decl_finish(program), program);
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
 * and its handles and contents stay as they were.
 */
NAOS_TEST(parse_leaves_the_program_as_it_is)
{
    clopts_command_t *build;
    clopts_program_t *program;
    clopts_option_t *flag;

    test_alloc_begin(0);
    program = make_program("tool");
    build = make_command("build");
    flag = test_decl_global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_REQUIRE_NOT_NULL(flag);
    add_command(program, build);

    parse_name_only(program, "tool", CLOPTS_PARSE_NO_COMMAND);
    parse_name_only(program, "tool", CLOPTS_PARSE_NO_COMMAND);
    test_alloc_set_fail_at(test_alloc.calls + 1);
    parse_name_only(program, "tool", CLOPTS_PARSE_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    parse_name_only(program, "tool", CLOPTS_PARSE_NO_COMMAND);

    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 1);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 1);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), flag);
    NAOS_CHECK_NULL(clopts_option_get_help(flag));

    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_SUITE(build,
    NAOS_ADD_TEST(free_null_does_nothing),
    NAOS_ADD_TEST(command_new_copies_name_and_starts_empty),
    NAOS_ADD_TEST(command_new_checks_name),
    NAOS_ADD_TEST(new_option_and_positional_start_reset_and_unattached),
    NAOS_ADD_TEST(unattached_objects_are_released_by_their_free),
    NAOS_ADD_TEST(attach_links_objects_in_order),
    NAOS_ADD_TEST(program_takes_positionals),
    NAOS_ADD_TEST(program_create_copies_name_and_starts_empty),
    NAOS_ADD_TEST(program_create_checks_name),
    NAOS_ADD_TEST(program_set_description),
    NAOS_ADD_TEST(program_add_option_keeps_the_order),
    NAOS_ADD_TEST(program_add_option_duplicate_changes_nothing),
    NAOS_ADD_TEST(program_add_option_negation_conflict_changes_nothing),
    NAOS_ADD_TEST(program_add_option_reports_the_first_error),
    NAOS_ADD_TEST(program_add_positional_mixed_layout_changes_nothing),
    NAOS_ADD_TEST(program_add_positional_order_changes_nothing),
    NAOS_ADD_TEST(program_add_positional_reports_the_first_error),
    NAOS_ADD_TEST(program_add_command_keeps_the_order),
    NAOS_ADD_TEST(program_add_command_duplicate_changes_nothing),
    NAOS_ADD_TEST(program_add_command_mixed_layout_changes_nothing),
    NAOS_ADD_TEST(program_add_command_reports_the_first_error),
    NAOS_ADD_TEST(program_does_not_check_names_between_scopes),
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
    NAOS_ADD_TEST(parse_leaves_the_program_as_it_is)
)
