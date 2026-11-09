/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl: the immediate checks of the command, attach and program
 * functions, the lookups shared with the scanner, and how a parse resolves
 * the names those checks allow. The checks of an option or a positional
 * alone are in the decl_option and decl_positional suites.
 *
 * Every builder function returns its clopts_decl_error_t right away, and a
 * failing call changes nothing. Each rule therefore has a test that the
 * offending call returns the expected error and leaves everything as it was
 * (the out pointer NULL, the vecs of the command the same length, the
 * attribute unchanged, an option, positional or command still owned by the
 * caller, who releases it), and a test of the allowed
 * counterparts. Checks against the siblings of an option or positional are
 * made by the attach functions and look at that command only; checks between
 * commands (command names, the layout of the root) are made by
 * clopts_program_create(). The order of the checks within one call is that of
 * parser_program.c and command.c.
 *
 * Names are not checked between the root and a command: there are no
 * reserved names, and a name of a command shadows the same name of the root.
 * The shadowing tests parse such declarations and read which option got the
 * value.
 *
 * Calls whose error is checked go to the API directly; the helpers of
 * test_decl.h build the surrounding declarations. The model and the lookups
 * are internal and are read or called directly.
 */

#include "parser/parser_internal.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Checks that call returns CLOPTS_DECL_NONE. */
#define EXPECT_OK(call) NAOS_CHECK_INT_EQ((call), CLOPTS_DECL_NONE)

/* Number of elements of the array a. */
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Objects whose address an out pointer holds before a call, so that a test
 * can see the call set it to NULL. They are never read or written.
 */
static clopts_program_t sentinel_program;
static clopts_command_t sentinel_command;

/* Returns option index of scope. */
static const clopts_option_t *option_at(const clopts_command_t *scope, size_t index)
{
    return clopts_command_option_at(scope, index);
}

/* Returns positional index of scope. */
static const clopts_positional_t *positional_at(const clopts_command_t *scope, size_t index)
{
    return clopts_command_positional_at(scope, index);
}

/* A new unattached command named name; the test stops when it cannot be made. */
static clopts_command_t *new_command(const char *name)
{
    clopts_command_t *command = NULL;

    EXPECT_OK(clopts_command_create(name, &command));
    NAOS_REQUIRE_NOT_NULL(command);
    return command;
}

/* A new root named "tool" (test_decl_root()); the test stops when it cannot be made. */
static clopts_command_t *new_root(void)
{
    clopts_command_t *root = test_decl_root("tool");

    NAOS_REQUIRE_NOT_NULL(root);
    return root;
}

/* A new command remembered for root (test_decl_command()); the test stops when it cannot be made. */
static clopts_command_t *command(clopts_command_t *root, const char *name)
{
    clopts_command_t *result = test_decl_command(root, name);

    NAOS_REQUIRE_NOT_NULL(result);
    return result;
}

/* The program of root and its remembered commands (test_decl_finish()); the test stops when it fails. */
static clopts_program_t *finish(clopts_command_t *root)
{
    clopts_program_t *program = test_decl_finish(root);

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/* A new unattached option; the test stops when it cannot be made. */
static clopts_option_t *make(clopts_option_kind_t kind, const char *long_name, char short_name)
{
    clopts_option_t *option = test_decl_option(kind, long_name, short_name);

    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/* A new unattached negatable FLAG; the test stops when it cannot be made. */
static clopts_option_t *make_negatable(const char *long_name, char short_name)
{
    clopts_option_t *option = make(CLOPTS_OPTION_FLAG, long_name, short_name);

    EXPECT_OK(clopts_option_set_negatable(option, true));
    return option;
}

/* A new unattached positional; the test stops when it cannot be made. */
static clopts_positional_t *make_positional(const char *name)
{
    clopts_positional_t *positional = test_decl_positional(name);

    NAOS_REQUIRE_NOT_NULL(positional);
    return positional;
}

/* Attaches option to scope; the test stops when the attach fails. */
static clopts_option_t *attach(clopts_command_t *scope, clopts_option_t *option)
{
    NAOS_REQUIRE_NOT_NULL(option);
    option = test_decl_attach(scope, option);
    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/* Attaches positional to scope; the test stops when the attach fails. */
static clopts_positional_t *attach_positional(clopts_command_t *scope, clopts_positional_t *positional)
{
    NAOS_REQUIRE_NOT_NULL(positional);
    positional = test_decl_attach_positional(scope, positional);
    NAOS_REQUIRE_NOT_NULL(positional);
    return positional;
}

/* attach(scope, make(kind, long_name, short_name)): an option without attributes. */
static clopts_option_t *add(clopts_command_t *scope, clopts_option_kind_t kind, const char *long_name,
                            char short_name)
{
    return attach(scope, make(kind, long_name, short_name));
}

/* attach_positional(scope, make_positional(name)): a positional without attributes. */
static clopts_positional_t *add_positional(clopts_command_t *scope, const char *name)
{
    return attach_positional(scope, make_positional(name));
}

/*
 * Attaches option to scope expecting error: checks that the options of scope
 * are unchanged, then releases option (the caller still owns it).
 */
static void expect_attach_error(clopts_command_t *scope, clopts_option_t *option, clopts_decl_error_t error)
{
    size_t len;

    NAOS_REQUIRE_NOT_NULL(scope);
    NAOS_REQUIRE_NOT_NULL(option);
    len = scope->options.len;
    NAOS_CHECK_INT_EQ(clopts_command_add_option(scope, option), error);
    NAOS_CHECK_UINT_EQ(scope->options.len, len);
    clopts_option_destroy(option);
}

/* As expect_attach_error(), for a positional. */
static void expect_attach_positional_error(clopts_command_t *scope, clopts_positional_t *positional,
                                           clopts_decl_error_t error)
{
    size_t len;

    NAOS_REQUIRE_NOT_NULL(scope);
    NAOS_REQUIRE_NOT_NULL(positional);
    len = scope->positionals.len;
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(scope, positional), error);
    NAOS_CHECK_UINT_EQ(scope->positionals.len, len);
    clopts_positional_destroy(positional);
}

/* Maximum number of commands passed to expect_program_error(). */
#define MAX_PROGRAM_COMMANDS 8

/*
 * Calls clopts_program_create(root, commands, count) expecting error and checks
 * that the out pointer is set to NULL and that no handle changed: the
 * numbers of options and positionals of root and of every command (NULL
 * entries skipped) are the same as before.
 */
static void expect_program_error(clopts_command_t *root, clopts_command_t *const *commands, size_t count,
                                 clopts_decl_error_t error)
{
    clopts_command_t *all[MAX_PROGRAM_COMMANDS + 1];
    size_t options[MAX_PROGRAM_COMMANDS + 1];
    size_t positionals[MAX_PROGRAM_COMMANDS + 1];
    clopts_program_t *out = &sentinel_program;
    size_t n = 0;
    size_t i;

    NAOS_REQUIRE_TRUE(count <= MAX_PROGRAM_COMMANDS);
    all[n++] = root;
    for (i = 0; (commands != NULL) && (i < count); i++)
    {
        all[n++] = commands[i];
    }
    for (i = 0; i < n; i++)
    {
        if (all[i] != NULL)
        {
            options[i] = all[i]->options.len;
            positionals[i] = all[i]->positionals.len;
        }
    }

    NAOS_CHECK_INT_EQ(clopts_program_create(root, commands, count, &out), error);
    NAOS_CHECK_NULL(out);
    for (i = 0; i < n; i++)
    {
        if (all[i] != NULL)
        {
            NAOS_NOTE("handle %u", (unsigned)i);
            NAOS_CHECK_UINT_EQ(all[i]->options.len, options[i]);
            NAOS_CHECK_UINT_EQ(all[i]->positionals.len, positionals[i]);
        }
    }
}

/* Calls clopts_program_create(root, commands, count) expecting success; the test stops when it fails. */
static clopts_program_t *program_new(clopts_command_t *root, clopts_command_t *const *commands, size_t count)
{
    clopts_program_t *program = NULL;

    EXPECT_OK(clopts_program_create(root, commands, count, &program));
    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/* Maximum number of tokens parse_tokens() passes after argv[0]. */
#define MAX_TOKENS 7

/*
 * Parses tokens (NULL-terminated, at most MAX_TOKENS) after argv[0] "tool"
 * against program. The argv elements are static copies: a result must be
 * read and released before the next call.
 */
static clopts_result_t *parse_tokens(const clopts_program_t *program, const char *const *tokens)
{
    static char buffers[MAX_TOKENS + 1][32];
    char *argv[MAX_TOKENS + 2];
    int argc = 0;

    strcpy(buffers[0], "tool");
    argv[argc++] = buffers[0];
    for (; tokens[argc - 1] != NULL; argc++)
    {
        NAOS_REQUIRE_TRUE(argc <= MAX_TOKENS);
        NAOS_REQUIRE_TRUE(strlen(tokens[argc - 1]) < sizeof(buffers[0]));
        strcpy(buffers[argc], tokens[argc - 1]);
        argv[argc] = buffers[argc];
    }
    argv[argc] = NULL;
    return clopts_parse(program, argc, argv);
}

/* ---- name rules -------------------------------------------------------- */

NAOS_TEST(valid_command_name_rules)
{
    NAOS_CHECK_TRUE(clopts_parser_valid_command_name("build"));
    NAOS_CHECK_TRUE(clopts_parser_valid_command_name("a-b"));
    NAOS_CHECK_TRUE(clopts_parser_valid_command_name("help"));
    NAOS_CHECK_FALSE(clopts_parser_valid_command_name(NULL));
    NAOS_CHECK_FALSE(clopts_parser_valid_command_name(""));
    NAOS_CHECK_FALSE(clopts_parser_valid_command_name("-b"));
    NAOS_CHECK_FALSE(clopts_parser_valid_command_name("a b"));
    NAOS_CHECK_FALSE(clopts_parser_valid_command_name("a\tb"));
    NAOS_CHECK_FALSE(clopts_parser_valid_command_name("ab\n"));
}

NAOS_TEST(valid_long_name_rules)
{
    NAOS_CHECK_TRUE(clopts_parser_valid_long_name("output"));
    NAOS_CHECK_TRUE(clopts_parser_valid_long_name("no-color"));
    NAOS_CHECK_FALSE(clopts_parser_valid_long_name(NULL));
    NAOS_CHECK_FALSE(clopts_parser_valid_long_name(""));
    NAOS_CHECK_FALSE(clopts_parser_valid_long_name("-o"));
    NAOS_CHECK_FALSE(clopts_parser_valid_long_name("a=b"));
    NAOS_CHECK_FALSE(clopts_parser_valid_long_name("a b"));
    NAOS_CHECK_FALSE(clopts_parser_valid_long_name("a\rb"));
}

NAOS_TEST(valid_short_name_rules)
{
    NAOS_CHECK_TRUE(clopts_parser_valid_short_name('o'));
    NAOS_CHECK_TRUE(clopts_parser_valid_short_name('4'));
    NAOS_CHECK_FALSE(clopts_parser_valid_short_name('\0'));
    NAOS_CHECK_FALSE(clopts_parser_valid_short_name('-'));
    NAOS_CHECK_FALSE(clopts_parser_valid_short_name('='));
    NAOS_CHECK_FALSE(clopts_parser_valid_short_name(' '));
    NAOS_CHECK_FALSE(clopts_parser_valid_short_name('\t'));
}

/* ---- clopts_command_create -------------------------------------------------- */

NAOS_TEST(command_name_invalid)
{
    static const char *const names[] = { NULL, "", "-b", "b c", "b\nc", "my\ttool" };
    clopts_command_t *out;
    size_t i;

    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("name=\"%s\"", (names[i] != NULL) ? names[i] : "(null)");
        out = &sentinel_command;
        NAOS_CHECK_INT_EQ(clopts_command_create(names[i], &out), CLOPTS_DECL_INVALID_NAME);
        NAOS_CHECK_NULL(out);
    }
}

/* "help" is an ordinary name: as the root (the program name) and as a subcommand. */
NAOS_TEST(command_named_help_is_accepted)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *commands[2];

    root = new_command("help");
    program = program_new(root, NULL, 0);
    NAOS_CHECK_STR_EQ(clopts_command_name(clopts_program_root(program)), "help");
    clopts_program_destroy(program);

    root = new_command("tool");
    commands[0] = new_command("build");
    commands[1] = new_command("help");
    program = program_new(root, commands, 2);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(program, "help"), commands[1]);
    clopts_program_destroy(program);
}

/* ---- clopts_program_create: arguments ------------------------------------------ */

/* ---- clopts_program_create: DUPLICATE_COMMAND ---------------------------------- */

NAOS_TEST(program_new_duplicate_command)
{
    clopts_command_t *root = new_command("tool");
    clopts_command_t *commands[3];
    clopts_command_t *pair[2];
    clopts_program_t *program;

    commands[0] = new_command("build");
    commands[1] = new_command("copy");
    commands[2] = new_command("build");
    expect_program_error(root, commands, 3, CLOPTS_DECL_DUPLICATE_COMMAND);
    pair[0] = commands[2];
    pair[1] = commands[0];
    expect_program_error(root, pair, 2, CLOPTS_DECL_DUPLICATE_COMMAND);
    clopts_command_destroy(commands[2]);
    program = program_new(root, commands, 2);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(program, "build"), commands[0]);
    clopts_program_destroy(program);
}

NAOS_TEST(command_names_are_case_sensitive)
{
    static const char *const names[] = { "build", "Build", "buil", "Help", "helper", "tool" };
    clopts_command_t *root = new_command("tool");
    clopts_command_t *commands[COUNT_OF(names)];
    clopts_program_t *program;
    size_t i;

    /* the name of the root is not a command name: a command may share it */
    for (i = 0; i < COUNT_OF(names); i++)
    {
        commands[i] = new_command(names[i]);
    }
    program = program_new(root, commands, COUNT_OF(names));
    NAOS_CHECK_UINT_EQ(clopts_program_command_count(program), COUNT_OF(names));
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_program_command_at(program, i), commands[i]);
    }
    clopts_program_destroy(program);
}

/* ---- clopts_program_create: MIXED_LAYOUT --------------------------------------- */

NAOS_TEST(program_new_root_positional_with_commands_is_mixed)
{
    clopts_command_t *root = new_command("tool");
    clopts_command_t *build = new_command("build");
    clopts_program_t *program;

    add_positional(root, "file");
    add_positional(build, "file");
    expect_program_error(root, &build, 1, CLOPTS_DECL_MIXED_LAYOUT);

    /* without subcommands the root takes positionals */
    program = program_new(root, NULL, 0);
    NAOS_CHECK_UINT_EQ(clopts_command_positional_count(clopts_program_root(program)), 1);
    clopts_program_destroy(program);

    /* and subcommands may have positionals, with root options */
    root = new_command("tool");
    add(root, CLOPTS_OPTION_FLAG, "verbose", 'v');
    program = program_new(root, &build, 1);
    clopts_program_destroy(program);
}

/* ---- clopts_program_create: order and failure ---------------------------------- */

NAOS_TEST(program_new_checks_follow_order)
{
    clopts_command_t *root = new_command("tool");
    clopts_command_t *build = new_command("build");
    clopts_command_t *build2 = new_command("build");
    clopts_command_t *commands[4];

    add_positional(root, "file");

    /* DUPLICATE_COMMAND before MIXED_LAYOUT */
    commands[0] = build;
    commands[1] = build2;
    expect_program_error(root, commands, 2, CLOPTS_DECL_DUPLICATE_COMMAND);

    /* MIXED_LAYOUT last */
    expect_program_error(root, commands, 1, CLOPTS_DECL_MIXED_LAYOUT);

    clopts_command_destroy(root);
    clopts_command_destroy(build);
    clopts_command_destroy(build2);
}

/*
 * A failed clopts_program_create() allocates and releases nothing, and the
 * caller still owns and may change every handle; also when an allocation of
 * the call fails.
 */
NAOS_TEST(program_new_failure_changes_nothing)
{
    clopts_command_t *root;
    clopts_command_t *commands[2];
    clopts_program_t *program;
    size_t calls;
    long live;

    test_alloc_begin(0);
    root = new_command("tool");
    commands[0] = new_command("build");
    commands[1] = new_command("build");
    add(root, CLOPTS_OPTION_FLAG, "verbose", 'v');
    add_positional(root, "file");

    live = test_alloc.live;
    expect_program_error(root, commands, 2, CLOPTS_DECL_DUPLICATE_COMMAND);
    expect_program_error(root, commands, 1, CLOPTS_DECL_MIXED_LAYOUT);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);

    /* still the caller's: setters and attaches work, the release works */
    EXPECT_OK(clopts_command_set_summary(commands[0], "Build things."));
    EXPECT_OK(clopts_command_set_description(root, "A tool."));
    add(commands[0], CLOPTS_OPTION_FLAG, "verbose", 'v');
    live = test_alloc.live;
    clopts_command_destroy(commands[1]);
    NAOS_CHECK_INT_LT(test_alloc.live, live);

    /* the rules are checked before any allocation */
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_program_error(root, commands, 1, CLOPTS_DECL_MIXED_LAYOUT);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    clopts_command_destroy(root);

    /* out of memory at each allocation of the call */
    root = new_command("tool");
    live = test_alloc.live;
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_program_error(root, commands, 1, CLOPTS_DECL_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    test_alloc_set_fail_at(calls + 2);
    expect_program_error(root, commands, 1, CLOPTS_DECL_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    test_alloc_set_fail_at(0);

    program = program_new(root, commands, 1);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);
    NAOS_CHECK_PTR_EQ(clopts_program_command_at(program, 0), commands[0]);
    clopts_program_destroy(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- names shared by the root and a command ----------------------------------- */

/* The names once reserved for the built-in help and version are ordinary names. */
NAOS_TEST(help_and_version_names_are_accepted)
{
    clopts_command_t *root = new_root();
    clopts_command_t *build = command(root, "build");
    clopts_command_t *help;
    clopts_program_t *program;

    add(root, CLOPTS_OPTION_FLAG, "help", 'h');
    add(root, CLOPTS_OPTION_FLAG, "version", 'V');
    add(build, CLOPTS_OPTION_VALUE, "help", 'h');
    add(build, CLOPTS_OPTION_VALUE, "version", '\0');
    help = command(root, "help");
    add(help, CLOPTS_OPTION_COUNT, NULL, 'h');
    add(help, CLOPTS_OPTION_LIST, "version", 'v');
    program = finish(root);
    NAOS_CHECK_UINT_EQ(root->options.len, 2);
    NAOS_CHECK_UINT_EQ(build->options.len, 2);
    NAOS_CHECK_UINT_EQ(help->options.len, 2);
    clopts_program_destroy(program);
}

/* The same long or short name in the root and in commands, attached in any order. */
NAOS_TEST(option_same_name_in_root_and_command_is_accepted)
{
    clopts_command_t *root = new_root();
    clopts_command_t *build = command(root, "build");
    clopts_command_t *copy = command(root, "copy");
    clopts_program_t *program;

    /* root first */
    add(root, CLOPTS_OPTION_FLAG, "verbose", 'v');
    add(build, CLOPTS_OPTION_FLAG, "verbose", 'v');
    add(build, CLOPTS_OPTION_VALUE, "value", 'x');

    /* command first */
    add(copy, CLOPTS_OPTION_FLAG, "release", 'r');
    add(root, CLOPTS_OPTION_VALUE, "root", 'r');
    add(root, CLOPTS_OPTION_FLAG, "release", '\0');

    /* the same name in two commands */
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    add(build, CLOPTS_OPTION_FLAG, "Output", 'O');
    add(copy, CLOPTS_OPTION_FLAG, NULL, 'O');
    add(root, CLOPTS_OPTION_COUNT, "output", '\0');

    program = finish(root);
    NAOS_CHECK_UINT_EQ(root->options.len, 4);
    NAOS_CHECK_UINT_EQ(build->options.len, 4);
    NAOS_CHECK_UINT_EQ(copy->options.len, 3);
    clopts_program_destroy(program);
}

/* A negatable FLAG x in one scope and an option named no-x in another one. */
NAOS_TEST(negation_across_root_and_command_is_accepted)
{
    clopts_command_t *root = new_root();
    clopts_command_t *build = command(root, "build");
    clopts_command_t *copy = command(root, "copy");
    clopts_program_t *program;

    /* a negatable global and no-x in a command, in both orders */
    attach(root, make_negatable("color", '\0'));
    add(build, CLOPTS_OPTION_FLAG, "no-color", 'n');
    add(copy, CLOPTS_OPTION_VALUE, "no-cache", '\0');
    attach(root, make_negatable("cache", 'c'));

    /* a global no-x and a negatable command flag, in both orders */
    add(root, CLOPTS_OPTION_FLAG, "no-quiet", '\0');
    attach(build, make_negatable("quiet", 'q'));
    attach(copy, make_negatable("dry", '\0'));
    add(root, CLOPTS_OPTION_LIST, "no-dry", '\0');

    /* a negatable flag of one command and no-x of another */
    attach(build, make_negatable("sign", '\0'));
    add(copy, CLOPTS_OPTION_FLAG, "no-sign", '\0');

    program = finish(root);
    NAOS_CHECK_UINT_EQ(root->options.len, 4);
    NAOS_CHECK_UINT_EQ(build->options.len, 3);
    NAOS_CHECK_UINT_EQ(copy->options.len, 3);
    clopts_program_destroy(program);
}

/* ---- clopts_command_add_option ------------------------------------------- */

NAOS_TEST(option_duplicate_in_same_command)
{
    clopts_command_t *root = new_command("tool");
    clopts_command_t *build = new_command("build");
    clopts_option_t *force;

    force = add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    expect_attach_error(root, make(CLOPTS_OPTION_VALUE, "force", 'o'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(root, make(CLOPTS_OPTION_COUNT, NULL, 'f'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "force", 'f'), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), force);

    add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    expect_attach_error(build, make(CLOPTS_OPTION_LIST, "release", '\0'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(build, make(CLOPTS_OPTION_VALUE, "root", 'r'), CLOPTS_DECL_DUPLICATE_OPTION);

    /* names are case-sensitive, and long and short names are separate namespaces */
    add(build, CLOPTS_OPTION_FLAG, "Release", 'R');
    add(build, CLOPTS_OPTION_FLAG, "r", '\0');
    add(build, CLOPTS_OPTION_FLAG, "f", 'F');
    NAOS_CHECK_UINT_EQ(build->options.len, 4);
    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    clopts_command_destroy(build);
    clopts_command_destroy(root);
}

/* no-x attached after a negatable FLAG x in the same command */
NAOS_TEST(option_negation_conflict_on_add)
{
    clopts_command_t *root = new_command("tool");
    clopts_command_t *build = new_command("build");

    /* any kind, in the root and in a command */
    attach(root, make_negatable("color", '\0'));
    expect_attach_error(root, make(CLOPTS_OPTION_VALUE, "no-color", '\0'), CLOPTS_DECL_NEGATION_CONFLICT);
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "no-color", 'n'), CLOPTS_DECL_NEGATION_CONFLICT);
    attach(build, make_negatable("cache", 'c'));
    expect_attach_error(build, make(CLOPTS_OPTION_LIST, "no-cache", '\0'), CLOPTS_DECL_NEGATION_CONFLICT);
    expect_attach_error(build, make(CLOPTS_OPTION_COUNT, "no-cache", 'x'), CLOPTS_DECL_NEGATION_CONFLICT);

    /* a negatable FLAG that is not the first option of the command */
    add(build, CLOPTS_OPTION_FLAG, "quiet", 'q');
    attach(build, make_negatable("sign", '\0'));
    expect_attach_error(build, make(CLOPTS_OPTION_FLAG, "no-sign", '\0'), CLOPTS_DECL_NEGATION_CONFLICT);

    /* only the command itself is checked */
    add(build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    NAOS_CHECK_UINT_EQ(build->options.len, 4);
    clopts_command_destroy(build);
    clopts_command_destroy(root);
}

/* a negatable FLAG x attached after no-x in the same command */
NAOS_TEST(negatable_with_existing_no_option)
{
    clopts_command_t *root = new_command("tool");
    clopts_command_t *build = new_command("build");
    clopts_option_t *option;

    add(root, CLOPTS_OPTION_VALUE, "no-color", '\0');
    expect_attach_error(root, make_negatable("color", '\0'), CLOPTS_DECL_NEGATION_CONFLICT);

    add(build, CLOPTS_OPTION_FLAG, "quiet", 'q');
    add(build, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    expect_attach_error(build, make_negatable("cache", 'c'), CLOPTS_DECL_NEGATION_CONFLICT);

    /* without negatable the same option is allowed */
    option = make_negatable("cache", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, false));
    attach(build, option);

    /* only the command itself is checked */
    attach(root, make_negatable("cache", '\0'));
    NAOS_CHECK_UINT_EQ(root->options.len, 2);
    NAOS_CHECK_UINT_EQ(build->options.len, 3);
    clopts_command_destroy(build);
    clopts_command_destroy(root);
}

NAOS_TEST(option_no_prefix_without_negatable_flag_is_allowed)
{
    clopts_command_t *root = new_command("tool");

    add(root, CLOPTS_OPTION_FLAG, "color", '\0');
    add(root, CLOPTS_OPTION_FLAG, "no-color", '\0');
    add(root, CLOPTS_OPTION_VALUE, "no-output", '\0');
    add(root, CLOPTS_OPTION_VALUE, "output", 'o');
    add(root, CLOPTS_OPTION_FLAG, "no-", '\0');
    attach(root, make_negatable("Cache", '\0'));
    add(root, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    add(root, CLOPTS_OPTION_FLAG, "No-Cache", '\0');
    NAOS_CHECK_UINT_EQ(root->options.len, 8);
    clopts_command_destroy(root);
}

NAOS_TEST(negatable_flag_without_long_name_is_allowed)
{
    clopts_command_t *root = new_command("tool");
    clopts_option_t *option;

    add(root, CLOPTS_OPTION_FLAG, "no-", '\0');
    option = attach(root, make_negatable(NULL, 'c'));
    NAOS_CHECK_TRUE(option->negatable);
    clopts_command_destroy(root);
}

NAOS_TEST(option_attach_checks_follow_order)
{
    clopts_command_t *root = new_command("tool");

    add(root, CLOPTS_OPTION_FLAG, "hidden", '\0');

    /* DUPLICATE_OPTION before NEGATION_CONFLICT, in both directions */
    attach(root, make_negatable("color", '\0'));
    add(root, CLOPTS_OPTION_FLAG, "quiet", 'q');
    add(root, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "no-color", 'q'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(root, make_negatable("cache", 'q'), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_UINT_EQ(root->options.len, 4);
    clopts_command_destroy(root);
}

/* ---- clopts_command_add_positional ---------------------------------------- */

NAOS_TEST(positional_after_variadic)
{
    clopts_command_t *build = new_command("build");
    clopts_positional_t *files;
    clopts_positional_t *extra;

    files = make_positional("files");
    EXPECT_OK(clopts_positional_set_variadic(files, true));
    attach_positional(build, files);

    expect_attach_positional_error(build, make_positional("extra"), CLOPTS_DECL_VARIADIC_POSITION);
    extra = make_positional("extra");
    EXPECT_OK(clopts_positional_set_variadic(extra, true));
    expect_attach_positional_error(build, extra, CLOPTS_DECL_VARIADIC_POSITION);

    /* VARIADIC_POSITION before POSITIONAL_ORDER (an optional variadic, then a required one) */
    extra = make_positional("extra");
    EXPECT_OK(clopts_positional_set_required(extra, true));
    expect_attach_positional_error(build, extra, CLOPTS_DECL_VARIADIC_POSITION);
    NAOS_CHECK_PTR_EQ(positional_at(build, 0), files);
    clopts_command_destroy(build);
}

NAOS_TEST(required_positional_after_optional)
{
    clopts_command_t *copy = new_command("copy");
    clopts_positional_t *dst;

    add_positional(copy, "src");
    dst = make_positional("dst");
    EXPECT_OK(clopts_positional_set_required(dst, true));
    expect_attach_positional_error(copy, dst, CLOPTS_DECL_POSITIONAL_ORDER);

    /* a required variadic one too */
    dst = make_positional("dst");
    EXPECT_OK(clopts_positional_set_required(dst, true));
    EXPECT_OK(clopts_positional_set_variadic(dst, true));
    expect_attach_positional_error(copy, dst, CLOPTS_DECL_POSITIONAL_ORDER);

    /* an optional one is allowed */
    add_positional(copy, "dst");
    NAOS_CHECK_UINT_EQ(copy->positionals.len, 2);
    clopts_command_destroy(copy);
}

NAOS_TEST(positional_order_allowed)
{
    clopts_command_t *root = new_command("tool");
    clopts_positional_t *a;
    clopts_positional_t *b;
    clopts_positional_t *c;
    clopts_positional_t *d;
    clopts_program_t *program;

    a = make_positional("a");
    EXPECT_OK(clopts_positional_set_required(a, true));
    attach_positional(root, a);
    b = make_positional("b");
    EXPECT_OK(clopts_positional_set_required(b, true));
    EXPECT_OK(clopts_positional_set_variadic(b, true));
    EXPECT_OK(clopts_positional_set_variadic(b, false));
    attach_positional(root, b);
    c = make_positional("c");
    EXPECT_OK(clopts_positional_set_required(c, true));
    EXPECT_OK(clopts_positional_set_required(c, false));
    attach_positional(root, c);
    d = make_positional("d");
    EXPECT_OK(clopts_positional_set_variadic(d, true));
    attach_positional(root, d);

    NAOS_REQUIRE_UINT_EQ(root->positionals.len, 4);
    NAOS_CHECK_TRUE(a->required);
    NAOS_CHECK_TRUE(b->required);
    NAOS_CHECK_FALSE(b->variadic);
    NAOS_CHECK_FALSE(c->required);
    NAOS_CHECK_TRUE(d->variadic);
    NAOS_CHECK_PTR_EQ(positional_at(root, 3), d);
    program = program_new(root, NULL, 0);
    clopts_program_destroy(program);

    /* a single required variadic one */
    root = new_command("tool");
    a = make_positional("files");
    EXPECT_OK(clopts_positional_set_required(a, true));
    EXPECT_OK(clopts_positional_set_variadic(a, true));
    attach_positional(root, a);
    clopts_command_destroy(root);
}

/* ---- negatable flags at attach ------------------------------------------ */

NAOS_TEST(negatable_unset_is_not_checked)
{
    clopts_command_t *root = new_command("tool");
    clopts_option_t *value;
    clopts_option_t *color;

    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_negatable(value, false));
    attach(root, value);
    add(root, CLOPTS_OPTION_FLAG, "no-color", '\0');
    color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(color, false));
    attach(root, color);
    NAOS_CHECK_UINT_EQ(root->options.len, 3);
    clopts_command_destroy(root);
}

/* ---- releases ----------------------------------------------------------- */

NAOS_TEST(releases_accept_null)
{
    clopts_program_destroy(NULL);
    clopts_command_destroy(NULL);
    clopts_option_destroy(NULL);
    clopts_positional_destroy(NULL);
    NAOS_PASS();
}

/* ---- ownership on a failed attach ------------------------------------------ */

/*
 * A failed attach leaves the option or positional with the caller: no block
 * is allocated or released by the call, the caller may attach it elsewhere,
 * or release it, after which nothing is left alive.
 */
NAOS_TEST(failed_attach_leaves_ownership_with_the_caller)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_program_t *program;
    clopts_option_t *option;
    clopts_positional_t *positional;
    long live;

    test_alloc_begin(0);
    root = new_root();
    build = command(root, "build");
    copy = command(root, "copy");
    add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    add(build, CLOPTS_OPTION_FLAG, "force", '\0');

    /* released by the caller */
    option = make(CLOPTS_OPTION_VALUE, "force", '\0');
    EXPECT_OK(clopts_option_set_help(option, "Force."));
    EXPECT_OK(clopts_option_set_value_name(option, "WHEN"));
    EXPECT_OK(clopts_option_set_env(option, "TOOL_FORCE"));
    EXPECT_OK(clopts_option_set_default(option, "always"));
    live = test_alloc.live;
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_STR_EQ(option->help, "Force.");
    EXPECT_OK(clopts_option_set_help(option, "Still mine."));
    clopts_option_destroy(option);
    NAOS_CHECK_INT_LT(test_alloc.live, live);

    /* attached elsewhere after the failure, then released with the program */
    option = make(CLOPTS_OPTION_VALUE, "out", 'o');
    EXPECT_OK(clopts_option_set_help(option, "Output."));
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_DUPLICATE_OPTION);
    EXPECT_OK(clopts_command_add_option(copy, option));
    NAOS_CHECK_PTR_EQ(option_at(copy, 0), option);

    /* the same for positionals */
    positional = make_positional("rest");
    EXPECT_OK(clopts_positional_set_variadic(positional, true));
    attach_positional(build, positional);
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_help(positional, "Source."));
    live = test_alloc.live;
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(build, positional), CLOPTS_DECL_VARIADIC_POSITION);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    EXPECT_OK(clopts_command_add_positional(copy, positional));
    NAOS_CHECK_PTR_EQ(positional_at(copy, 0), positional);

    positional = make_positional("dst");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(copy, positional), CLOPTS_DECL_POSITIONAL_ORDER);
    clopts_positional_destroy(positional);

    program = finish(root);
    clopts_program_destroy(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- valid declarations -------------------------------------------------- */

/*
 * A declaration shaped like the fixture "tool": globals, two commands, a
 * negatable flag, env and default, a list delimiter, a count maximum,
 * required options and positionals with a variadic tail, and a terminal
 * --help in the root and in a command. Every call succeeds, and a program
 * built only from successful calls parses.
 */
NAOS_TEST(tool_like_declaration_is_ok)
{
    static const char *const tokens[] = { "copy", "a", "b", NULL };
    clopts_command_t *root = new_root();
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *option;
    clopts_positional_t *positional;
    clopts_program_t *program;
    clopts_result_t *result;

    EXPECT_OK(clopts_command_set_description(root, "A sample tool."));
    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    EXPECT_OK(clopts_option_set_count_max(option, 3));
    attach(root, option);
    attach(root, make_negatable("color", '\0'));
    option = make(CLOPTS_OPTION_VALUE, "config", 'c');
    EXPECT_OK(clopts_option_set_env(option, "TOOL_CONFIG"));
    attach(root, option);
    option = make(CLOPTS_OPTION_FLAG, "help", 'h');
    EXPECT_OK(clopts_option_set_terminal(option, true));
    attach(root, option);

    build = command(root, "build");
    EXPECT_OK(clopts_command_set_summary(build, "Build things."));
    option = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_env(option, "TOOL_OUTPUT"));
    EXPECT_OK(clopts_option_set_default(option, "a.out"));
    attach(build, option);
    option = make(CLOPTS_OPTION_LIST, "keys", 'k');
    EXPECT_OK(clopts_option_set_delimiter(option, ','));
    EXPECT_OK(clopts_option_set_required(option, true));
    attach(build, option);
    option = make(CLOPTS_OPTION_COUNT, "jobs", 'j');
    EXPECT_OK(clopts_option_set_count_max(option, 8));
    attach(build, option);
    add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    option = make(CLOPTS_OPTION_FLAG, "help", 'h');
    EXPECT_OK(clopts_option_set_terminal(option, true));
    attach(build, option);
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    attach_positional(build, positional);
    positional = make_positional("rest");
    EXPECT_OK(clopts_positional_set_variadic(positional, true));
    attach_positional(build, positional);

    copy = command(root, "copy");
    add(copy, CLOPTS_OPTION_FLAG, "release", 'r');
    add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    attach_positional(copy, positional);
    positional = make_positional("dst");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    attach_positional(copy, positional);

    program = finish(root);
    NAOS_CHECK_UINT_EQ(root->options.len, 4);
    NAOS_CHECK_UINT_EQ(build->options.len, 5);
    NAOS_CHECK_UINT_EQ(build->positionals.len, 2);
    NAOS_CHECK_UINT_EQ(copy->options.len, 2);
    NAOS_CHECK_UINT_EQ(copy->positionals.len, 2);
    NAOS_CHECK_UINT_EQ(clopts_program_command_count(program), 2);

    result = parse_tokens(program, tokens);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), copy);
    clopts_result_free(result);
    clopts_program_destroy(program);
}

/* ---- lookups ------------------------------------------------------------- */

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

static lookup_fixture_t make_lookup_fixture(void)
{
    lookup_fixture_t f;
    clopts_command_t *root = new_root();

    f.verbose = add(root, CLOPTS_OPTION_COUNT, "verbose", 'v');
    f.color = attach(root, make_negatable("color", '\0'));
    f.quiet = add(root, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.no_cache = add(root, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    f.build = command(root, "build");
    f.copy = command(root, "copy");
    f.build_output = add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_cache = attach(f.build, make_negatable("cache", 'c'));
    f.build_release = attach(f.build, make_negatable(NULL, 'r'));
    f.build_no_color = add(f.build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    f.copy_output = add(f.copy, CLOPTS_OPTION_VALUE, "output", 'o');
    f.program = finish(root);
    return f;
}

NAOS_TEST(find_command_is_exact_and_case_sensitive)
{
    lookup_fixture_t f = make_lookup_fixture();

    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(f.program, "build"), f.build);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(f.program, "copy"), f.copy);
    NAOS_CHECK_NULL(clopts_parser_find_command(f.program, "Build"));
    NAOS_CHECK_NULL(clopts_parser_find_command(f.program, "bui"));
    NAOS_CHECK_NULL(clopts_parser_find_command(f.program, "builds"));
    NAOS_CHECK_NULL(clopts_parser_find_command(f.program, ""));
    NAOS_CHECK_NULL(clopts_parser_find_command(f.program, "tool"));
    NAOS_CHECK_NULL(clopts_parser_find_command(f.program, "help"));
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_long_searches_command_then_root)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    ref = clopts_parser_find_long(f.program, f.build, "output", 6);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_output);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 0);
    NAOS_CHECK_FALSE(ref.negated);

    ref = clopts_parser_find_long(f.program, f.copy, "output", 6);
    NAOS_CHECK_PTR_EQ(ref.option, f.copy_output);

    ref = clopts_parser_find_long(f.program, f.build, "quiet", 5);
    NAOS_CHECK_PTR_EQ(ref.option, f.quiet);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 2);

    /* without a selected command only the root is searched */
    ref = clopts_parser_find_long(f.program, NULL, "output", 6);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_long(f.program, NULL, "verbose", 7);
    NAOS_CHECK_PTR_EQ(ref.option, f.verbose);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 0);

    /* the root as the selected command is the same as none */
    ref = clopts_parser_find_long(f.program, clopts_program_root(f.program), "quiet", 5);
    NAOS_CHECK_PTR_EQ(ref.option, f.quiet);
    NAOS_CHECK_TRUE(ref.global);

    /* other commands are not searched */
    ref = clopts_parser_find_long(f.program, f.copy, "cache", 5);
    NAOS_CHECK_NULL(ref.option);
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_long_is_exact_case_sensitive_and_uses_name_len)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, f.build, "Output", 6).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, f.build, "out", 3).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, f.build, "outputs", 7).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, f.build, "", 0).option);

    /* name_len bytes only: "output=x" with name_len 6 */
    ref = clopts_parser_find_long(f.program, f.build, "output=x", 6);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_output);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, f.build, "output=x", 5).option);
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_long_negation_only_for_negatable_flags_with_long_name)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    ref = clopts_parser_find_long(f.program, NULL, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 1);
    NAOS_CHECK_TRUE(ref.negated);

    ref = clopts_parser_find_long(f.program, f.build, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_cache);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 1);
    NAOS_CHECK_TRUE(ref.negated);

    ref = clopts_parser_find_long(f.program, f.copy, "no-color=1", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);
    NAOS_CHECK_TRUE(ref.negated);

    /* not negatable, not a FLAG, not selected, malformed */
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, NULL, "no-quiet", 8).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, f.build, "no-output", 9).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, NULL, "no-", 3).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, NULL, "No-color", 8).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, NULL, "no-Color", 8).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, NULL, "no-col", 6).option);
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, NULL, "nocolor", 7).option);
    /* a negatable FLAG without a long name has no negation */
    NAOS_CHECK_NULL(clopts_parser_find_long(f.program, f.build, "no-r", 4).option);
    clopts_program_destroy(f.program);
}

/*
 * The scopes are searched one after the other, each one completely (exact
 * name, then negation): a command name, exact or negated, shadows the root.
 */
NAOS_TEST(find_long_resolves_each_scope_completely)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    /* the exact no-color of build beats the negation of the global color */
    ref = clopts_parser_find_long(f.program, f.build, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_no_color);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_FALSE(ref.negated);

    /* the negation of the cache of build beats the exact global no-cache */
    ref = clopts_parser_find_long(f.program, f.build, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_cache);
    NAOS_CHECK_TRUE(ref.negated);

    /* in the root, and from another command, the root's names */
    ref = clopts_parser_find_long(f.program, NULL, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.no_cache);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_FALSE(ref.negated);
    ref = clopts_parser_find_long(f.program, f.copy, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.no_cache);
    ref = clopts_parser_find_long(f.program, f.copy, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);
    NAOS_CHECK_TRUE(ref.negated);
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_short_searches_command_then_root)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    ref = clopts_parser_find_short(f.program, f.build, 'o');
    NAOS_CHECK_PTR_EQ(ref.option, f.build_output);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 0);
    NAOS_CHECK_FALSE(ref.negated);

    ref = clopts_parser_find_short(f.program, f.build, 'r');
    NAOS_CHECK_PTR_EQ(ref.option, f.build_release);
    NAOS_CHECK_UINT_EQ(ref.index, 2);

    ref = clopts_parser_find_short(f.program, f.copy, 'v');
    NAOS_CHECK_PTR_EQ(ref.option, f.verbose);
    NAOS_CHECK_TRUE(ref.global);

    ref = clopts_parser_find_short(f.program, NULL, 'q');
    NAOS_CHECK_PTR_EQ(ref.option, f.quiet);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 2);

    NAOS_CHECK_NULL(clopts_parser_find_short(f.program, NULL, 'o').option);
    NAOS_CHECK_NULL(clopts_parser_find_short(f.program, f.copy, 'c').option);
    NAOS_CHECK_NULL(clopts_parser_find_short(f.program, f.build, 'V').option);
    NAOS_CHECK_NULL(clopts_parser_find_short(f.program, f.build, 'O').option);
    NAOS_CHECK_NULL(clopts_parser_find_short(f.program, f.build, '\0').option);
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_conflict_and_named_search_one_command)
{
    lookup_fixture_t f = make_lookup_fixture();
    const clopts_command_t *root = clopts_program_root(f.program);

    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(f.build, "output", '\0'), f.build_output);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(f.build, NULL, 'c'), f.build_cache);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(f.build, "release", 'o'), f.build_output);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(root, "quiet", 'x'), f.quiet);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(root, NULL, 'v'), f.verbose);

    /* neither the root from a command nor the commands from the root */
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.build, NULL, 'v'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.copy, "verbose", '\0'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(root, "cache", '\0'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(root, NULL, 'o'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.copy, "cache", 'c'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.build, "Output", 'O'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.build, NULL, '\0'));

    /* find_named compares exactly name_len bytes */
    NAOS_CHECK_PTR_EQ(clopts_parser_find_named(root, "color", 5), f.color);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_named(f.build, "cachexyz", 5), f.build_cache);
    NAOS_CHECK_NULL(clopts_parser_find_named(f.build, "color", 5));
    NAOS_CHECK_NULL(clopts_parser_find_named(root, "cache", 5));
    NAOS_CHECK_NULL(clopts_parser_find_named(f.build, "colo", 4));
    NAOS_CHECK_NULL(clopts_parser_find_named(f.build, "", 0));
    clopts_program_destroy(f.program);
}

/* ---- shadowing at parse time ----------------------------------------------- */

/*
 * The names the checks allow in both the root and a command, resolved by a
 * parse: root --output/-o VALUE, negatable --color, --no-cache FLAG and
 * --quiet/-q; build --output/-o VALUE, --no-color FLAG and negatable
 * --cache/-c; copy without options (and a positional, so that "copy" alone
 * is a complete command line).
 */
typedef struct shadow_fixture
{
    clopts_program_t *program;
    clopts_option_t *output;
    clopts_option_t *color;
    clopts_option_t *no_cache;
    clopts_option_t *quiet;
    clopts_command_t *build;
    clopts_option_t *build_output;
    clopts_option_t *build_no_color;
    clopts_option_t *build_cache;
    clopts_command_t *copy;
} shadow_fixture_t;

static shadow_fixture_t make_shadow_fixture(void)
{
    shadow_fixture_t f;
    clopts_command_t *root = new_root();

    f.output = add(root, CLOPTS_OPTION_VALUE, "output", 'o');
    f.color = attach(root, make_negatable("color", '\0'));
    f.no_cache = add(root, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    f.quiet = add(root, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.build = command(root, "build");
    f.build_output = add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_no_color = add(f.build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    f.build_cache = attach(f.build, make_negatable("cache", 'c'));
    f.copy = command(root, "copy");
    add_positional(f.copy, "file");
    f.program = finish(root);
    return f;
}

/* A name of the root and of the selected command: the command's after the command name, the root's before. */
NAOS_TEST(shadowed_long_name_resolves_by_position)
{
    static const char *const after[] = { "build", "--output", "x", NULL };
    static const char *const before[] = { "--output", "y", "build", NULL };
    static const char *const both[] = { "--output", "y", "build", "--output=x", NULL };
    static const char *const no_command[] = { "--output", "y", NULL };
    static const char *const other[] = { "copy", "--output", "z", "f", NULL };
    shadow_fixture_t f = make_shadow_fixture();
    clopts_result_t *result;

    result = parse_tokens(f.program, after);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.build);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.build_output), "x");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.build_output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_NULL(clopts_result_value(result, f.output));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.output), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);

    result = parse_tokens(f.program, before);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_NULL(clopts_result_value(result, f.build_output));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.build_output), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);

    result = parse_tokens(f.program, both);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.build_output), "x");
    clopts_result_free(result);

    /* no command: the root's */
    result = parse_tokens(f.program, no_command);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(result);

    /* a command without the name: the root's */
    result = parse_tokens(f.program, other);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), f.copy);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "z");
    NAOS_CHECK_NULL(clopts_result_value(result, f.build_output));
    clopts_result_free(result);
    clopts_program_destroy(f.program);
}

/* Short names likewise: alone, with an attached value and in a group. */
NAOS_TEST(shadowed_short_name_resolves_by_position)
{
    static const char *const after[] = { "build", "-o", "x", NULL };
    static const char *const before[] = { "-oy", "build", NULL };
    static const char *const both[] = { "-qo", "y", "build", "-co", "x", NULL };
    static const char *const no_command[] = { "-o", "y", NULL };
    static const char *const other[] = { "copy", "-qoz", "f", NULL };
    shadow_fixture_t f = make_shadow_fixture();
    clopts_result_t *result;

    result = parse_tokens(f.program, after);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.build_output), "x");
    NAOS_CHECK_NULL(clopts_result_value(result, f.output));
    clopts_result_free(result);

    result = parse_tokens(f.program, before);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    NAOS_CHECK_NULL(clopts_result_value(result, f.build_output));
    clopts_result_free(result);

    result = parse_tokens(f.program, both);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.quiet));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.build_cache));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.build_output), "x");
    clopts_result_free(result);

    result = parse_tokens(f.program, no_command);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "y");
    clopts_result_free(result);

    result = parse_tokens(f.program, other);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.quiet));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, f.output), "z");
    clopts_result_free(result);
    clopts_program_destroy(f.program);
}

/*
 * --no-x across the scopes: the selected command is searched completely
 * (exact name, then negation) before the root. build's exact --no-color
 * beats the negation of the global --color, and build's negation --no-cache
 * beats the global option literally named no-cache.
 */
NAOS_TEST(shadowed_negation_resolves_by_scope)
{
    static const char *const build_no_color[] = { "build", "--no-color", NULL };
    static const char *const root_no_color[] = { "--no-color", "build", NULL };
    static const char *const build_no_cache[] = { "build", "--no-cache", NULL };
    static const char *const root_no_cache[] = { "--no-cache", "build", NULL };
    static const char *const build_color[] = { "build", "--color", "--cache", NULL };
    static const char *const copy_both[] = { "copy", "--no-color", "--no-cache", "f", NULL };
    shadow_fixture_t f = make_shadow_fixture();
    clopts_result_t *result;

    result = parse_tokens(f.program, build_no_color);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.build_no_color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.build_no_color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.color), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);

    result = parse_tokens(f.program, root_no_color);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.build_no_color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.build_no_color), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);

    result = parse_tokens(f.program, build_no_cache);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.build_cache));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.build_cache), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.no_cache));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.no_cache), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);

    result = parse_tokens(f.program, root_no_cache);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.no_cache));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.build_cache), CLOPTS_SOURCE_NONE);
    clopts_result_free(result);

    /* names only one scope has */
    result = parse_tokens(f.program, build_color);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.color));
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.build_cache));
    clopts_result_free(result);

    /* a command without the names: the root's */
    result = parse_tokens(f.program, copy_both);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_flag(result, f.no_cache));
    clopts_result_free(result);
    clopts_program_destroy(f.program);
}

/* --help, -h and --version declared as ordinary options parse as ordinary options. */
NAOS_TEST(help_and_version_options_parse_as_declared)
{
    static const char *const tokens[] = { "--help", "-h", "--version", "x", "build", "--help", NULL };
    static const char *const help_command[] = { "help", "-h", NULL };
    clopts_command_t *root = new_root();
    clopts_command_t *build = command(root, "build");
    clopts_command_t *help = command(root, "help");
    clopts_option_t *root_help = add(root, CLOPTS_OPTION_COUNT, "help", 'h');
    clopts_option_t *version = add(root, CLOPTS_OPTION_VALUE, "version", '\0');
    clopts_option_t *build_help = add(build, CLOPTS_OPTION_FLAG, "help", '\0');
    clopts_option_t *help_h = add(help, CLOPTS_OPTION_FLAG, NULL, 'h');
    clopts_program_t *program = finish(root);
    clopts_result_t *result;

    result = parse_tokens(program, tokens);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), build);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, root_help), 2);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, version), "x");
    NAOS_CHECK_TRUE(clopts_result_flag(result, build_help));
    NAOS_CHECK_NULL(clopts_result_terminal(result));
    clopts_result_free(result);

    result = parse_tokens(program, help_command);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), help);
    NAOS_CHECK_TRUE(clopts_result_flag(result, help_h));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, root_help), 0);
    clopts_result_free(result);
    clopts_program_destroy(program);
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(valid_command_name_rules),
    NAOS_ADD_TEST(valid_long_name_rules),
    NAOS_ADD_TEST(valid_short_name_rules),
    NAOS_ADD_TEST(command_name_invalid),
    NAOS_ADD_TEST(command_named_help_is_accepted),
    NAOS_ADD_TEST(program_new_duplicate_command),
    NAOS_ADD_TEST(command_names_are_case_sensitive),
    NAOS_ADD_TEST(program_new_root_positional_with_commands_is_mixed),
    NAOS_ADD_TEST(program_new_checks_follow_order),
    NAOS_ADD_TEST(program_new_failure_changes_nothing),
    NAOS_ADD_TEST(help_and_version_names_are_accepted),
    NAOS_ADD_TEST(option_same_name_in_root_and_command_is_accepted),
    NAOS_ADD_TEST(negation_across_root_and_command_is_accepted),
    NAOS_ADD_TEST(option_duplicate_in_same_command),
    NAOS_ADD_TEST(option_negation_conflict_on_add),
    NAOS_ADD_TEST(negatable_with_existing_no_option),
    NAOS_ADD_TEST(option_no_prefix_without_negatable_flag_is_allowed),
    NAOS_ADD_TEST(negatable_flag_without_long_name_is_allowed),
    NAOS_ADD_TEST(option_attach_checks_follow_order),
    NAOS_ADD_TEST(positional_after_variadic),
    NAOS_ADD_TEST(required_positional_after_optional),
    NAOS_ADD_TEST(positional_order_allowed),
    NAOS_ADD_TEST(negatable_unset_is_not_checked),
    NAOS_ADD_TEST(releases_accept_null),
    NAOS_ADD_TEST(failed_attach_leaves_ownership_with_the_caller),
    NAOS_ADD_TEST(tool_like_declaration_is_ok),
    NAOS_ADD_TEST(find_command_is_exact_and_case_sensitive),
    NAOS_ADD_TEST(find_long_searches_command_then_root),
    NAOS_ADD_TEST(find_long_is_exact_case_sensitive_and_uses_name_len),
    NAOS_ADD_TEST(find_long_negation_only_for_negatable_flags_with_long_name),
    NAOS_ADD_TEST(find_long_resolves_each_scope_completely),
    NAOS_ADD_TEST(find_short_searches_command_then_root),
    NAOS_ADD_TEST(find_conflict_and_named_search_one_command),
    NAOS_ADD_TEST(shadowed_long_name_resolves_by_position),
    NAOS_ADD_TEST(shadowed_short_name_resolves_by_position),
    NAOS_ADD_TEST(shadowed_negation_resolves_by_scope),
    NAOS_ADD_TEST(help_and_version_options_parse_as_declared)
)
