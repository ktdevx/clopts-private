/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite build: the builder functions, the release, string copies, the
 * last-call-wins and reset rules, handle stability, out of memory in every
 * allocating builder function, the program builder (its creation by name, its
 * description, and the adds of global options, positionals and subcommands
 * with their errors and their order) and the read functions. The contents
 * of the shared fixtures of the parser tests are checked in the fixtures
 * suite (tests/parse/test_fixture_checks.c).
 *
 * A program is created by name; options, positionals and subcommands are
 * created by adding them to the program or to a command, and then configured
 * with their setters. A failing call changes nothing and leaves its error in
 * the program (clopts_program_get_error()); from then on every builder call
 * does nothing, so each failing call is made on a program of its own. The
 * declaration rules within a command are tested in the decl suites, and the
 * rules of the declaration error itself (NULL handles, the first error) in
 * the decl_error suite. Stored values are read through the public read
 * functions, and the fault allocator (test_alloc.h) shows which blocks are
 * alive.
 */

#include "clopts/parse/parse.h"
#include "clopts/decl/program.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

/* Number of elements of the array a. */
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* Checks that program has the declaration error error (CLOPTS_ERROR_NONE: none). */
#define EXPECT_ERROR(program, error) NAOS_CHECK_INT_EQ(clopts_program_get_error(program), (error))

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

/* clopts_program_create(name); the test stops when the program is NULL or has an error. */
static clopts_program_t *make_program(const char *name)
{
    clopts_program_t *program = test_decl_program(name);

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/* clopts_program_add_command(program, name); the test stops when it fails. */
static clopts_command_t *add_command(clopts_program_t *program, const char *name)
{
    clopts_command_t *command = clopts_program_add_command(program, name);

    NAOS_REQUIRE_NOT_NULL(command);
    return command;
}

/* clopts_program_add_option(); the test stops when it fails. */
static clopts_option_t *global_add(clopts_program_t *program, clopts_option_kind_t kind, const char *long_name,
                                   char short_name)
{
    clopts_option_t *option = clopts_program_add_option(program, kind, long_name, short_name);

    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/* clopts_command_add_option(); the test stops when it fails. */
static clopts_option_t *add(clopts_command_t *command, clopts_option_kind_t kind, const char *long_name,
                            char short_name)
{
    clopts_option_t *option = clopts_command_add_option(command, kind, long_name, short_name);

    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/* clopts_program_add_positional(); the test stops when it fails. */
static clopts_positional_t *global_add_positional(clopts_program_t *program, const char *name)
{
    clopts_positional_t *positional = clopts_program_add_positional(program, name);

    NAOS_REQUIRE_NOT_NULL(positional);
    return positional;
}

/* clopts_command_add_positional(); the test stops when it fails. */
static clopts_positional_t *add_positional(clopts_command_t *command, const char *name)
{
    clopts_positional_t *positional = clopts_command_add_positional(command, name);

    NAOS_REQUIRE_NOT_NULL(positional);
    return positional;
}

/* The global option index of program; the test stops when there is none. */
static const clopts_option_t *global_at(const clopts_program_t *program, size_t index)
{
    NAOS_REQUIRE_UINT_GT(clopts_scope_get_option_count(clopts_program_get_scope(program)), index);
    return clopts_scope_get_option_at(clopts_program_get_scope(program), index);
}

/*
 * The global options, positionals and subcommands of program, and the blocks
 * alive, to check that a failing call changed nothing and left nothing
 * allocated. Up to SHAPE_MAX of each are recorded.
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
    long live;
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
    shape.live = test_alloc.live;
    return shape;
}

/*
 * Checks that the last call failed with error and left program with the
 * options, positionals and subcommands of shape, in the same order, and the
 * same blocks alive.
 */
static void check_failed(const clopts_program_t *program, const program_shape_t *shape, clopts_error_t error)
{
    program_shape_t now = program_shape(program);
    size_t i;

    EXPECT_ERROR(program, error);
    NAOS_CHECK_UINT_EQ(now.option_count, shape->option_count);
    NAOS_CHECK_UINT_EQ(now.positional_count, shape->positional_count);
    NAOS_CHECK_UINT_EQ(now.command_count, shape->command_count);
    for (i = 0; i < SHAPE_MAX; i++)
    {
        NAOS_CHECK_PTR_EQ(now.options[i], shape->options[i]);
        NAOS_CHECK_PTR_EQ(now.positionals[i], shape->positionals[i]);
        NAOS_CHECK_PTR_EQ(now.commands[i], shape->commands[i]);
    }
    NAOS_CHECK_INT_EQ(now.live, shape->live);
}

/* Adds a global option to program expecting error: NULL, and nothing changed (check_failed()). */
static void expect_add_option_fails(clopts_program_t *program, clopts_option_kind_t kind, const char *long_name,
                                    char short_name, clopts_error_t error)
{
    program_shape_t shape = program_shape(program);

    NAOS_CHECK_NULL(clopts_program_add_option(program, kind, long_name, short_name));
    check_failed(program, &shape, error);
}

/* As expect_add_option_fails(), with clopts_program_add_positional(). */
static void expect_add_positional_fails(clopts_program_t *program, const char *name, clopts_error_t error)
{
    program_shape_t shape = program_shape(program);

    NAOS_CHECK_NULL(clopts_program_add_positional(program, name));
    check_failed(program, &shape, error);
}

/* As expect_add_option_fails(), with clopts_program_add_command(). */
static void expect_add_command_fails(clopts_program_t *program, const char *name, clopts_error_t error)
{
    program_shape_t shape = program_shape(program);

    NAOS_CHECK_NULL(clopts_program_add_command(program, name));
    check_failed(program, &shape, error);
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

NAOS_TEST(destroy_null_does_nothing)
{
    clopts_program_destroy(NULL);
    NAOS_PASS();
}

/* ---- commands ---------------------------------------------------------------- */

/* clopts_program_add_command() copies the name and starts with no texts, options or positionals. */
NAOS_TEST(add_command_copies_name_and_starts_empty)
{
    char name[] = "build";
    clopts_program_t *program = make_program("tool");
    clopts_command_t *command = add_command(program, name);

    name[0] = 'X';
    NAOS_REQUIRE_NOT_NULL(clopts_command_get_name(command));
    NAOS_CHECK_TRUE(clopts_command_get_name(command) != name);
    NAOS_CHECK_STR_EQ(clopts_command_get_name(command), "build");
    NAOS_CHECK_NULL(clopts_command_get_summary(command));
    NAOS_CHECK_NULL(clopts_command_get_description(command));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(command)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(command)), 0);
    clopts_program_destroy(program);
}

/*
 * A malformed name gives INVALID_NAME and allocates nothing. There are no
 * reserved names: "help", "h" and "version" are ordinary command names.
 */
NAOS_TEST(add_command_checks_name)
{
    static const char *const bad[] = { NULL, "", "-x", "--x", "a b", "a\tb", "ab\n" };
    static const char *const good[] = { "help", "h", "version", "x", "a-b", "a=b", "B" };
    clopts_program_t *program;
    size_t calls;
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(bad); i++)
    {
        NAOS_NOTE("name=\"%s\"", (bad[i] != NULL) ? bad[i] : "(null)");
        program = make_program("tool");
        calls = test_alloc.calls;
        expect_add_command_fails(program, bad[i], CLOPTS_ERROR_INVALID_NAME);
        NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
        clopts_program_destroy(program);
    }

    program = make_program("tool");
    for (i = 0; i < COUNT_OF(good); i++)
    {
        NAOS_NOTE("name=\"%s\"", good[i]);
        NAOS_CHECK_STR_EQ(clopts_command_get_name(add_command(program, good[i])), good[i]);
    }
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), COUNT_OF(good));
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- new options and positionals ---------------------------------------------- */

/* An added option or positional starts with its names and every other attribute reset. */
NAOS_TEST(added_option_and_positional_start_reset)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE,
                                                  CLOPTS_OPTION_LIST };
    char long_name[] = "output";
    char pos_name[] = "src";
    clopts_program_t *program;
    clopts_command_t *command;
    clopts_option_t *option;
    clopts_positional_t *positional;
    size_t i;

    for (i = 0; i < COUNT_OF(kinds); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        program = make_program("tool");
        option = global_add(program, kinds[i], long_name, 'o');
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
        clopts_program_destroy(program);
    }

    program = make_program("tool");
    command = add_command(program, "build");
    option = add(command, CLOPTS_OPTION_VALUE, NULL, 'o');
    NAOS_CHECK_NULL(clopts_option_get_long_name(option));
    option = add(command, CLOPTS_OPTION_FLAG, "force", '\0');
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(option), '\0');

    positional = add_positional(command, pos_name);
    pos_name[0] = 'X';
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(positional), "src");
    NAOS_CHECK_NULL(clopts_positional_get_help(positional));
    NAOS_CHECK_FALSE(clopts_positional_is_required(positional));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(positional));
    clopts_program_destroy(program);
}

/* ---- add ----------------------------------------------------------------- */

NAOS_TEST(add_links_objects_in_order)
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

    /* declared without checks, checked once at the end */
    program = clopts_program_create("tool");
    flag = clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "force", 'f');
    count = clopts_program_add_option(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    value = clopts_program_add_option(program, CLOPTS_OPTION_VALUE, NULL, 'o');
    list = clopts_program_add_option(program, CLOPTS_OPTION_LIST, "include", '\0');
    build = clopts_program_add_command(program, "build");
    copy = clopts_program_add_command(program, "copy");
    local = clopts_command_add_option(build, CLOPTS_OPTION_FLAG, "release", 'r');
    src = clopts_command_add_positional(copy, "src");
    dst = clopts_command_add_positional(copy, "dst");
    NAOS_REQUIRE_PTR_EQ(test_decl_finish(program), program);

    NAOS_REQUIRE_NOT_NULL(flag);
    NAOS_REQUIRE_NOT_NULL(count);
    NAOS_REQUIRE_NOT_NULL(value);
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_REQUIRE_NOT_NULL(build);
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_REQUIRE_NOT_NULL(local);
    NAOS_REQUIRE_NOT_NULL(src);
    NAOS_REQUIRE_NOT_NULL(dst);

    /* global options in order of addition */
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

/* A program without subcommands takes positionals, in order of addition, and has its own description. */
NAOS_TEST(program_takes_positionals)
{
    clopts_program_t *program = make_program("filt");
    clopts_positional_t *file = global_add_positional(program, "file");
    clopts_positional_t *rest = global_add_positional(program, "rest");

    clopts_program_set_description(program, "Filters lines.");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
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
    clopts_program_t *program;

    test_alloc_begin(0);
    program = clopts_program_create(name);
    NAOS_REQUIRE_NOT_NULL(program);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
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
 * A malformed name gives a program with the error INVALID_NAME; only the
 * program itself is allocated, and clopts_program_destroy() releases it. The
 * rules are those of a command name, and there are no reserved names.
 */
NAOS_TEST(program_create_checks_name)
{
    static const char *const bad[] = { NULL, "", "-x", "--x", "a b", "a\tb", "ab\n" };
    static const char *const good[] = { "help", "h", "version", "x", "a-b", "a=b", "B", "tool.exe" };
    clopts_program_t *program;
    size_t calls;
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(bad); i++)
    {
        NAOS_NOTE("name=\"%s\"", (bad[i] != NULL) ? bad[i] : "(null)");
        calls = test_alloc.calls;
        program = clopts_program_create(bad[i]);
        NAOS_REQUIRE_NOT_NULL(program);
        EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
        NAOS_CHECK_UINT_EQ(test_alloc.calls, calls + 1);
        clopts_program_destroy(program);
        NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    }

    for (i = 0; i < COUNT_OF(good); i++)
    {
        NAOS_NOTE("name=\"%s\"", good[i]);
        program = make_program(good[i]);
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
    clopts_program_set_description(program, text);
    strcpy(text, "XXXXX");
    NAOS_REQUIRE_NOT_NULL(clopts_program_get_description(program));
    NAOS_CHECK_TRUE(clopts_program_get_description(program) != text);
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "first");

    clopts_program_set_description(program, "second");
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "second");
    clopts_program_set_description(program, "");
    NAOS_REQUIRE_NOT_NULL(clopts_program_get_description(program));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "");
    clopts_program_set_description(program, NULL);
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    clopts_program_set_description(program, NULL);
    NAOS_CHECK_NULL(clopts_program_get_description(program));

    /* the name and the rest of the program do not change */
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_program_add_option() --------------------------------------------- */

/* Global options keep the order of addition and the program owns them (released with it). */
NAOS_TEST(program_add_option_keeps_the_order)
{
    clopts_program_t *program;
    clopts_option_t *options[3];

    test_alloc_begin(0);
    program = make_program("tool");
    options[0] = global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    options[1] = global_add(program, CLOPTS_OPTION_COUNT, NULL, 'v');
    options[2] = global_add(program, CLOPTS_OPTION_VALUE, "output", '\0');

    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), options[0]);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 1), options[1]);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 2), options[2]);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);

    /* global options go with subcommands and with positionals alike */
    add_command(program, "build");
    global_add(program, CLOPTS_OPTION_FLAG, "late", '\0');
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    clopts_program_destroy(program);

    program = make_program("filt");
    global_add_positional(program, "file");
    global_add(program, CLOPTS_OPTION_FLAG, "late", '\0');
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 1);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* --force/-f and --output, the global options of the DUPLICATE tests. */
static clopts_program_t *make_force_output(void)
{
    clopts_program_t *program = make_program("tool");

    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    global_add(program, CLOPTS_OPTION_VALUE, "output", '\0');
    return program;
}

/*
 * DUPLICATE: a global option with the same long or short name (exact,
 * case-sensitive). The failing call changes nothing and leaves no block
 * allocated.
 */
NAOS_TEST(program_add_option_duplicate_changes_nothing)
{
    clopts_program_t *program;

    test_alloc_begin(0);
    program = make_force_output();
    expect_add_option_fails(program, CLOPTS_OPTION_COUNT, "force", 'x', CLOPTS_ERROR_DUPLICATE);
    clopts_program_destroy(program);

    program = make_force_output();
    expect_add_option_fails(program, CLOPTS_OPTION_COUNT, "other", 'f', CLOPTS_ERROR_DUPLICATE);
    clopts_program_destroy(program);

    program = make_force_output();
    expect_add_option_fails(program, CLOPTS_OPTION_FLAG, "output", 'o', CLOPTS_ERROR_DUPLICATE);
    clopts_program_destroy(program);

    /* names differing in case are different names, and a command has its own names */
    program = make_force_output();
    global_add(program, CLOPTS_OPTION_FLAG, "Force", 'F');
    add(add_command(program, "build"), CLOPTS_OPTION_FLAG, "output", 'o');
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* A negatable --color and --no-cache, the global options of the CONFLICT tests. */
static clopts_program_t *make_color_no_cache(void)
{
    clopts_program_t *program = make_program("tool");

    clopts_option_set_negatable(global_add(program, CLOPTS_OPTION_FLAG, "color", 'c'), true);
    global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    NAOS_REQUIRE_INT_EQ(clopts_program_get_error(program), CLOPTS_ERROR_NONE);
    return program;
}

/*
 * CONFLICT: --no-x next to a negatable --x, in either order: the add of
 * --no-x, or clopts_option_set_negatable() on --x after --no-x was added. A
 * --no-x next to a --x that is not negatable is fine. The failing call
 * changes nothing.
 */
NAOS_TEST(program_add_option_negation_conflict_changes_nothing)
{
    clopts_program_t *program;
    clopts_option_t *cache;
    program_shape_t shape;

    test_alloc_begin(0);

    /* --no-color after a negatable --color */
    program = make_color_no_cache();
    expect_add_option_fails(program, CLOPTS_OPTION_FLAG, "no-color", '\0', CLOPTS_ERROR_CONFLICT);
    clopts_program_destroy(program);

    /* a negatable --cache after --no-cache */
    program = make_color_no_cache();
    cache = global_add(program, CLOPTS_OPTION_FLAG, "cache", '\0');
    shape = program_shape(program);
    clopts_option_set_negatable(cache, true);
    check_failed(program, &shape, CLOPTS_ERROR_CONFLICT);
    NAOS_CHECK_FALSE(clopts_option_is_negatable(cache));
    clopts_program_destroy(program);

    /* --cache that is not negatable, and --no-x next to a --x that is not negatable */
    program = make_color_no_cache();
    global_add(program, CLOPTS_OPTION_FLAG, "cache", '\0');
    global_add(program, CLOPTS_OPTION_FLAG, "no-cache-x", '\0');
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The errors of clopts_program_add_option() come in a fixed order:
 * INVALID_ARGUMENT, INVALID_NAME, DUPLICATE, CONFLICT.
 */
NAOS_TEST(program_add_option_reports_the_first_error)
{
    clopts_program_t *program;

    test_alloc_begin(0);

    /* --no-color breaks both scope rules; its short name is taken */
    program = make_color_no_cache();
    expect_add_option_fails(program, CLOPTS_OPTION_FLAG, "no-color", 'c', CLOPTS_ERROR_DUPLICATE);
    clopts_program_destroy(program);

    /* CONFLICT last */
    program = make_color_no_cache();
    expect_add_option_fails(program, CLOPTS_OPTION_FLAG, "no-color", 'n', CLOPTS_ERROR_CONFLICT);
    clopts_program_destroy(program);

    /* the option alone before the scope */
    program = make_color_no_cache();
    expect_add_option_fails(program, CLOPTS_OPTION_FLAG, "no-color", '-', CLOPTS_ERROR_INVALID_NAME);
    clopts_program_destroy(program);

    program = make_color_no_cache();
    expect_add_option_fails(program, (clopts_option_kind_t)7, "color", '=', CLOPTS_ERROR_INVALID_ARGUMENT);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_program_add_positional() ----------------------------------------- */

/* CONFLICT: a program with subcommands takes no positionals. The failing call changes nothing. */
NAOS_TEST(program_add_positional_mixed_layout_changes_nothing)
{
    clopts_program_t *program;

    test_alloc_begin(0);
    program = make_program("tool");
    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    add_command(program, "build");
    expect_add_positional_fails(program, "file", CLOPTS_ERROR_CONFLICT);
    clopts_program_destroy(program);

    /* a program without subcommands takes it */
    program = make_program("filt");
    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(global_add_positional(program, "file")), "file");
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* SRC (required) and DST, the positionals of the ORDER tests. */
static clopts_program_t *make_src_dst(void)
{
    clopts_program_t *program = make_program("filt");

    clopts_positional_set_required(global_add_positional(program, "SRC"), true);
    global_add_positional(program, "DST");
    NAOS_REQUIRE_INT_EQ(clopts_program_get_error(program), CLOPTS_ERROR_NONE);
    return program;
}

/*
 * ORDER: nothing after a variadic positional, and no required positional
 * after an optional one. Each failing call changes nothing.
 */
NAOS_TEST(program_add_positional_order_changes_nothing)
{
    clopts_program_t *program;
    clopts_positional_t *positional;
    program_shape_t shape;

    test_alloc_begin(0);

    /* required after optional */
    program = make_src_dst();
    positional = global_add_positional(program, "MORE");
    shape = program_shape(program);
    clopts_positional_set_required(positional, true);
    check_failed(program, &shape, CLOPTS_ERROR_ORDER);
    NAOS_CHECK_FALSE(clopts_positional_is_required(positional));
    clopts_program_destroy(program);

    /* optional after optional, then anything after the variadic one */
    program = make_src_dst();
    positional = global_add_positional(program, "REST");
    clopts_positional_set_variadic(positional, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    expect_add_positional_fails(program, "LAST", CLOPTS_ERROR_ORDER);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 3);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The errors of clopts_program_add_positional() come in a fixed order:
 * INVALID_NAME, CONFLICT, then ORDER. A program never has positionals and
 * subcommands at the same time, so CONFLICT and ORDER cannot meet; CONFLICT
 * is checked for every name.
 */
NAOS_TEST(program_add_positional_reports_the_first_error)
{
    static const char *const names[] = { "file", "tool", "build" };
    clopts_program_t *program;
    clopts_positional_t *rest;
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("name=\"%s\"", names[i]);
        program = make_program("tool");
        add_command(program, "build");
        expect_add_positional_fails(program, names[i], CLOPTS_ERROR_CONFLICT);
        clopts_program_destroy(program);
    }

    /* INVALID_NAME before CONFLICT */
    program = make_program("tool");
    add_command(program, "build");
    expect_add_positional_fails(program, "", CLOPTS_ERROR_INVALID_NAME);
    clopts_program_destroy(program);

    /* INVALID_NAME before ORDER, from the variadic positional */
    program = make_program("filt");
    rest = global_add_positional(program, "REST");
    clopts_positional_set_variadic(rest, true);
    expect_add_positional_fails(program, NULL, CLOPTS_ERROR_INVALID_NAME);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_program_add_command() -------------------------------------------- */

/*
 * The program keeps the subcommands in the order they were added, at the
 * addresses the adds returned, and releases them with itself.
 */
NAOS_TEST(program_add_command_keeps_the_order)
{
    clopts_command_t *commands[3];
    clopts_program_t *program;

    test_alloc_begin(0);
    program = make_program("tool");
    commands[0] = add_command(program, "copy");
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 1);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), commands[0]);
    commands[1] = add_command(program, "build");
    commands[2] = add_command(program, "cat");
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
 * DUPLICATE: a subcommand of the same name (exact, case-sensitive). The
 * failing call changes nothing; the first command keeps what was added to it.
 */
NAOS_TEST(program_add_command_duplicate_changes_nothing)
{
    clopts_program_t *program;
    clopts_command_t *build;

    test_alloc_begin(0);
    program = make_program("tool");
    build = add_command(program, "build");
    add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    add_command(program, "copy");

    expect_add_command_fails(program, "build", CLOPTS_ERROR_DUPLICATE);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "build"), build);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 1);
    clopts_program_destroy(program);

    /* a subcommand named like the program and names differing in case are fine */
    program = make_program("tool");
    add_command(program, "build");
    add_command(program, "copy");
    add_command(program, "Build");
    add_command(program, "tool");
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 4);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * CONFLICT: a program with positionals takes no subcommands. Global options
 * with subcommands, and positionals without, are fine. The failing call
 * changes nothing.
 */
NAOS_TEST(program_add_command_mixed_layout_changes_nothing)
{
    clopts_program_t *program;
    clopts_command_t *build;

    test_alloc_begin(0);
    program = make_program("tool");
    global_add_positional(program, "file");
    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    expect_add_command_fails(program, "build", CLOPTS_ERROR_CONFLICT);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 1);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "build"));
    clopts_program_destroy(program);

    /* global options with a subcommand */
    program = make_program("tool");
    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    build = add_command(program, "build");
    NAOS_CHECK_PTR_EQ(command_at(program, 0), build);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The errors of clopts_program_add_command() come in a fixed order:
 * INVALID_NAME, DUPLICATE, CONFLICT. A program never has positionals and
 * subcommands at the same time, so DUPLICATE and CONFLICT cannot meet;
 * CONFLICT is checked for every name, also the name of the program.
 */
NAOS_TEST(program_add_command_reports_the_first_error)
{
    static const char *const names[] = { "build", "filt", "help" };
    clopts_program_t *program;
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("name=\"%s\"", names[i]);
        program = make_program("filt");
        global_add_positional(program, "file");
        expect_add_command_fails(program, names[i], CLOPTS_ERROR_CONFLICT);
        clopts_program_destroy(program);
    }

    /* INVALID_NAME before CONFLICT */
    program = make_program("filt");
    global_add_positional(program, "file");
    expect_add_command_fails(program, "-x", CLOPTS_ERROR_INVALID_NAME);
    clopts_program_destroy(program);

    program = make_program("tool");
    add_command(program, "build");
    expect_add_command_fails(program, "build", CLOPTS_ERROR_DUPLICATE);
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
    clopts_program_t *program = make_program("tool");
    clopts_command_t *build = add_command(program, "build");
    clopts_command_t *help = add_command(program, "help");

    clopts_option_set_negatable(global_add(program, CLOPTS_OPTION_FLAG, "color", 'c'), true);
    global_add(program, CLOPTS_OPTION_FLAG, "help", 'h');
    global_add(program, CLOPTS_OPTION_FLAG, "version", 'V');
    add(build, CLOPTS_OPTION_VALUE, "color", 'c');
    add(build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    add(build, CLOPTS_OPTION_COUNT, "help", 'h');
    add(help, CLOPTS_OPTION_COUNT, "help", 'h');

    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 3);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(help)), 1);
    clopts_program_destroy(program);
}

/* ---- read functions ---------------------------------------------------------- */

/*
 * The read functions of a program: name, description, the global options and
 * positionals in order of addition, the subcommands in the order they were
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
    clopts_program_set_description(program, "A tool.");
    force = global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    verbose = global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    commands[0] = add_command(program, "copy");
    commands[1] = add_command(program, "build");
    commands[2] = add_command(program, "Build");

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
    file = global_add_positional(program, "file");
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

/* The read functions of a command: name, texts, and the options and positionals in order of addition. */
NAOS_TEST(command_read_functions)
{
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *force;
    clopts_positional_t *src;
    clopts_positional_t *dst;
    clopts_program_t *program;

    program = make_program("tool");
    build = add_command(program, "build");
    clopts_command_set_summary(build, "Builds.");
    clopts_command_set_description(build, "Builds the tree.");
    clopts_program_set_description(program, "A tool.");
    output = add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    force = add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    src = add_positional(build, "src");
    dst = add_positional(build, "dst");

    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
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
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);
    clopts_program_destroy(program);
}

/* The read functions of an option give back every attribute. Attributes not set read as NULL, '\0', 0 or false. */
NAOS_TEST(option_read_functions)
{
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *count;
    clopts_option_t *flag;
    clopts_option_t *bare;
    clopts_program_t *program;

    program = make_program("tool");
    value = global_add(program, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_help(value, "Output file.");
    clopts_option_set_value_name(value, "FILE");
    clopts_option_set_env(value, "TOOL_OUTPUT");
    clopts_option_set_default(value, "a.out");
    list = global_add(program, CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_delimiter(list, ',');
    clopts_option_set_required(list, true);
    count = global_add(program, CLOPTS_OPTION_COUNT, NULL, 'v');
    clopts_option_set_count_max(count, 3);
    flag = global_add(program, CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(flag, true);
    clopts_option_set_terminal(flag, true);
    bare = global_add(program, CLOPTS_OPTION_VALUE, "bare", '\0');
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);

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
    NAOS_CHECK_PTR_EQ(global_at(program, 3), flag);
    clopts_program_destroy(program);
}

/* The read functions of a positional. */
NAOS_TEST(positional_read_functions)
{
    clopts_positional_t *src;
    clopts_positional_t *rest;
    clopts_program_t *program;

    program = make_program("tool");
    src = global_add_positional(program, "SRC");
    clopts_positional_set_help(src, "Source.");
    clopts_positional_set_required(src, true);
    rest = global_add_positional(program, "REST");
    clopts_positional_set_variadic(rest, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);

    check_str_or_null(clopts_positional_get_name(src), "SRC");
    check_str_or_null(clopts_positional_get_help(src), "Source.");
    NAOS_CHECK_TRUE(clopts_positional_is_required(src));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(src));
    check_str_or_null(clopts_positional_get_name(rest), "REST");
    NAOS_CHECK_NULL(clopts_positional_get_help(rest));
    NAOS_CHECK_FALSE(clopts_positional_is_required(rest));
    NAOS_CHECK_TRUE(clopts_positional_is_variadic(rest));
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(program), 1), rest);
    clopts_program_destroy(program);
}

/* ---- terminal ------------------------------------------------------------------ */

/*
 * clopts_option_set_terminal(): FLAG only (NOT_APPLICABLE for the other
 * kinds, while false resets on any kind).
 */
NAOS_TEST(set_terminal_is_for_flags)
{
    static const clopts_option_kind_t others[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_option_t *flag;
    clopts_option_t *option;
    clopts_program_t *program;
    size_t i;

    for (i = 0; i < COUNT_OF(others); i++)
    {
        NAOS_NOTE("kind=%d", (int)others[i]);
        program = make_program("tool");
        option = global_add(program, others[i], "other", 'o');
        clopts_option_set_terminal(option, true);
        EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
        NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
        clopts_program_destroy(program);

        program = make_program("tool");
        option = global_add(program, others[i], "other", 'o');
        clopts_option_set_terminal(option, false);
        EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
        NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
        clopts_program_destroy(program);
    }

    program = make_program("tool");
    flag = global_add(program, CLOPTS_OPTION_FLAG, "help", 'h');
    clopts_option_set_terminal(flag, true);
    NAOS_CHECK_TRUE(clopts_option_is_terminal(flag));
    clopts_option_set_terminal(flag, true);
    NAOS_CHECK_TRUE(clopts_option_is_terminal(flag));
    clopts_option_set_terminal(flag, false);
    NAOS_CHECK_FALSE(clopts_option_is_terminal(flag));
    clopts_option_set_terminal(flag, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_TRUE(clopts_option_is_terminal(global_at(program, 0)));
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
    build = add_command(program, cmd_name);
    output = add(build, CLOPTS_OPTION_VALUE, long_name, 'o');
    src = add_positional(build, pos_name);
    memset(program_name, 'X', sizeof(program_name) - 1);
    memset(cmd_name, 'X', sizeof(cmd_name) - 1);
    memset(long_name, 'X', sizeof(long_name) - 1);
    memset(pos_name, 'X', sizeof(pos_name) - 1);

    strcpy(text, "Builds.");
    clopts_command_set_summary(build, text);
    strcpy(text, "XXXXXXX");
    strcpy(text, "Builds the tree.");
    clopts_command_set_description(build, text);
    strcpy(text, "XXXXXXXXXXXXXXXX");
    strcpy(text, "Program text.");
    clopts_program_set_description(program, text);
    strcpy(text, "XXXXXXXXXXXXX");
    strcpy(text, "Output file.");
    clopts_option_set_help(output, text);
    strcpy(text, "XXXXXXXXXXXX");
    strcpy(text, "FILE");
    clopts_option_set_value_name(output, text);
    strcpy(text, "XXXX");
    strcpy(text, "TOOL_OUTPUT");
    clopts_option_set_env(output, text);
    strcpy(text, "XXXXXXXXXXX");
    strcpy(text, "a.out");
    clopts_option_set_default(output, text);
    strcpy(text, "XXXXX");
    strcpy(text, "Source.");
    clopts_positional_set_help(src, text);
    strcpy(text, "XXXXXXX");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);

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
    clopts_positional_t *src;

    program = make_program("tool");
    build = add_command(program, "build");
    clopts_command_set_summary(build, "Builds.");
    clopts_command_set_summary(build, clopts_command_get_summary(build));
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(build), "Builds.");
    clopts_command_set_description(build, "Builds the tree.");
    clopts_command_set_description(build, clopts_command_get_description(build));
    NAOS_CHECK_STR_EQ(clopts_command_get_description(build), "Builds the tree.");

    output = add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_help(output, "Output file.");
    clopts_option_set_help(output, clopts_option_get_help(output));
    NAOS_CHECK_STR_EQ(clopts_option_get_help(output), "Output file.");

    src = add_positional(build, "src");
    clopts_positional_set_help(src, "Source.");
    clopts_positional_set_help(src, clopts_positional_get_help(src));
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(src), "Source.");

    clopts_program_set_description(program, "A tool.");
    clopts_program_set_description(program, clopts_program_get_description(program));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "A tool.");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
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
    build = add_command(program, "build");
    value = global_add(program, CLOPTS_OPTION_VALUE, "output", 'o');
    list = global_add(program, CLOPTS_OPTION_LIST, "include", 'I');
    count = global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    flag = global_add(program, CLOPTS_OPTION_FLAG, "color", '\0');
    src = add_positional(build, "src");

    clopts_program_set_description(program, "first");
    clopts_program_set_description(program, "second");
    clopts_command_set_summary(build, "first");
    clopts_command_set_summary(build, "second");
    clopts_command_set_description(build, "first");
    clopts_command_set_description(build, "second");
    clopts_option_set_help(value, "first");
    clopts_option_set_help(value, "second");
    clopts_option_set_value_name(value, "A");
    clopts_option_set_value_name(value, "B");
    clopts_option_set_env(value, "ENV_A");
    clopts_option_set_env(value, "ENV_B");
    clopts_option_set_default(value, "x");
    clopts_option_set_default(value, "y");
    clopts_option_set_delimiter(list, ',');
    clopts_option_set_delimiter(list, ':');
    clopts_option_set_count_max(count, 3);
    clopts_option_set_count_max(count, 8);
    clopts_option_set_negatable(flag, true);
    clopts_option_set_terminal(flag, true);
    clopts_option_set_required(list, true);
    clopts_positional_set_help(src, "first");
    clopts_positional_set_help(src, "second");
    clopts_positional_set_required(src, true);
    clopts_positional_set_variadic(src, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);

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
    clopts_program_set_description(program, NULL);
    clopts_command_set_summary(build, NULL);
    clopts_command_set_description(build, NULL);
    clopts_option_set_help(value, NULL);
    clopts_option_set_value_name(value, NULL);
    clopts_option_set_env(value, NULL);
    clopts_option_set_default(value, NULL);
    clopts_option_set_delimiter(list, '\0');
    clopts_option_set_count_max(count, 0);
    clopts_option_set_negatable(flag, false);
    clopts_option_set_terminal(flag, false);
    clopts_option_set_required(list, false);
    clopts_positional_set_help(src, NULL);
    clopts_positional_set_variadic(src, false);
    clopts_positional_set_required(src, false);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);

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

    clopts_program_destroy(program);
}

/* ---- handle stability ---------------------------------------------------- */

NAOS_TEST(hundred_options_keep_the_first_handle)
{
    char name[16];
    clopts_program_t *program;
    clopts_option_t *first;
    clopts_option_t *option;
    int i;

    program = make_program("tool");

    first = global_add(program, CLOPTS_OPTION_VALUE, "o0", 'a');
    clopts_option_set_help(first, "first help");
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "o%d", i);
        NAOS_NOTE("i=%d", i);
        option = global_add(program, CLOPTS_OPTION_FLAG, name, '\0');
        NAOS_CHECK_PTR_EQ(global_at(program, (size_t)i), option);
    }
    for (i = 0; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "p%d", i);
        NAOS_NOTE("i=%d", i);
        global_add_positional(program, name);
    }

    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 100);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 100);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), first);
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(first), CLOPTS_OPTION_VALUE);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(first), "o0");
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(first), 'a');
    NAOS_CHECK_STR_EQ(clopts_option_get_help(first), "first help");
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(clopts_scope_get_option_at(clopts_program_get_scope(program), 99)),
                      "o99");
    NAOS_CHECK_STR_EQ(
        clopts_positional_get_name(clopts_scope_get_positional_at(clopts_program_get_scope(program), 99)), "p99");

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
    first = add_command(program, "c0");
    clopts_command_set_summary(first, "first summary");
    add(first, CLOPTS_OPTION_FLAG, "force", 'f');
    commands[0] = first;
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "c%d", i);
        NAOS_NOTE("i=%d", i);
        commands[i] = add_command(program, name);
        NAOS_CHECK_PTR_EQ(command_at(program, 0), first);
    }

    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
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

/* The handles of one out-of-memory case. */
typedef struct oom_ctx
{
    clopts_program_t *program;
    clopts_command_t *command;
    clopts_option_t *option;
    clopts_positional_t *positional;
    const void *added; /* what an add returned */
} oom_ctx_t;

/* One allocating builder call under fault injection. */
typedef struct oom_case
{
    const char *name;
    /* Declares what the call needs, without fault injection (NULL: nothing, the program is NULL). */
    void (*setup)(oom_ctx_t *c);
    /* Makes the call. */
    void (*call)(oom_ctx_t *c);
    /* Checks after a failed call that nothing changed (may be NULL). */
    void (*unchanged)(oom_ctx_t *c);
    /* true for an add: c->added is NULL after a failure and not NULL after a success. */
    bool adds;
} oom_case_t;

/*
 * The program "tool" (description "old") with the global VALUE --output/-o
 * (help, value name, env and default "old") and the subcommand build
 * (summary and description "old") with the positional SRC (help "old").
 */
static void oom_setup_tool(oom_ctx_t *c)
{
    c->program = clopts_program_create("tool");
    clopts_program_set_description(c->program, "old");
    c->option = clopts_program_add_option(c->program, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_help(c->option, "old");
    clopts_option_set_value_name(c->option, "old");
    clopts_option_set_env(c->option, "old");
    clopts_option_set_default(c->option, "old");
    c->command = clopts_program_add_command(c->program, "build");
    clopts_command_set_summary(c->command, "old");
    clopts_command_set_description(c->command, "old");
    c->positional = clopts_command_add_positional(c->command, "SRC");
    clopts_positional_set_help(c->positional, "old");
    NAOS_REQUIRE_INT_EQ(clopts_program_get_error(c->program), CLOPTS_ERROR_NONE);
}

/* The empty program "filt", without subcommands. */
static void oom_setup_filt(oom_ctx_t *c)
{
    c->program = make_program("filt");
}

/* The tool of oom_setup_tool(), as it was declared. */
static void oom_tool_unchanged(oom_ctx_t *c)
{
    NAOS_CHECK_STR_EQ(clopts_program_get_description(c->program), "old");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(c->program)), 1);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(c->program), 1);
    NAOS_CHECK_NULL(clopts_program_find_command(c->program, "copy"));
    NAOS_CHECK_STR_EQ(clopts_option_get_help(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_get_value_name(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_get_env(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_option_get_default(c->option), "old");
    NAOS_CHECK_STR_EQ(clopts_command_get_summary(c->command), "old");
    NAOS_CHECK_STR_EQ(clopts_command_get_description(c->command), "old");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(c->command)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(c->command)), 1);
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(c->positional), "old");
}

/* The filt of oom_setup_filt(), still empty. */
static void oom_filt_unchanged(oom_ctx_t *c)
{
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(c->program)), 0);
}

static void oom_program_create(oom_ctx_t *c)
{
    c->program = clopts_program_create("tool");
    c->added = c->program;
}

static void oom_program_description(oom_ctx_t *c)
{
    clopts_program_set_description(c->program, "new");
}

static void oom_program_add_option(oom_ctx_t *c)
{
    c->added = clopts_program_add_option(c->program, CLOPTS_OPTION_FLAG, "color", '\0');
}

static void oom_program_add_positional(oom_ctx_t *c)
{
    c->added = clopts_program_add_positional(c->program, "file");
}

static void oom_program_add_command(oom_ctx_t *c)
{
    c->added = clopts_program_add_command(c->program, "copy");
}

static void oom_program_add_help_option(oom_ctx_t *c)
{
    c->added = clopts_program_add_help_option(c->program);
}

static void oom_program_add_version_option(oom_ctx_t *c)
{
    c->added = clopts_program_add_version_option(c->program);
}

static void oom_command_summary(oom_ctx_t *c)
{
    clopts_command_set_summary(c->command, "new");
}

static void oom_command_description(oom_ctx_t *c)
{
    clopts_command_set_description(c->command, "new");
}

static void oom_command_add_option(oom_ctx_t *c)
{
    c->added = clopts_command_add_option(c->command, CLOPTS_OPTION_VALUE, "jobs", 'j');
}

static void oom_command_add_positional(oom_ctx_t *c)
{
    c->added = clopts_command_add_positional(c->command, "DST");
}

static void oom_option_help(oom_ctx_t *c)
{
    clopts_option_set_help(c->option, "new");
}

static void oom_option_value_name(oom_ctx_t *c)
{
    clopts_option_set_value_name(c->option, "new");
}

static void oom_option_env(oom_ctx_t *c)
{
    clopts_option_set_env(c->option, "new");
}

static void oom_option_default(oom_ctx_t *c)
{
    clopts_option_set_default(c->option, "new");
}

static void oom_positional_help(oom_ctx_t *c)
{
    clopts_positional_set_help(c->positional, "new");
}

/*
 * Makes the call of oc with its n-th fault point failing, for n = 1, 2, ...
 * until it succeeds, each time on a new declaration from oc->setup (a failed
 * call leaves its error in the program, which then does nothing more). Each
 * failing run must leave OUT_OF_MEMORY in the program (or a NULL program),
 * return NULL from an add, leave the number of live blocks as it was, and
 * pass oc->unchanged(). Returns the number of failing runs; the last run
 * succeeds.
 */
static size_t fail_each_point(const oom_case_t *oc)
{
    oom_ctx_t c;
    clopts_error_t error;
    long live;
    size_t n;

    for (n = 1; n < 20; n++)
    {
        NAOS_NOTE("%s: fault point %u", oc->name, (unsigned)n);
        memset(&c, 0, sizeof(c));
        if (oc->setup != NULL)
        {
            oc->setup(&c);
        }
        live = test_alloc.live;
        test_alloc_set_fail_at(test_alloc.calls + n);
        oc->call(&c);
        test_alloc_set_fail_at(0);
        error = clopts_program_get_error(c.program);
        if (error == CLOPTS_ERROR_NONE)
        {
            if (oc->adds)
            {
                NAOS_CHECK_NOT_NULL(c.added);
            }
            clopts_program_destroy(c.program);
            NAOS_CHECK_INT_EQ(test_alloc.live, 0);
            return n - 1;
        }
        NAOS_CHECK_INT_EQ(error, CLOPTS_ERROR_OUT_OF_MEMORY);
        if (oc->adds)
        {
            NAOS_CHECK_NULL(c.added);
        }
        NAOS_CHECK_INT_EQ(test_alloc.live, live);
        if (oc->unchanged != NULL)
        {
            oc->unchanged(&c);
        }
        clopts_program_destroy(c.program);
        NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    }
    /*
     * The call never succeeded. Reported with a check that fails here (n is 20)
     * rather than NAOS_FAIL, which does not return and would make the return
     * unreachable (MSVC C4702).
     */
    NAOS_CHECK_TRUE(n < 20);
    return n;
}

/*
 * Every fault point of every allocating builder function: the call leaves
 * OUT_OF_MEMORY in the program, returns NULL from an add, leaves no block
 * allocated and changes nothing, and the same call succeeds once allocation
 * works again.
 */
NAOS_TEST(out_of_memory_changes_nothing)
{
    static const oom_case_t cases[] = {
        { "clopts_program_set_description", oom_setup_tool, oom_program_description, oom_tool_unchanged, false },
        { "clopts_program_add_option", oom_setup_tool, oom_program_add_option, oom_tool_unchanged, true },
        { "clopts_program_add_positional", oom_setup_filt, oom_program_add_positional, oom_filt_unchanged, true },
        { "clopts_program_add_command", oom_setup_tool, oom_program_add_command, oom_tool_unchanged, true },
        { "clopts_command_set_summary", oom_setup_tool, oom_command_summary, oom_tool_unchanged, false },
        { "clopts_command_set_description", oom_setup_tool, oom_command_description, oom_tool_unchanged, false },
        { "clopts_command_add_option", oom_setup_tool, oom_command_add_option, oom_tool_unchanged, true },
        { "clopts_command_add_positional", oom_setup_tool, oom_command_add_positional, oom_tool_unchanged, true },
        { "clopts_option_set_help", oom_setup_tool, oom_option_help, oom_tool_unchanged, false },
        { "clopts_option_set_value_name", oom_setup_tool, oom_option_value_name, oom_tool_unchanged, false },
        { "clopts_option_set_env", oom_setup_tool, oom_option_env, oom_tool_unchanged, false },
        { "clopts_option_set_default", oom_setup_tool, oom_option_default, oom_tool_unchanged, false },
        { "clopts_positional_set_help", oom_setup_tool, oom_positional_help, oom_tool_unchanged, false },
    };
    static const oom_case_t create = { "clopts_program_create", NULL, oom_program_create, NULL, true };
    size_t i;

    test_alloc_begin(0);

    /* the program and its name: a NULL program, whose error is OUT_OF_MEMORY */
    NAOS_CHECK_UINT_EQ(fail_each_point(&create), 2);

    for (i = 0; i < COUNT_OF(cases); i++)
    {
        NAOS_CHECK_UINT_GT(fail_each_point(&cases[i]), 0);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * As out_of_memory_changes_nothing, for clopts_program_add_help_option() and
 * clopts_program_add_version_option(). Registered as an expected failure:
 * when the copy of the help text runs out of memory, the call has already
 * added the option, and leaves it in the program (src/decl/program.c,
 * program_add_terminal()), against the rule that a failing builder function
 * changes nothing (clopts/decl/program.h).
 */
NAOS_TEST(out_of_memory_in_help_and_version_option_changes_nothing)
{
    static const oom_case_t cases[] = {
        { "clopts_program_add_help_option", oom_setup_tool, oom_program_add_help_option, oom_tool_unchanged, true },
        { "clopts_program_add_version_option", oom_setup_tool, oom_program_add_version_option, oom_tool_unchanged,
          true },
    };
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(cases); i++)
    {
        NAOS_CHECK_UINT_GT(fail_each_point(&cases[i]), 0);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Declares a small program, with one subcommand, through every allocating
 * builder function and without a single check, as a caller does. With fault
 * injection, everything after the failed allocation does nothing.
 */
static void build_sample(clopts_program_t *program)
{
    clopts_command_t *build;
    clopts_option_t *option;
    clopts_positional_t *positional;

    clopts_program_set_description(program, "A tool.");
    clopts_program_add_option(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    option = clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(option, true);
    clopts_program_add_help_option(program);
    clopts_program_add_version_option(program);
    build = clopts_program_add_command(program, "build");
    clopts_command_set_summary(build, "Builds.");
    clopts_command_set_description(build, "Builds the tree.");
    option = clopts_command_add_option(build, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_help(option, "Output file.");
    clopts_option_set_value_name(option, "FILE");
    clopts_option_set_env(option, "TOOL_OUTPUT");
    clopts_option_set_default(option, "a.out");
    option = clopts_command_add_option(build, CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_delimiter(option, ',');
    clopts_command_add_option(build, CLOPTS_OPTION_FLAG, "release", '\0');
    positional = clopts_command_add_positional(build, "src");
    clopts_positional_set_help(positional, "Source.");
    clopts_positional_set_required(positional, true);
    positional = clopts_command_add_positional(build, "rest");
    clopts_positional_set_variadic(positional, true);
    test_decl_help_command(program);
}

/*
 * Moves the failing fault point fail_at = 1, 2, ... over
 * clopts_program_create() and build_sample() until a run passes no failing
 * fault point (test_alloc.calls < fail_at). The first two fault points are in
 * clopts_program_create(), which then gives NULL. Every run ends with the
 * error OUT_OF_MEMORY (it passed a failing fault point) or NONE (it did not),
 * and clopts_program_destroy() releases whatever was made.
 */
NAOS_TEST(every_failure_point_is_released)
{
    clopts_program_t *program;
    clopts_error_t error;
    size_t fail_at;
    bool succeeded = false;

    for (fail_at = 1; (fail_at < 300) && (!succeeded); fail_at++)
    {
        NAOS_NOTE("fail_at=%u", (unsigned)fail_at);
        test_alloc_begin(fail_at);
        program = clopts_program_create("tool");
        if (program == NULL)
        {
            /* Only the first two fault points are in clopts_program_create(). */
            NAOS_CHECK_UINT_LE(fail_at, 2);
            NAOS_CHECK_UINT_EQ(test_alloc.calls, fail_at);
        }

        build_sample(program);
        error = clopts_program_get_error(program);
        if (test_alloc.calls < fail_at)
        {
            NAOS_CHECK_INT_EQ(error, CLOPTS_ERROR_NONE);
            NAOS_CHECK_NOT_NULL(program);
            succeeded = true;
        }
        else
        {
            NAOS_CHECK_INT_EQ(error, CLOPTS_ERROR_OUT_OF_MEMORY);
            /* nothing allocates after the failed fault point */
            NAOS_CHECK_UINT_EQ(test_alloc.calls, fail_at);
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
NAOS_TEST(program_destroy_releases_everything)
{
    clopts_program_t *program;

    test_alloc_begin(0);
    program = clopts_program_create("tool");
    build_sample(program);
    NAOS_REQUIRE_PTR_EQ(test_decl_finish(program), program);
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
    build = add_command(program, "build");
    flag = global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');

    parse_name_only(program, "tool", CLOPTS_PARSE_NO_COMMAND);
    parse_name_only(program, "tool", CLOPTS_PARSE_NO_COMMAND);
    test_alloc_set_fail_at(test_alloc.calls + 1);
    parse_name_only(program, "tool", CLOPTS_PARSE_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    parse_name_only(program, "tool", CLOPTS_PARSE_NO_COMMAND);

    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
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
    NAOS_ADD_TEST(destroy_null_does_nothing),
    NAOS_ADD_TEST(add_command_copies_name_and_starts_empty),
    NAOS_ADD_TEST(add_command_checks_name),
    NAOS_ADD_TEST(added_option_and_positional_start_reset),
    NAOS_ADD_TEST(add_links_objects_in_order),
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
    NAOS_ADD_TEST(out_of_memory_in_help_and_version_option_changes_nothing),
    NAOS_ADD_TEST(every_failure_point_is_released),
    NAOS_ADD_TEST(program_destroy_releases_everything),
    NAOS_ADD_TEST(parse_leaves_the_program_as_it_is)
)
