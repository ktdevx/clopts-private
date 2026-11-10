/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl_error: the declaration error of a program and the rules every
 * builder function shares (clopts/decl/program.h, "Declaration API"):
 *
 *   - the program keeps the first error, whichever scope (the program or one
 *     of its commands) or handle reported it;
 *   - once the program has an error, every builder function on it or on
 *     anything added to it does nothing (it allocates nothing either), and
 *     the add functions return NULL;
 *   - every builder function accepts NULL handles and does nothing;
 *   - clopts_program_get_error(NULL) is OUT_OF_MEMORY, and
 *     clopts_program_create() with an invalid name gives a program whose
 *     error is INVALID_NAME;
 *   - clopts_program_add_help_option() / add_version_option() add terminal
 *     global FLAGs with fixed names and help.
 *
 * A failing call changes nothing; the tests read the handles of a program
 * with an error only to see that.
 */

#include "clopts/decl/program.h"
#include "clopts/parse/parse.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

/* ---- helpers ----------------------------------------------------------- */

/* Number of elements of the array a. */
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* Checks that program has the declaration error error (CLOPTS_ERROR_NONE: none). */
#define EXPECT_ERROR(program, error) NAOS_CHECK_INT_EQ(clopts_program_get_error(program), (error))

/* A new empty program "tool"; the test stops when it cannot be made. */
static clopts_program_t *new_program(void)
{
    clopts_program_t *program = test_decl_program("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/*
 * A declaration with a handle of every kind, each with some attributes set:
 * global options of every kind, a subcommand with an option and positionals,
 * and a second program without subcommands for the positionals of a program.
 */
typedef struct sample
{
    clopts_program_t *program;
    clopts_option_t *value;     /* --output/-o VALUE, help, value name, env, default */
    clopts_option_t *list;      /* --include/-I LIST, delimiter ',', required */
    clopts_option_t *count;     /* -v COUNT, count max 3 */
    clopts_option_t *flag;      /* --color FLAG, negatable */
    clopts_option_t *help;      /* --help/-h, clopts_program_add_help_option() */
    clopts_command_t *build;    /* summary, description */
    clopts_option_t *release;   /* build --release/-r FLAG */
    clopts_positional_t *src;   /* build SRC, required, help */
    clopts_positional_t *rest;  /* build REST, variadic */
} sample_t;

/* Builds the sample; the test stops when a call fails. */
static sample_t make_sample(void)
{
    sample_t s;

    s.program = new_program();
    clopts_program_set_description(s.program, "old");
    s.value = clopts_program_add_option(s.program, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_help(s.value, "old");
    clopts_option_set_value_name(s.value, "OLD");
    clopts_option_set_env(s.value, "OLD");
    clopts_option_set_default(s.value, "old");
    s.list = clopts_program_add_option(s.program, CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_delimiter(s.list, ',');
    clopts_option_set_required(s.list, true);
    s.count = clopts_program_add_option(s.program, CLOPTS_OPTION_COUNT, NULL, 'v');
    clopts_option_set_count_max(s.count, 3);
    s.flag = clopts_program_add_option(s.program, CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(s.flag, true);
    s.help = clopts_program_add_help_option(s.program);
    s.build = clopts_program_add_command(s.program, "build");
    clopts_command_set_summary(s.build, "old");
    clopts_command_set_description(s.build, "old");
    s.release = clopts_command_add_option(s.build, CLOPTS_OPTION_FLAG, "release", 'r');
    s.src = clopts_command_add_positional(s.build, "SRC");
    clopts_positional_set_required(s.src, true);
    clopts_positional_set_help(s.src, "old");
    s.rest = clopts_command_add_positional(s.build, "REST");
    clopts_positional_set_variadic(s.rest, true);

    NAOS_REQUIRE_INT_EQ(clopts_program_get_error(s.program), CLOPTS_ERROR_NONE);
    NAOS_REQUIRE_NOT_NULL(s.value);
    NAOS_REQUIRE_NOT_NULL(s.list);
    NAOS_REQUIRE_NOT_NULL(s.count);
    NAOS_REQUIRE_NOT_NULL(s.flag);
    NAOS_REQUIRE_NOT_NULL(s.help);
    NAOS_REQUIRE_NOT_NULL(s.build);
    NAOS_REQUIRE_NOT_NULL(s.release);
    NAOS_REQUIRE_NOT_NULL(s.src);
    NAOS_REQUIRE_NOT_NULL(s.rest);
    return s;
}

/* Checks that the sample is as make_sample() built it. */
static void check_sample_unchanged(const sample_t *s)
{
    const clopts_scope_t *global = clopts_program_get_scope(s->program);
    const clopts_scope_t *build = clopts_command_get_scope(s->build);

    NAOS_CHECK_STR_EQ(clopts_program_get_name(s->program), "tool");
    NAOS_CHECK_STR_EQ(clopts_program_get_description(s->program), "old");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(global), 5);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(global), 0);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(s->program), 1);

    NAOS_CHECK_STR_EQ(clopts_option_get_help(s->value), "old");
    NAOS_CHECK_STR_EQ(clopts_option_get_value_name(s->value), "OLD");
    NAOS_CHECK_STR_EQ(clopts_option_get_env(s->value), "OLD");
    NAOS_CHECK_STR_EQ(clopts_option_get_default(s->value), "old");
    NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(s->list), ',');
    NAOS_CHECK_TRUE(clopts_option_is_required(s->list));
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(s->count), 3);
    NAOS_CHECK_TRUE(clopts_option_is_negatable(s->flag));
    NAOS_CHECK_FALSE(clopts_option_is_terminal(s->flag));
    NAOS_CHECK_TRUE(clopts_option_is_terminal(s->help));
    NAOS_CHECK_STR_EQ(clopts_option_get_help(s->help), "Show this help");

    NAOS_CHECK_STR_EQ(clopts_command_get_summary(s->build), "old");
    NAOS_CHECK_STR_EQ(clopts_command_get_description(s->build), "old");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(build), 1);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(build), 2);
    NAOS_CHECK_FALSE(clopts_option_is_terminal(s->release));
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(s->src), "old");
    NAOS_CHECK_TRUE(clopts_positional_is_required(s->src));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(s->src));
    NAOS_CHECK_FALSE(clopts_positional_is_required(s->rest));
    NAOS_CHECK_TRUE(clopts_positional_is_variadic(s->rest));
}

/*
 * Calls every builder function on the sample and on its handles, with
 * arguments that would succeed on a program without an error (and some that
 * would fail with another error), and checks that each add returns NULL.
 */
static void call_every_builder(const sample_t *s)
{
    clopts_program_set_description(s->program, "new");
    clopts_program_set_description(s->program, NULL);
    NAOS_CHECK_NULL(clopts_program_add_option(s->program, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_CHECK_NULL(clopts_program_add_option(s->program, CLOPTS_OPTION_FLAG, "output", '\0'));
    NAOS_CHECK_NULL(clopts_program_add_option(s->program, (clopts_option_kind_t)9, NULL, '\0'));
    NAOS_CHECK_NULL(clopts_program_add_positional(s->program, "FILE"));
    NAOS_CHECK_NULL(clopts_program_add_command(s->program, "copy"));
    NAOS_CHECK_NULL(clopts_program_add_command(s->program, "build"));
    NAOS_CHECK_NULL(clopts_program_add_help_option(s->program));
    NAOS_CHECK_NULL(clopts_program_add_version_option(s->program));

    clopts_command_set_summary(s->build, "new");
    clopts_command_set_description(s->build, NULL);
    NAOS_CHECK_NULL(clopts_command_add_option(s->build, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_CHECK_NULL(clopts_command_add_positional(s->build, "MORE"));

    clopts_option_set_help(s->value, "new");
    clopts_option_set_value_name(s->value, NULL);
    clopts_option_set_env(s->value, "NEW");
    clopts_option_set_default(s->value, NULL);
    clopts_option_set_required(s->value, true);
    clopts_option_set_delimiter(s->list, ':');
    clopts_option_set_required(s->list, false);
    clopts_option_set_count_max(s->count, 0);
    clopts_option_set_negatable(s->flag, false);
    clopts_option_set_terminal(s->flag, true);
    clopts_option_set_terminal(s->help, false);
    clopts_option_set_help(s->help, NULL);
    clopts_option_set_negatable(s->release, true);
    clopts_option_set_terminal(s->release, true);
    clopts_option_set_terminal(s->count, true); /* would be NOT_APPLICABLE */

    clopts_positional_set_help(s->src, "new");
    clopts_positional_set_required(s->src, false);
    clopts_positional_set_variadic(s->src, true); /* would be ORDER */
    clopts_positional_set_variadic(s->rest, false);
    clopts_positional_set_required(s->rest, true);
}

/* ---- clopts_program_get_error() and clopts_program_create() ------------------ */

/* A NULL program is what clopts_program_create() gives when it runs out of memory. */
NAOS_TEST(get_error_of_null_program_is_out_of_memory)
{
    NAOS_CHECK_INT_EQ(clopts_program_get_error(NULL), CLOPTS_ERROR_OUT_OF_MEMORY);
}

NAOS_TEST(new_program_has_no_error)
{
    clopts_program_t *program = clopts_program_create("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/*
 * clopts_program_create() with an invalid name gives a program (not NULL)
 * whose error is INVALID_NAME. The builder functions do nothing on it, and
 * clopts_program_destroy() releases it.
 */
NAOS_TEST(program_create_with_invalid_name_keeps_the_error)
{
    static const char *const names[] = { NULL, "", "-t", "--tool", "my tool", "tool\n", "my\ttool" };
    clopts_program_t *program;
    size_t calls;
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("name=\"%s\"", (names[i] != NULL) ? names[i] : "(null)");
        program = clopts_program_create(names[i]);
        NAOS_REQUIRE_NOT_NULL(program);
        EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);

        calls = test_alloc.calls;
        clopts_program_set_description(program, "A tool.");
        NAOS_CHECK_NULL(clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "force", 'f'));
        NAOS_CHECK_NULL(clopts_program_add_positional(program, "FILE"));
        NAOS_CHECK_NULL(clopts_program_add_command(program, "build"));
        NAOS_CHECK_NULL(clopts_program_add_help_option(program));
        NAOS_CHECK_NULL(clopts_program_add_version_option(program));
        NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
        EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
        NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
        NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);

        NAOS_CHECK_PTR_EQ(test_decl_finish(program), NULL);
        NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- the first error is kept --------------------------------------------- */

/* A later failing call does not replace the first error, whatever its error. */
NAOS_TEST(first_error_is_kept)
{
    clopts_program_t *program = new_program();
    clopts_option_t *force = clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_command_t *build = clopts_program_add_command(program, "build");

    NAOS_REQUIRE_NOT_NULL(force);
    NAOS_REQUIRE_NOT_NULL(build);
    NAOS_CHECK_NULL(clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "force", '\0'));
    EXPECT_ERROR(program, CLOPTS_ERROR_DUPLICATE);

    /* calls that would fail with other errors */
    NAOS_CHECK_NULL(clopts_program_add_option(program, CLOPTS_OPTION_FLAG, NULL, '\0'));
    NAOS_CHECK_NULL(clopts_program_add_option(program, (clopts_option_kind_t)9, "x", '\0'));
    NAOS_CHECK_NULL(clopts_program_add_positional(program, "FILE"));
    clopts_option_set_default(force, "x");
    NAOS_CHECK_NULL(clopts_command_add_positional(build, ""));
    EXPECT_ERROR(program, CLOPTS_ERROR_DUPLICATE);

    /* calls that would succeed */
    NAOS_CHECK_NULL(clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "quiet", 'q'));
    EXPECT_ERROR(program, CLOPTS_ERROR_DUPLICATE);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 1);
    clopts_program_destroy(program);
}

/*
 * An error reported through a command, an option or a positional is the
 * error of the program: the program and its other commands stop as well.
 */
NAOS_TEST(error_of_any_handle_is_the_program_error)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *output;
    clopts_positional_t *src;

    /* from a command add */
    program = new_program();
    build = clopts_program_add_command(program, "build");
    copy = clopts_program_add_command(program, "copy");
    NAOS_REQUIRE_NOT_NULL(clopts_command_add_option(build, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_CHECK_NULL(clopts_command_add_option(build, CLOPTS_OPTION_COUNT, NULL, 'f'));
    EXPECT_ERROR(program, CLOPTS_ERROR_DUPLICATE);
    NAOS_CHECK_NULL(clopts_command_add_option(copy, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_CHECK_NULL(clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(copy)), 0);
    clopts_program_destroy(program);

    /* from an option setter */
    program = new_program();
    output = clopts_program_add_option(program, CLOPTS_OPTION_VALUE, "output", 'o');
    build = clopts_program_add_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(build);
    clopts_option_set_count_max(output, 3);
    EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
    clopts_command_set_summary(build, "Builds.");
    NAOS_CHECK_NULL(clopts_command_get_summary(build));
    NAOS_CHECK_NULL(clopts_command_add_positional(build, "SRC"));
    clopts_program_destroy(program);

    /* from a positional setter */
    program = new_program();
    build = clopts_program_add_command(program, "build");
    src = clopts_command_add_positional(build, "SRC");
    NAOS_REQUIRE_NOT_NULL(clopts_command_add_positional(build, "DST"));
    clopts_positional_set_variadic(src, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_ORDER);
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(src));
    clopts_program_set_description(program, "A tool.");
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    clopts_program_destroy(program);
}

/*
 * Once the program has an error, every builder function on the program or
 * on a handle added before the error does nothing: it allocates nothing,
 * changes nothing, keeps the error, and the add functions return NULL.
 */
NAOS_TEST(builder_calls_after_an_error_do_nothing)
{
    clopts_program_t *other;
    sample_t s;
    size_t calls;
    long live;

    test_alloc_begin(0);

    /* the error from an add: a second subcommand "build" */
    s = make_sample();
    NAOS_CHECK_NULL(clopts_program_add_command(s.program, "build"));
    EXPECT_ERROR(s.program, CLOPTS_ERROR_DUPLICATE);
    check_sample_unchanged(&s);
    calls = test_alloc.calls;
    live = test_alloc.live;
    call_every_builder(&s);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    EXPECT_ERROR(s.program, CLOPTS_ERROR_DUPLICATE);
    check_sample_unchanged(&s);

    /* another program is not affected */
    other = new_program();
    NAOS_CHECK_NOT_NULL(clopts_program_add_positional(other, "FILE"));
    EXPECT_ERROR(other, CLOPTS_ERROR_NONE);
    clopts_program_destroy(other);
    clopts_program_destroy(s.program);

    /* the error from a setter */
    s = make_sample();
    clopts_option_set_terminal(s.value, true);
    EXPECT_ERROR(s.program, CLOPTS_ERROR_NOT_APPLICABLE);
    check_sample_unchanged(&s);
    calls = test_alloc.calls;
    call_every_builder(&s);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    EXPECT_ERROR(s.program, CLOPTS_ERROR_NOT_APPLICABLE);
    check_sample_unchanged(&s);
    clopts_program_destroy(s.program);

    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- NULL handles -------------------------------------------------------- */

/*
 * Every builder function accepts NULL handles and does nothing: it
 * allocates nothing, the add functions return NULL, and no program gets an
 * error. clopts_program_destroy(NULL) does nothing too.
 */
NAOS_TEST(null_handles_do_nothing)
{
    clopts_program_t *program;
    size_t calls;

    test_alloc_begin(0);
    program = new_program();
    calls = test_alloc.calls;

    clopts_program_destroy(NULL);
    clopts_program_set_description(NULL, "A tool.");
    NAOS_CHECK_NULL(clopts_program_add_option(NULL, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_CHECK_NULL(clopts_program_add_positional(NULL, "FILE"));
    NAOS_CHECK_NULL(clopts_program_add_command(NULL, "build"));
    NAOS_CHECK_NULL(clopts_program_add_help_option(NULL));
    NAOS_CHECK_NULL(clopts_program_add_version_option(NULL));

    clopts_command_set_summary(NULL, "Builds.");
    clopts_command_set_description(NULL, "Builds the tree.");
    NAOS_CHECK_NULL(clopts_command_add_option(NULL, CLOPTS_OPTION_FLAG, "force", 'f'));
    NAOS_CHECK_NULL(clopts_command_add_positional(NULL, "SRC"));

    clopts_option_set_help(NULL, "Help.");
    clopts_option_set_value_name(NULL, "FILE");
    clopts_option_set_env(NULL, "TOOL_OUTPUT");
    clopts_option_set_default(NULL, "a.out");
    clopts_option_set_required(NULL, true);
    clopts_option_set_negatable(NULL, true);
    clopts_option_set_delimiter(NULL, ',');
    clopts_option_set_count_max(NULL, 3);
    clopts_option_set_terminal(NULL, true);

    clopts_positional_set_help(NULL, "Source.");
    clopts_positional_set_required(NULL, true);
    clopts_positional_set_variadic(NULL, true);

    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The NULL that a failed add returns can be passed on: the setters and adds
 * given it do nothing, and the program keeps the error of the failed add.
 */
NAOS_TEST(null_from_a_failed_add_can_be_passed_on)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option;
    clopts_command_t *command;
    clopts_positional_t *positional;

    option = clopts_program_add_option(program, CLOPTS_OPTION_VALUE, "bad name", 'o');
    NAOS_CHECK_NULL(option);
    clopts_option_set_help(option, "Output.");
    clopts_option_set_default(option, "a.out");
    command = clopts_program_add_command(program, "build");
    NAOS_CHECK_NULL(command);
    clopts_command_set_summary(command, "Builds.");
    positional = clopts_command_add_positional(command, "SRC");
    NAOS_CHECK_NULL(positional);
    clopts_positional_set_required(positional, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_PTR_EQ(test_decl_finish(program), NULL);

    /* a NULL program from clopts_program_create() likewise */
    option = clopts_program_add_option(NULL, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_help(option, "Force.");
    NAOS_CHECK_INT_EQ(clopts_program_get_error(NULL), CLOPTS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(test_decl_finish(NULL));
}

/* ---- help and version options ------------------------------------------- */

/*
 * clopts_program_add_help_option() adds the global terminal FLAG -h/--help
 * "Show this help", and clopts_program_add_version_option() the global
 * terminal FLAG --version "Show version information", after the options
 * already there.
 */
NAOS_TEST(help_and_version_option_attributes)
{
    clopts_program_t *program = new_program();
    clopts_option_t *force = clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_t *help = clopts_program_add_help_option(program);
    clopts_option_t *version = clopts_program_add_version_option(program);
    const clopts_scope_t *global = clopts_program_get_scope(program);

    NAOS_REQUIRE_NOT_NULL(force);
    NAOS_REQUIRE_NOT_NULL(help);
    NAOS_REQUIRE_NOT_NULL(version);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(global), 3);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(global, 1), help);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(global, 2), version);

    NAOS_CHECK_INT_EQ(clopts_option_get_kind(help), CLOPTS_OPTION_FLAG);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(help), "help");
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(help), 'h');
    NAOS_CHECK_STR_EQ(clopts_option_get_help(help), "Show this help");
    NAOS_CHECK_TRUE(clopts_option_is_terminal(help));
    NAOS_CHECK_FALSE(clopts_option_is_negatable(help));
    NAOS_CHECK_FALSE(clopts_option_is_required(help));
    NAOS_CHECK_NULL(clopts_option_get_env(help));

    NAOS_CHECK_INT_EQ(clopts_option_get_kind(version), CLOPTS_OPTION_FLAG);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(version), "version");
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(version), '\0');
    NAOS_CHECK_STR_EQ(clopts_option_get_help(version), "Show version information");
    NAOS_CHECK_TRUE(clopts_option_is_terminal(version));
    NAOS_CHECK_FALSE(clopts_option_is_negatable(version));

    /* they are ordinary options: their setters change them */
    clopts_option_set_help(help, "Print the help");
    clopts_option_set_terminal(version, false);
    NAOS_CHECK_STR_EQ(clopts_option_get_help(help), "Print the help");
    NAOS_CHECK_FALSE(clopts_option_is_terminal(version));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/* The help option is global: given after a subcommand it ends the parse there too. */
NAOS_TEST(help_option_terminates_the_parse)
{
    char arg0[] = "tool";
    char arg1[] = "build";
    char arg2[] = "-h";
    char arg3[] = "--version";
    char *argv[] = { arg0, arg1, arg2, arg3, NULL };
    clopts_program_t *program = new_program();
    clopts_option_t *help = clopts_program_add_help_option(program);
    clopts_option_t *version = clopts_program_add_version_option(program);
    clopts_command_t *build = clopts_program_add_command(program, "build");
    clopts_result_t *result;

    NAOS_REQUIRE_PTR_EQ(test_decl_finish(program), program);
    NAOS_REQUIRE_NOT_NULL(help);
    NAOS_REQUIRE_NOT_NULL(version);
    result = clopts_parse(program, 4, argv);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), help);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), build);
    clopts_result_destroy(result);

    result = clopts_parse(program, 2, argv + 2);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), version);
    clopts_result_destroy(result);
    clopts_program_destroy(program);
}

/*
 * The help and version options are added as clopts_program_add_option()
 * adds an option: a global option already named help, -h or version gives
 * DUPLICATE, the add returns NULL and nothing changes. The names are only
 * checked among the global options.
 */
NAOS_TEST(help_and_version_option_names_are_checked)
{
    static const struct
    {
        const char *long_name;
        char short_name;
        int version; /* 0: add_help_option(), 1: add_version_option() */
    } taken[] = {
        { "help", '\0', 0 },
        { NULL, 'h', 0 },
        { "version", 'V', 1 },
    };
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(taken); i++)
    {
        NAOS_NOTE("case %u", (unsigned)i);
        program = new_program();
        NAOS_REQUIRE_NOT_NULL(clopts_program_add_option(program, CLOPTS_OPTION_COUNT, taken[i].long_name,
                                                        taken[i].short_name));
        option = (taken[i].version != 0) ? clopts_program_add_version_option(program)
                                         : clopts_program_add_help_option(program);
        NAOS_CHECK_NULL(option);
        EXPECT_ERROR(program, CLOPTS_ERROR_DUPLICATE);
        NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 1);
        clopts_program_destroy(program);
        NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    }

    /* -V, --no-help and the same names in a subcommand are fine */
    program = new_program();
    build = clopts_program_add_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "no-help", 'V'));
    NAOS_REQUIRE_NOT_NULL(clopts_command_add_option(build, CLOPTS_OPTION_VALUE, "help", 'h'));
    NAOS_REQUIRE_NOT_NULL(clopts_command_add_option(build, CLOPTS_OPTION_VALUE, "version", '\0'));
    NAOS_CHECK_NOT_NULL(clopts_program_add_help_option(program));
    NAOS_CHECK_NOT_NULL(clopts_program_add_version_option(program));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);

    /* each one only once */
    NAOS_CHECK_NULL(clopts_program_add_help_option(program));
    EXPECT_ERROR(program, CLOPTS_ERROR_DUPLICATE);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_SUITE(decl_error,
    NAOS_ADD_TEST(get_error_of_null_program_is_out_of_memory),
    NAOS_ADD_TEST(new_program_has_no_error),
    NAOS_ADD_TEST(program_create_with_invalid_name_keeps_the_error),
    NAOS_ADD_TEST(first_error_is_kept),
    NAOS_ADD_TEST(error_of_any_handle_is_the_program_error),
    NAOS_ADD_TEST(builder_calls_after_an_error_do_nothing),
    NAOS_ADD_TEST(null_handles_do_nothing),
    NAOS_ADD_TEST(null_from_a_failed_add_can_be_passed_on),
    NAOS_ADD_TEST(help_and_version_option_attributes),
    NAOS_ADD_TEST(help_option_terminates_the_parse),
    NAOS_ADD_TEST(help_and_version_option_names_are_checked)
)
