/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl: the immediate checks of the program, command and attach
 * functions, the name comparisons of one scope (scope.c), the lookup across
 * the selected command and the program (lookup.c), and how a parse resolves
 * the names those checks allow. The checks of an option or a positional
 * alone are in the decl_option and decl_positional suites.
 *
 * Every builder function returns its clopts_decl_error_t right away, and a
 * failing call changes nothing. Each rule therefore has a test that the
 * offending call returns the expected error and leaves everything as it was
 * (the out pointer NULL, the counts of the program and of the command the
 * same, the attribute unchanged, an option, positional or command still owned
 * by the caller, who releases it), and a test of the allowed counterparts.
 *
 * The program and every command have the same rules for their own options
 * and positionals (clopts_program_add_option() / add_positional() and
 * clopts_command_add_option() / add_positional() share the scope code), so
 * each of those rules is tested on a program and on a command (owner_t
 * below). Those checks look at that one scope only. The checks between
 * commands (command names, and the layout of the program: positionals or
 * subcommands, not both) are made by clopts_program_add_command() and
 * clopts_program_add_positional(). The order of the checks within one call is
 * that of program.c, command.c and scope.c.
 *
 * Names are not checked between the global options and a command: there are
 * no reserved names, and a name of a command shadows the same global name.
 * The shadowing tests parse such declarations and read which option got the
 * value.
 *
 * Calls whose error is checked go to the API directly; the helpers of
 * test_decl.h build the surrounding declarations. The model and the name
 * comparisons are internal and are read or called directly.
 */

#include "clopts/parse.h"
#include "decl/lookup.h"
#include "decl/name.h"
#include "decl/scope.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Checks that call returns CLOPTS_DECL_ERROR_NONE. */
#define EXPECT_OK(call) NAOS_CHECK_INT_EQ((call), CLOPTS_DECL_ERROR_NONE)

/* Number of elements of the array a. */
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Objects whose address an out pointer holds before a call, so that a test
 * can see the call set it to NULL. They are never read or written.
 */
static char sentinel_program_storage;
#define SENTINEL_PROGRAM ((clopts_program_t *)&sentinel_program_storage)
static char sentinel_command_storage;
#define SENTINEL_COMMAND ((clopts_command_t *)&sentinel_command_storage)

/*
 * What options and positionals are attached to: the program (its global
 * options and positionals) or a command. Exactly one of the two is set.
 */
typedef struct owner
{
    clopts_program_t *program;
    clopts_command_t *command;
} owner_t;

/* The kinds of owner every scope rule is tested on. */
typedef enum owner_kind
{
    OWNER_PROGRAM,
    OWNER_COMMAND,
    OWNER_KIND_COUNT
} owner_kind_t;

/* program as an owner. */
static owner_t in_program(clopts_program_t *program)
{
    owner_t owner;

    owner.program = program;
    owner.command = NULL;
    return owner;
}

/* command as an owner. */
static owner_t in_command(clopts_command_t *command)
{
    owner_t owner;

    owner.program = NULL;
    owner.command = command;
    return owner;
}

/* A new unattached command named name; the test stops when it cannot be made. */
static clopts_command_t *new_command(const char *name)
{
    clopts_command_t *command = NULL;

    EXPECT_OK(clopts_command_create(name, &command));
    NAOS_REQUIRE_NOT_NULL(command);
    return command;
}

/* A new empty program named name; the test stops when it cannot be made. */
static clopts_program_t *new_program_named(const char *name)
{
    clopts_program_t *program = NULL;

    EXPECT_OK(clopts_program_create(name, &program));
    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/*
 * A new program named "tool" (test_decl_begin()), to be completed with
 * finish(); the test stops when it cannot be made.
 */
static clopts_program_t *new_program(void)
{
    clopts_program_t *program = test_decl_begin("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/*
 * A new empty owner of kind: the program "tool" or the unattached command
 * "build". Notes the kind, so that a failing check names it. Release it with
 * owner_destroy().
 */
static owner_t new_owner(owner_kind_t kind)
{
    if (kind == OWNER_PROGRAM)
    {
        NAOS_NOTE("owner: program");
        return in_program(new_program_named("tool"));
    }
    NAOS_NOTE("owner: command");
    return in_command(new_command("build"));
}

/* Releases owner and everything attached to it. */
static void owner_destroy(owner_t owner)
{
    clopts_program_destroy(owner.program);
    clopts_command_destroy(owner.command);
}

/* clopts_program_add_option() or clopts_command_add_option(). */
static clopts_decl_error_t owner_add_option(owner_t owner, clopts_option_t *option)
{
    return (owner.program != NULL) ? clopts_program_add_option(owner.program, option)
                                   : clopts_command_add_option(owner.command, option);
}

/* clopts_program_add_positional() or clopts_command_add_positional(). */
static clopts_decl_error_t owner_add_positional(owner_t owner, clopts_positional_t *positional)
{
    return (owner.program != NULL) ? clopts_program_add_positional(owner.program, positional)
                                   : clopts_command_add_positional(owner.command, positional);
}

static size_t owner_option_count(owner_t owner)
{
    return (owner.program != NULL) ? clopts_scope_get_option_count(clopts_program_get_scope(owner.program))
                                   : clopts_scope_get_option_count(clopts_command_get_scope(owner.command));
}

static size_t owner_positional_count(owner_t owner)
{
    return (owner.program != NULL) ? clopts_scope_get_positional_count(clopts_program_get_scope(owner.program))
                                   : clopts_scope_get_positional_count(clopts_command_get_scope(owner.command));
}

/* Returns option index of owner. */
static const clopts_option_t *owner_option_at(owner_t owner, size_t index)
{
    return (owner.program != NULL) ? clopts_scope_get_option_at(clopts_program_get_scope(owner.program), index)
                                   : clopts_scope_get_option_at(clopts_command_get_scope(owner.command), index);
}

/* Returns positional index of owner. */
static const clopts_positional_t *owner_positional_at(owner_t owner, size_t index)
{
    return (owner.program != NULL) ? clopts_scope_get_positional_at(clopts_program_get_scope(owner.program), index)
                                   : clopts_scope_get_positional_at(clopts_command_get_scope(owner.command), index);
}

/* A new command remembered for program (test_decl_command()); the test stops when it cannot be made. */
static clopts_command_t *command(clopts_program_t *program, const char *name)
{
    clopts_command_t *result = test_decl_command(program, name);

    NAOS_REQUIRE_NOT_NULL(result);
    return result;
}

/* program with its remembered commands added (test_decl_finish()); the test stops when it fails. */
static clopts_program_t *finish(clopts_program_t *program)
{
    clopts_program_t *result = test_decl_finish(program);

    NAOS_REQUIRE_NOT_NULL(result);
    return result;
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

/* Attaches option to owner; the test stops when the attach fails. */
static clopts_option_t *owner_attach(owner_t owner, clopts_option_t *option)
{
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_REQUIRE_INT_EQ(owner_add_option(owner, option), CLOPTS_DECL_ERROR_NONE);
    return option;
}

/* Attaches positional to owner; the test stops when the attach fails. */
static clopts_positional_t *owner_attach_positional(owner_t owner, clopts_positional_t *positional)
{
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_REQUIRE_INT_EQ(owner_add_positional(owner, positional), CLOPTS_DECL_ERROR_NONE);
    return positional;
}

/* owner_attach(owner, make(kind, long_name, short_name)): an option without attributes. */
static clopts_option_t *owner_add(owner_t owner, clopts_option_kind_t kind, const char *long_name, char short_name)
{
    return owner_attach(owner, make(kind, long_name, short_name));
}

/* owner_attach_positional(owner, make_positional(name)): a positional without attributes. */
static clopts_positional_t *owner_add_named_positional(owner_t owner, const char *name)
{
    return owner_attach_positional(owner, make_positional(name));
}

/* Attaches option to command; the test stops when the attach fails. */
static clopts_option_t *attach(clopts_command_t *command, clopts_option_t *option)
{
    return owner_attach(in_command(command), option);
}

/* Attaches option to program as a global option; the test stops when the attach fails. */
static clopts_option_t *global_attach(clopts_program_t *program, clopts_option_t *option)
{
    return owner_attach(in_program(program), option);
}

/* attach(command, make(kind, long_name, short_name)). */
static clopts_option_t *add(clopts_command_t *command, clopts_option_kind_t kind, const char *long_name,
                            char short_name)
{
    return owner_add(in_command(command), kind, long_name, short_name);
}

/* global_attach(program, make(kind, long_name, short_name)). */
static clopts_option_t *global_add(clopts_program_t *program, clopts_option_kind_t kind, const char *long_name,
                                   char short_name)
{
    return owner_add(in_program(program), kind, long_name, short_name);
}

/* Attaches positional to command; the test stops when the attach fails. */
static clopts_positional_t *attach_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    return owner_attach_positional(in_command(command), positional);
}

/* attach_positional(command, make_positional(name)). */
static clopts_positional_t *add_positional(clopts_command_t *command, const char *name)
{
    return owner_add_named_positional(in_command(command), name);
}

/* Attaches positional to program; the test stops when the attach fails. */
static clopts_positional_t *global_add_positional(clopts_program_t *program, const char *name)
{
    return owner_add_named_positional(in_program(program), name);
}

/*
 * Attaches option to owner expecting error: checks that the options of owner
 * are unchanged, then releases option (the caller still owns it).
 */
static void expect_attach_error(owner_t owner, clopts_option_t *option, clopts_decl_error_t error)
{
    size_t len;

    NAOS_REQUIRE_NOT_NULL(option);
    len = owner_option_count(owner);
    NAOS_CHECK_INT_EQ(owner_add_option(owner, option), error);
    NAOS_CHECK_UINT_EQ(owner_option_count(owner), len);
    clopts_option_destroy(option);
}

/* As expect_attach_error(), for a positional. */
static void expect_attach_positional_error(owner_t owner, clopts_positional_t *positional, clopts_decl_error_t error)
{
    size_t len;

    NAOS_REQUIRE_NOT_NULL(positional);
    len = owner_positional_count(owner);
    NAOS_CHECK_INT_EQ(owner_add_positional(owner, positional), error);
    NAOS_CHECK_UINT_EQ(owner_positional_count(owner), len);
    clopts_positional_destroy(positional);
}

/*
 * Calls clopts_program_add_command(program, command) expecting error and
 * checks that nothing changed: the numbers of subcommands, global options and
 * positionals of program and the numbers of options and positionals of
 * command. command is left with the caller, who still owns it.
 */
static void expect_add_command_error(clopts_program_t *program, clopts_command_t *command, clopts_decl_error_t error)
{
    size_t commands;
    size_t options;
    size_t positionals;
    size_t command_options;
    size_t command_positionals;

    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_NOT_NULL(command);
    commands = clopts_program_get_command_count(program);
    options = clopts_scope_get_option_count(clopts_program_get_scope(program));
    positionals = clopts_scope_get_positional_count(clopts_program_get_scope(program));
    command_options = clopts_scope_get_option_count(clopts_command_get_scope(command));
    command_positionals = clopts_scope_get_positional_count(clopts_command_get_scope(command));

    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, command), error);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), commands);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), options);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), positionals);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(command)), command_options);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(command)), command_positionals);
}

/* Adds command to program; the test stops when the call fails. */
static clopts_command_t *add_command(clopts_program_t *program, clopts_command_t *command)
{
    NAOS_REQUIRE_INT_EQ(clopts_program_add_command(program, command), CLOPTS_DECL_ERROR_NONE);
    return command;
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
    NAOS_CHECK_TRUE(clopts_name_valid_command("build"));
    NAOS_CHECK_TRUE(clopts_name_valid_command("a-b"));
    NAOS_CHECK_TRUE(clopts_name_valid_command("help"));
    NAOS_CHECK_FALSE(clopts_name_valid_command(NULL));
    NAOS_CHECK_FALSE(clopts_name_valid_command(""));
    NAOS_CHECK_FALSE(clopts_name_valid_command("-b"));
    NAOS_CHECK_FALSE(clopts_name_valid_command("a b"));
    NAOS_CHECK_FALSE(clopts_name_valid_command("a\tb"));
    NAOS_CHECK_FALSE(clopts_name_valid_command("ab\n"));
}

NAOS_TEST(valid_long_name_rules)
{
    NAOS_CHECK_TRUE(clopts_name_valid_long("output"));
    NAOS_CHECK_TRUE(clopts_name_valid_long("no-color"));
    NAOS_CHECK_FALSE(clopts_name_valid_long(NULL));
    NAOS_CHECK_FALSE(clopts_name_valid_long(""));
    NAOS_CHECK_FALSE(clopts_name_valid_long("-o"));
    NAOS_CHECK_FALSE(clopts_name_valid_long("a=b"));
    NAOS_CHECK_FALSE(clopts_name_valid_long("a b"));
    NAOS_CHECK_FALSE(clopts_name_valid_long("a\rb"));
}

NAOS_TEST(valid_short_name_rules)
{
    NAOS_CHECK_TRUE(clopts_name_valid_short('o'));
    NAOS_CHECK_TRUE(clopts_name_valid_short('4'));
    NAOS_CHECK_FALSE(clopts_name_valid_short('\0'));
    NAOS_CHECK_FALSE(clopts_name_valid_short('-'));
    NAOS_CHECK_FALSE(clopts_name_valid_short('='));
    NAOS_CHECK_FALSE(clopts_name_valid_short(' '));
    NAOS_CHECK_FALSE(clopts_name_valid_short('\t'));
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
        out = SENTINEL_COMMAND;
        NAOS_CHECK_INT_EQ(clopts_command_create(names[i], &out), CLOPTS_DECL_ERROR_INVALID_NAME);
        NAOS_CHECK_NULL(out);
    }
}

/* ---- clopts_program_create and the program read functions -------------------- */

NAOS_TEST(program_name_invalid)
{
    static const char *const names[] = { NULL, "", "-t", "my tool", "tool\n", "my\ttool" };
    clopts_program_t *out;
    size_t i;

    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("name=\"%s\"", (names[i] != NULL) ? names[i] : "(null)");
        out = SENTINEL_PROGRAM;
        NAOS_CHECK_INT_EQ(clopts_program_create(names[i], &out), CLOPTS_DECL_ERROR_INVALID_NAME);
        NAOS_CHECK_NULL(out);
    }
}

/* A new program is empty and keeps its own copy of the name. */
NAOS_TEST(program_create_is_empty_and_copies_the_name)
{
    char name[] = "tool";
    clopts_program_t *program = new_program_named(name);

    name[0] = 'x';
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "tool");
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);
    NAOS_CHECK_NULL(clopts_program_find_command(program, "tool"));
    clopts_program_destroy(program);
}

NAOS_TEST(program_set_description)
{
    char text[] = "A tool.";
    clopts_program_t *program = new_program_named("tool");

    EXPECT_OK(clopts_program_set_description(program, text));
    text[0] = 'x';
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "A tool.");
    EXPECT_OK(clopts_program_set_description(program, "Another tool."));
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "Another tool.");
    EXPECT_OK(clopts_program_set_description(program, NULL));
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    clopts_program_destroy(program);
}

/*
 * The read functions give the global options, positionals and subcommands in
 * the order of attachment, as the very handles attached. Global options may
 * still be attached after subcommands were added (until the first parse).
 */
NAOS_TEST(program_read_functions_follow_the_order_of_attachment)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_program_t *plain = new_program_named("filt");
    clopts_option_t *verbose = global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_t *color = global_attach(program, make_negatable("color", '\0'));
    clopts_command_t *build = add_command(program, new_command("build"));
    clopts_command_t *copy = add_command(program, new_command("copy"));
    clopts_option_t *quiet = global_add(program, CLOPTS_OPTION_FLAG, "quiet", 'q');
    clopts_positional_t *first = global_add_positional(plain, "first");
    clopts_positional_t *rest = make_positional("rest");

    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 3);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 0), verbose);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 1), color);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_program_get_scope(program), 2), quiet);
    NAOS_REQUIRE_UINT_EQ(clopts_program_get_command_count(program), 2);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 1), copy);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "copy"), copy);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);

    clopts_positional_set_variadic(rest, true);
    owner_attach_positional(in_program(plain), rest);
    NAOS_REQUIRE_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(plain)), 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(plain), 0), first);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_program_get_scope(plain), 1), rest);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(plain), 0);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(plain)), 0);
    clopts_program_destroy(plain);
    clopts_program_destroy(program);
}

/* "help" is an ordinary name: as the program name and as a subcommand. */
NAOS_TEST(command_named_help_is_accepted)
{
    clopts_program_t *program;
    clopts_command_t *help;

    program = new_program_named("help");
    NAOS_CHECK_STR_EQ(clopts_program_get_name(program), "help");
    clopts_program_destroy(program);

    program = new_program_named("tool");
    add_command(program, new_command("build"));
    help = add_command(program, new_command("help"));
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "help"), help);
    clopts_program_destroy(program);
}

/* ---- clopts_program_add_command: DUPLICATE_COMMAND ------------------------------ */

NAOS_TEST(program_add_command_duplicate_command)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = add_command(program, new_command("build"));
    clopts_command_t *again = new_command("build");

    add_command(program, new_command("copy"));
    expect_add_command_error(program, again, CLOPTS_DECL_ERROR_DUPLICATE_COMMAND);
    expect_add_command_error(program, again, CLOPTS_DECL_ERROR_DUPLICATE_COMMAND);
    clopts_command_destroy(again);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "build"), build);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);
    clopts_program_destroy(program);
}

NAOS_TEST(command_names_are_case_sensitive)
{
    static const char *const names[] = { "build", "Build", "buil", "Help", "helper", "tool" };
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *commands[COUNT_OF(names)];
    size_t i;

    /* the name of the program is not a command name: a command may share it */
    for (i = 0; i < COUNT_OF(names); i++)
    {
        commands[i] = add_command(program, new_command(names[i]));
    }
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), COUNT_OF(names));
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, i), commands[i]);
    }
    clopts_program_destroy(program);
}

/* ---- MIXED_LAYOUT: clopts_program_add_command and clopts_program_add_positional ---- */

/* A program with positionals takes no subcommand. */
NAOS_TEST(program_add_command_with_program_positionals_is_mixed)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = new_command("build");
    clopts_command_t *other = new_command("other");

    global_add_positional(program, "file");
    add_positional(build, "file");
    expect_add_command_error(program, build, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    expect_add_command_error(program, other, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    clopts_command_destroy(other);

    /* without subcommands the program takes positionals */
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 1);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 0);
    clopts_program_destroy(program);

    /* and subcommands may have positionals, with global options */
    program = new_program_named("tool");
    global_add(program, CLOPTS_OPTION_FLAG, "verbose", 'v');
    add_command(program, build);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(clopts_program_get_command_at(program, 0))), 1);
    clopts_program_destroy(program);
}

/*
 * A program with subcommands takes no positional, whatever the positional;
 * MIXED_LAYOUT is checked first. (A program never has both, so neither the
 * order DUPLICATE_COMMAND / MIXED_LAYOUT of add_command nor MIXED_LAYOUT /
 * VARIADIC_POSITION / POSITIONAL_ORDER of add_positional can be seen from
 * outside.)
 */
NAOS_TEST(program_add_positional_with_commands_is_mixed)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_positional_t *positional;

    add_command(program, new_command("build"));
    expect_attach_positional_error(in_program(program), make_positional("file"), CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    positional = make_positional("files");
    clopts_positional_set_required(positional, true);
    clopts_positional_set_variadic(positional, true);
    expect_attach_positional_error(in_program(program), positional, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 1);

    /* global options are still allowed */
    global_add(program, CLOPTS_OPTION_FLAG, "verbose", 'v');
    clopts_program_destroy(program);
}

/* ---- failing program calls ---------------------------------------------------- */

/*
 * A failed clopts_program_add_command() or clopts_program_add_positional()
 * allocates and releases nothing, and the caller still owns and may change
 * the command or positional; also when an allocation of the call fails. A
 * failed clopts_program_create() leaves nothing allocated.
 */
NAOS_TEST(program_failed_call_changes_nothing)
{
    clopts_program_t *program;
    clopts_program_t *out;
    clopts_command_t *build;
    clopts_command_t *again;
    clopts_option_t *option;
    clopts_positional_t *positional;
    size_t calls;
    long live;

    test_alloc_begin(0);
    program = new_program_named("tool");
    build = add_command(program, new_command("build"));
    again = new_command("build");
    global_add(program, CLOPTS_OPTION_FLAG, "verbose", 'v');

    /* DUPLICATE_COMMAND, before any allocation */
    live = test_alloc.live;
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_add_command_error(program, again, CLOPTS_DECL_ERROR_DUPLICATE_COMMAND);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);

    /* still the caller's: setters and attaches work, the release works */
    EXPECT_OK(clopts_command_set_summary(again, "Build again."));
    add(again, CLOPTS_OPTION_FLAG, "verbose", 'v');
    live = test_alloc.live;
    clopts_command_destroy(again);
    NAOS_CHECK_INT_LT(test_alloc.live, live);

    /* MIXED_LAYOUT from add_positional, before any allocation; the positional goes elsewhere */
    positional = make_positional("file");
    EXPECT_OK(clopts_positional_set_help(positional, "A file."));
    live = test_alloc.live;
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    NAOS_CHECK_INT_EQ(clopts_program_add_positional(program, positional), CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    EXPECT_OK(clopts_positional_set_help(positional, "Still mine."));
    again = new_command("copy");
    attach_positional(again, positional);
    add_command(program, again);
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_command_get_scope(clopts_program_find_command(program, "copy")), 0), positional);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);
    clopts_program_destroy(program);

    /* MIXED_LAYOUT from add_command, before any allocation */
    program = new_program_named("tool");
    global_add_positional(program, "file");
    build = new_command("build");
    live = test_alloc.live;
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_add_command_error(program, build, CLOPTS_DECL_ERROR_MIXED_LAYOUT);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);

    /* out of memory in add_command (the first one allocates the array) */
    program = new_program_named("tool");
    live = test_alloc.live;
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_add_command_error(program, build, CLOPTS_DECL_ERROR_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    add_command(program, build);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);

    /* out of memory in add_option and add_positional of the program (the first one allocates the array) */
    live = test_alloc.live;
    option = make(CLOPTS_OPTION_FLAG, "verbose", 'v');
    test_alloc_set_fail_at(test_alloc.calls + 1);
    expect_attach_error(in_program(program), option, CLOPTS_DECL_ERROR_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);

    program = new_program_named("filt");
    live = test_alloc.live;
    positional = make_positional("file");
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_attach_positional_error(in_program(program), positional, CLOPTS_DECL_ERROR_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);

    /* out of memory at each allocation of clopts_program_create */
    live = test_alloc.live;
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    out = SENTINEL_PROGRAM;
    NAOS_CHECK_INT_EQ(clopts_program_create("tool", &out), CLOPTS_DECL_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(out);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    test_alloc_set_fail_at(calls + 2);
    out = SENTINEL_PROGRAM;
    NAOS_CHECK_INT_EQ(clopts_program_create("tool", &out), CLOPTS_DECL_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(out);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    test_alloc_set_fail_at(0);

    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- names shared by the program and a command -------------------------------- */

/* The names once reserved for the built-in help and version are ordinary names. */
NAOS_TEST(help_and_version_names_are_accepted)
{
    clopts_program_t *program = new_program();
    clopts_command_t *build = command(program, "build");
    clopts_command_t *help;

    global_add(program, CLOPTS_OPTION_FLAG, "help", 'h');
    global_add(program, CLOPTS_OPTION_FLAG, "version", 'V');
    add(build, CLOPTS_OPTION_VALUE, "help", 'h');
    add(build, CLOPTS_OPTION_VALUE, "version", '\0');
    help = command(program, "help");
    add(help, CLOPTS_OPTION_COUNT, NULL, 'h');
    add(help, CLOPTS_OPTION_LIST, "version", 'v');
    program = finish(program);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(help)), 2);
    clopts_program_destroy(program);
}

/* The same long or short name in the program and in commands, attached in any order. */
NAOS_TEST(option_same_name_in_program_and_command_is_accepted)
{
    clopts_program_t *program = new_program();
    clopts_command_t *build = command(program, "build");
    clopts_command_t *copy = command(program, "copy");

    /* global first */
    global_add(program, CLOPTS_OPTION_FLAG, "verbose", 'v');
    add(build, CLOPTS_OPTION_FLAG, "verbose", 'v');
    add(build, CLOPTS_OPTION_VALUE, "value", 'x');

    /* command first */
    add(copy, CLOPTS_OPTION_FLAG, "release", 'r');
    global_add(program, CLOPTS_OPTION_VALUE, "remote", 'r');
    global_add(program, CLOPTS_OPTION_FLAG, "release", '\0');

    /* the same name in two commands */
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    add(build, CLOPTS_OPTION_FLAG, "Output", 'O');
    add(copy, CLOPTS_OPTION_FLAG, NULL, 'O');

    program = finish(program);

    /* a global option attached after the commands were added */
    global_add(program, CLOPTS_OPTION_COUNT, "output", '\0');
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 4);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(copy)), 3);
    clopts_program_destroy(program);
}

/* A negatable FLAG x in one scope and an option named no-x in another one. */
NAOS_TEST(negation_across_program_and_command_is_accepted)
{
    clopts_program_t *program = new_program();
    clopts_command_t *build = command(program, "build");
    clopts_command_t *copy = command(program, "copy");

    /* a negatable global and no-x in a command, in both orders */
    global_attach(program, make_negatable("color", '\0'));
    add(build, CLOPTS_OPTION_FLAG, "no-color", 'n');
    add(copy, CLOPTS_OPTION_VALUE, "no-cache", '\0');
    global_attach(program, make_negatable("cache", 'c'));

    /* a global no-x and a negatable command flag, in both orders */
    global_add(program, CLOPTS_OPTION_FLAG, "no-quiet", '\0');
    attach(build, make_negatable("quiet", 'q'));
    attach(copy, make_negatable("dry", '\0'));
    global_add(program, CLOPTS_OPTION_LIST, "no-dry", '\0');

    /* a negatable flag of one command and no-x of another */
    attach(build, make_negatable("sign", '\0'));
    add(copy, CLOPTS_OPTION_FLAG, "no-sign", '\0');

    program = finish(program);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 3);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(copy)), 3);
    clopts_program_destroy(program);
}

/* ---- clopts_program_add_option and clopts_command_add_option -------------------- */

NAOS_TEST(option_duplicate_in_same_scope)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);
        clopts_option_t *force;

        force = owner_add(owner, CLOPTS_OPTION_FLAG, "force", 'f');
        expect_attach_error(owner, make(CLOPTS_OPTION_VALUE, "force", 'o'), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
        expect_attach_error(owner, make(CLOPTS_OPTION_COUNT, NULL, 'f'), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
        expect_attach_error(owner, make(CLOPTS_OPTION_FLAG, "force", 'f'), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
        NAOS_CHECK_PTR_EQ(owner_option_at(owner, 0), force);

        owner_add(owner, CLOPTS_OPTION_FLAG, "release", 'r');
        expect_attach_error(owner, make(CLOPTS_OPTION_LIST, "release", '\0'), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
        expect_attach_error(owner, make(CLOPTS_OPTION_VALUE, "remote", 'r'), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);

        /* names are case-sensitive, and long and short names are separate namespaces */
        owner_add(owner, CLOPTS_OPTION_FLAG, "Release", 'R');
        owner_add(owner, CLOPTS_OPTION_FLAG, "r", '\0');
        owner_add(owner, CLOPTS_OPTION_FLAG, "f", 'F');
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 5);
        owner_destroy(owner);
    }
}

/* Only the scope itself is checked: the program and a command do not see each other. */
NAOS_TEST(option_duplicate_is_checked_per_scope)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = new_command("build");

    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    add_command(program, build);
    expect_attach_error(in_program(program), make(CLOPTS_OPTION_VALUE, "force", '\0'),
                        CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    global_add(program, CLOPTS_OPTION_FLAG, "release", 'r');
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 1);
    clopts_program_destroy(program);
}

/* no-x attached after a negatable FLAG x in the same scope */
NAOS_TEST(option_negation_conflict_on_add)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t other = new_owner((owner_kind_t)(OWNER_KIND_COUNT - 1 - k));
        owner_t owner = new_owner((owner_kind_t)k);  /* noted last: the owner under test */

        /* any kind */
        owner_attach(owner, make_negatable("color", '\0'));
        expect_attach_error(owner, make(CLOPTS_OPTION_VALUE, "no-color", '\0'), CLOPTS_DECL_ERROR_NEGATION_CONFLICT);
        expect_attach_error(owner, make(CLOPTS_OPTION_FLAG, "no-color", 'n'), CLOPTS_DECL_ERROR_NEGATION_CONFLICT);
        owner_attach(owner, make_negatable("cache", 'c'));
        expect_attach_error(owner, make(CLOPTS_OPTION_LIST, "no-cache", '\0'), CLOPTS_DECL_ERROR_NEGATION_CONFLICT);
        expect_attach_error(owner, make(CLOPTS_OPTION_COUNT, "no-cache", 'x'), CLOPTS_DECL_ERROR_NEGATION_CONFLICT);

        /* a negatable FLAG that is not the first option of the scope */
        owner_add(owner, CLOPTS_OPTION_FLAG, "quiet", 'q');
        owner_attach(owner, make_negatable("sign", '\0'));
        expect_attach_error(owner, make(CLOPTS_OPTION_FLAG, "no-sign", '\0'), CLOPTS_DECL_ERROR_NEGATION_CONFLICT);
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 4);

        /* only the scope itself is checked */
        owner_add(other, CLOPTS_OPTION_FLAG, "no-color", '\0');
        NAOS_CHECK_UINT_EQ(owner_option_count(other), 1);
        owner_destroy(other);
        owner_destroy(owner);
    }
}

/* a negatable FLAG x attached after no-x in the same scope */
NAOS_TEST(negatable_with_existing_no_option)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t other = new_owner((owner_kind_t)(OWNER_KIND_COUNT - 1 - k));
        owner_t owner = new_owner((owner_kind_t)k);  /* noted last: the owner under test */
        clopts_option_t *option;

        owner_add(owner, CLOPTS_OPTION_VALUE, "no-color", '\0');
        expect_attach_error(owner, make_negatable("color", '\0'), CLOPTS_DECL_ERROR_NEGATION_CONFLICT);

        /* a no-x that is not the first option of the scope */
        owner_add(owner, CLOPTS_OPTION_FLAG, "quiet", 'q');
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-cache", '\0');
        expect_attach_error(owner, make_negatable("cache", 'c'), CLOPTS_DECL_ERROR_NEGATION_CONFLICT);

        /* without negatable the same option is allowed */
        option = make_negatable("cache", '\0');
        EXPECT_OK(clopts_option_set_negatable(option, false));
        owner_attach(owner, option);
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 4);

        /* only the scope itself is checked */
        owner_attach(other, make_negatable("color", '\0'));
        owner_attach(other, make_negatable("cache", '\0'));
        NAOS_CHECK_UINT_EQ(owner_option_count(other), 2);
        owner_destroy(other);
        owner_destroy(owner);
    }
}

NAOS_TEST(option_no_prefix_without_negatable_flag_is_allowed)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);

        owner_add(owner, CLOPTS_OPTION_FLAG, "color", '\0');
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-color", '\0');
        owner_add(owner, CLOPTS_OPTION_VALUE, "no-output", '\0');
        owner_add(owner, CLOPTS_OPTION_VALUE, "output", 'o');
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-", '\0');
        owner_attach(owner, make_negatable("Cache", '\0'));
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-cache", '\0');
        owner_add(owner, CLOPTS_OPTION_FLAG, "No-Cache", '\0');
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 8);
        owner_destroy(owner);
    }
}

NAOS_TEST(negatable_flag_without_long_name_is_allowed)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);
        clopts_option_t *option;

        owner_add(owner, CLOPTS_OPTION_FLAG, "no-", '\0');
        option = owner_attach(owner, make_negatable(NULL, 'c'));
        NAOS_CHECK_TRUE(clopts_option_is_negatable(option));
        owner_destroy(owner);
    }
}

NAOS_TEST(option_attach_checks_follow_order)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);

        owner_add(owner, CLOPTS_OPTION_FLAG, "hidden", '\0');

        /* DUPLICATE_OPTION before NEGATION_CONFLICT, in both directions */
        owner_attach(owner, make_negatable("color", '\0'));
        owner_add(owner, CLOPTS_OPTION_FLAG, "quiet", 'q');
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-cache", '\0');
        expect_attach_error(owner, make(CLOPTS_OPTION_FLAG, "no-color", 'q'), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
        expect_attach_error(owner, make_negatable("cache", 'q'), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 4);
        owner_destroy(owner);
    }
}

/* ---- clopts_program_add_positional and clopts_command_add_positional ------------ */

NAOS_TEST(positional_after_variadic)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);
        clopts_positional_t *files;
        clopts_positional_t *extra;

        files = make_positional("files");
        clopts_positional_set_variadic(files, true);
        owner_attach_positional(owner, files);

        expect_attach_positional_error(owner, make_positional("extra"), CLOPTS_DECL_ERROR_VARIADIC_POSITION);
        extra = make_positional("extra");
        clopts_positional_set_variadic(extra, true);
        expect_attach_positional_error(owner, extra, CLOPTS_DECL_ERROR_VARIADIC_POSITION);

        /* VARIADIC_POSITION before POSITIONAL_ORDER (an optional variadic, then a required one) */
        extra = make_positional("extra");
        clopts_positional_set_required(extra, true);
        expect_attach_positional_error(owner, extra, CLOPTS_DECL_ERROR_VARIADIC_POSITION);
        NAOS_CHECK_PTR_EQ(owner_positional_at(owner, 0), files);
        owner_destroy(owner);
    }
}

NAOS_TEST(required_positional_after_optional)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);
        clopts_positional_t *dst;

        owner_add_named_positional(owner, "src");
        dst = make_positional("dst");
        clopts_positional_set_required(dst, true);
        expect_attach_positional_error(owner, dst, CLOPTS_DECL_ERROR_POSITIONAL_ORDER);

        /* a required variadic one too */
        dst = make_positional("dst");
        clopts_positional_set_required(dst, true);
        clopts_positional_set_variadic(dst, true);
        expect_attach_positional_error(owner, dst, CLOPTS_DECL_ERROR_POSITIONAL_ORDER);

        /* an optional one is allowed */
        owner_add_named_positional(owner, "dst");
        NAOS_CHECK_UINT_EQ(owner_positional_count(owner), 2);
        owner_destroy(owner);
    }
}

NAOS_TEST(positional_order_allowed)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);
        clopts_positional_t *a;
        clopts_positional_t *b;
        clopts_positional_t *c;
        clopts_positional_t *d;

        a = make_positional("a");
        clopts_positional_set_required(a, true);
        owner_attach_positional(owner, a);
        b = make_positional("b");
        clopts_positional_set_required(b, true);
        clopts_positional_set_variadic(b, true);
        clopts_positional_set_variadic(b, false);
        owner_attach_positional(owner, b);
        c = make_positional("c");
        clopts_positional_set_required(c, true);
        clopts_positional_set_required(c, false);
        owner_attach_positional(owner, c);
        d = make_positional("d");
        clopts_positional_set_variadic(d, true);
        owner_attach_positional(owner, d);

        NAOS_REQUIRE_UINT_EQ(owner_positional_count(owner), 4);
        NAOS_CHECK_TRUE(clopts_positional_is_required(a));
        NAOS_CHECK_TRUE(clopts_positional_is_required(b));
        NAOS_CHECK_FALSE(clopts_positional_is_variadic(b));
        NAOS_CHECK_FALSE(clopts_positional_is_required(c));
        NAOS_CHECK_TRUE(clopts_positional_is_variadic(d));
        NAOS_CHECK_PTR_EQ(owner_positional_at(owner, 3), d);
        owner_destroy(owner);

        /* a single required variadic one */
        owner = new_owner((owner_kind_t)k);
        a = make_positional("files");
        clopts_positional_set_required(a, true);
        clopts_positional_set_variadic(a, true);
        owner_attach_positional(owner, a);
        NAOS_CHECK_UINT_EQ(owner_positional_count(owner), 1);
        owner_destroy(owner);
    }
}

/* ---- negatable flags at attach ------------------------------------------ */

NAOS_TEST(negatable_unset_is_not_checked)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);
        clopts_option_t *value;
        clopts_option_t *color;

        value = make(CLOPTS_OPTION_VALUE, "output", 'o');
        EXPECT_OK(clopts_option_set_negatable(value, false));
        owner_attach(owner, value);
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-color", '\0');
        color = make(CLOPTS_OPTION_FLAG, "color", '\0');
        EXPECT_OK(clopts_option_set_negatable(color, false));
        owner_attach(owner, color);
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 3);
        owner_destroy(owner);
    }
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
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *option;
    clopts_positional_t *positional;
    long live;

    test_alloc_begin(0);
    program = new_program();
    build = command(program, "build");
    copy = command(program, "copy");
    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    add(build, CLOPTS_OPTION_FLAG, "force", '\0');

    /* released by the caller */
    option = make(CLOPTS_OPTION_VALUE, "force", '\0');
    EXPECT_OK(clopts_option_set_help(option, "Force."));
    EXPECT_OK(clopts_option_set_value_name(option, "WHEN"));
    EXPECT_OK(clopts_option_set_env(option, "TOOL_FORCE"));
    EXPECT_OK(clopts_option_set_default(option, "always"));
    live = test_alloc.live;
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_INT_EQ(clopts_program_add_option(program, option), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_STR_EQ(clopts_option_get_help(option), "Force.");
    EXPECT_OK(clopts_option_set_help(option, "Still mine."));
    clopts_option_destroy(option);
    NAOS_CHECK_INT_LT(test_alloc.live, live);

    /* attached elsewhere after the failure, then released with the program */
    option = make(CLOPTS_OPTION_VALUE, "out", 'o');
    EXPECT_OK(clopts_option_set_help(option, "Output."));
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    EXPECT_OK(clopts_command_add_option(copy, option));
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_command_get_scope(copy), 0), option);

    /* from the program to a command */
    option = make(CLOPTS_OPTION_COUNT, NULL, 'f');
    NAOS_CHECK_INT_EQ(clopts_program_add_option(program, option), CLOPTS_DECL_ERROR_DUPLICATE_OPTION);
    EXPECT_OK(clopts_command_add_option(copy, option));
    NAOS_CHECK_PTR_EQ(clopts_scope_get_option_at(clopts_command_get_scope(copy), 1), option);

    /* the same for positionals */
    positional = make_positional("rest");
    clopts_positional_set_variadic(positional, true);
    attach_positional(build, positional);
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_help(positional, "Source."));
    live = test_alloc.live;
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(build, positional), CLOPTS_DECL_ERROR_VARIADIC_POSITION);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    EXPECT_OK(clopts_command_add_positional(copy, positional));
    NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(clopts_command_get_scope(copy), 0), positional);

    positional = make_positional("dst");
    clopts_positional_set_required(positional, true);
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(copy, positional), CLOPTS_DECL_ERROR_POSITIONAL_ORDER);
    clopts_positional_destroy(positional);

    program = finish(program);
    clopts_program_destroy(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- valid declarations -------------------------------------------------- */

/*
 * A declaration shaped like the fixture "tool": globals, two commands, a
 * negatable flag, env and default, a list delimiter, a count maximum,
 * required options and positionals with a variadic tail, and a terminal
 * --help in the program and in a command. Every call succeeds, and a program
 * built only from successful calls parses.
 */
NAOS_TEST(tool_like_declaration_is_ok)
{
    static const char *const tokens[] = { "copy", "a", "b", NULL };
    clopts_program_t *program = new_program();
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *option;
    clopts_positional_t *positional;
    clopts_result_t *result;

    EXPECT_OK(clopts_program_set_description(program, "A sample tool."));
    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    EXPECT_OK(clopts_option_set_count_max(option, 3));
    global_attach(program, option);
    global_attach(program, make_negatable("color", '\0'));
    option = make(CLOPTS_OPTION_VALUE, "config", 'c');
    EXPECT_OK(clopts_option_set_env(option, "TOOL_CONFIG"));
    global_attach(program, option);
    option = make(CLOPTS_OPTION_FLAG, "help", 'h');
    EXPECT_OK(clopts_option_set_terminal(option, true));
    global_attach(program, option);

    build = command(program, "build");
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
    clopts_positional_set_required(positional, true);
    attach_positional(build, positional);
    positional = make_positional("rest");
    clopts_positional_set_variadic(positional, true);
    attach_positional(build, positional);

    copy = command(program, "copy");
    add(copy, CLOPTS_OPTION_FLAG, "release", 'r');
    add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    positional = make_positional("src");
    clopts_positional_set_required(positional, true);
    attach_positional(copy, positional);
    positional = make_positional("dst");
    clopts_positional_set_required(positional, true);
    attach_positional(copy, positional);

    program = finish(program);
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "A sample tool.");
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 5);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(build)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(copy)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(copy)), 2);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 2);

    result = parse_tokens(program, tokens);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), copy);
    clopts_result_destroy(result);
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
    clopts_program_t *program = new_program();

    f.verbose = global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    f.color = global_attach(program, make_negatable("color", '\0'));
    f.quiet = global_add(program, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.no_cache = global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    f.build = command(program, "build");
    f.copy = command(program, "copy");
    f.build_output = add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_cache = attach(f.build, make_negatable("cache", 'c'));
    f.build_release = attach(f.build, make_negatable(NULL, 'r'));
    f.build_no_color = add(f.build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    f.copy_output = add(f.copy, CLOPTS_OPTION_VALUE, "output", 'o');
    f.program = finish(program);
    return f;
}

NAOS_TEST(find_command_is_exact_and_case_sensitive)
{
    lookup_fixture_t f = make_lookup_fixture();

    NAOS_CHECK_PTR_EQ(clopts_program_find_command(f.program, "build"), f.build);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(f.program, "copy"), f.copy);
    NAOS_CHECK_NULL(clopts_program_find_command(f.program, "Build"));
    NAOS_CHECK_NULL(clopts_program_find_command(f.program, "bui"));
    NAOS_CHECK_NULL(clopts_program_find_command(f.program, "builds"));
    NAOS_CHECK_NULL(clopts_program_find_command(f.program, ""));
    NAOS_CHECK_NULL(clopts_program_find_command(f.program, "tool"));
    NAOS_CHECK_NULL(clopts_program_find_command(f.program, "help"));
    clopts_program_destroy(f.program);
}

NAOS_TEST(find_long_and_short_compare_exactly_in_one_scope)
{
    lookup_fixture_t f = make_lookup_fixture();
    const clopts_scope_t *global = clopts_program_get_scope(f.program);
    const clopts_scope_t *build = clopts_command_get_scope(f.build);
    size_t index = 99;
    bool negated = true;

    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "output", 6, &index, &negated), f.build_output);
    NAOS_CHECK_UINT_EQ(index, 0);
    NAOS_CHECK_FALSE(negated);
    /* exactly name_len bytes */
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "outputs", 6, &index, &negated), f.build_output);
    /* an exact name comes before the negation of another option */
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "no-color", 8, &index, &negated), f.build_no_color);
    NAOS_CHECK_UINT_EQ(index, 3);
    NAOS_CHECK_FALSE(negated);
    /* --no-<name> of a negatable FLAG */
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "no-cache", 8, &index, &negated), f.build_cache);
    NAOS_CHECK_UINT_EQ(index, 1);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(global, "no-color", 8, &index, &negated), f.color);
    NAOS_CHECK_UINT_EQ(index, 1);
    NAOS_CHECK_TRUE(negated);
    /* no negation of an option that is not a negatable FLAG */
    NAOS_CHECK_NULL(clopts_scope_find_long(global, "no-quiet", 8, &index, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(global, "no-verbose", 10, &index, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "no-", 3, &index, &negated));
    /* exact and case-sensitive, no prefix */
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "Output", 6, &index, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "out", 3, &index, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "", 0, &index, &negated));
    /* a name of the program is not found in a command, nor the other way round */
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "verbose", 7, &index, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(global, "output", 6, &index, &negated));

    NAOS_CHECK_PTR_EQ(clopts_scope_find_short(build, 'r', &index), f.build_release);
    NAOS_CHECK_UINT_EQ(index, 2);
    NAOS_CHECK_PTR_EQ(clopts_scope_find_short(global, 'q', &index), f.quiet);
    NAOS_CHECK_UINT_EQ(index, 2);
    NAOS_CHECK_NULL(clopts_scope_find_short(build, 'O', &index));
    NAOS_CHECK_NULL(clopts_scope_find_short(build, 'v', &index));
    NAOS_CHECK_NULL(clopts_scope_find_short(build, '\0', &index));
    clopts_program_destroy(f.program);
}

NAOS_TEST(lookup_looks_in_the_command_first_then_the_program)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_lookup_t found;

    found = clopts_lookup_long(f.program, f.build, "output", 6);
    NAOS_CHECK_PTR_EQ(found.option, f.build_output);
    NAOS_CHECK_FALSE(found.global);
    NAOS_CHECK_UINT_EQ(found.index, 0);
    NAOS_CHECK_NULL(clopts_lookup_long(f.program, NULL, "output", 6).option);

    found = clopts_lookup_long(f.program, f.build, "verbose", 7);
    NAOS_CHECK_PTR_EQ(found.option, f.verbose);
    NAOS_CHECK_TRUE(found.global);
    NAOS_CHECK_UINT_EQ(found.index, 0);

    /* the command's exact name before the program's negation */
    found = clopts_lookup_long(f.program, f.build, "no-color", 8);
    NAOS_CHECK_PTR_EQ(found.option, f.build_no_color);
    NAOS_CHECK_FALSE(found.negated);
    found = clopts_lookup_long(f.program, f.copy, "no-color", 8);
    NAOS_CHECK_PTR_EQ(found.option, f.color);
    NAOS_CHECK_TRUE(found.global);
    NAOS_CHECK_TRUE(found.negated);
    /* the command's negation before the program's exact name */
    found = clopts_lookup_long(f.program, f.build, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(found.option, f.build_cache);
    NAOS_CHECK_TRUE(found.negated);
    found = clopts_lookup_long(f.program, f.copy, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(found.option, f.no_cache);
    NAOS_CHECK_FALSE(found.negated);

    NAOS_CHECK_PTR_EQ(clopts_lookup_short(f.program, f.build, 'o').option, f.build_output);
    NAOS_CHECK_NULL(clopts_lookup_short(f.program, NULL, 'o').option);
    found = clopts_lookup_short(f.program, f.build, 'v');
    NAOS_CHECK_PTR_EQ(found.option, f.verbose);
    NAOS_CHECK_TRUE(found.global);
    NAOS_CHECK_NULL(clopts_lookup_short(f.program, f.build, '\0').option);
    clopts_program_destroy(f.program);
}

/* ---- shadowing at parse time ----------------------------------------------- */

/*
 * The names the checks allow in both the program and a command, resolved by a
 * parse: global --output/-o VALUE, negatable --color, --no-cache FLAG and
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
    clopts_program_t *program = new_program();

    f.output = global_add(program, CLOPTS_OPTION_VALUE, "output", 'o');
    f.color = global_attach(program, make_negatable("color", '\0'));
    f.no_cache = global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    f.quiet = global_add(program, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.build = command(program, "build");
    f.build_output = add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_no_color = add(f.build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    f.build_cache = attach(f.build, make_negatable("cache", 'c'));
    f.copy = command(program, "copy");
    add_positional(f.copy, "file");
    f.program = finish(program);
    return f;
}

/* A global name and a name of the selected command: the command's after the command name, the global before. */
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
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.build);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.build_output), "x");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.build_output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_NULL(clopts_result_get_value(result, f.output));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);

    result = parse_tokens(f.program, before);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_NULL(clopts_result_get_value(result, f.build_output));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.build_output), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);

    result = parse_tokens(f.program, both);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.build_output), "x");
    clopts_result_destroy(result);

    /* no command: the global one */
    result = parse_tokens(f.program, no_command);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);

    /* a command without the name: the global one */
    result = parse_tokens(f.program, other);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), f.copy);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "z");
    NAOS_CHECK_NULL(clopts_result_get_value(result, f.build_output));
    clopts_result_destroy(result);
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
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.build_output), "x");
    NAOS_CHECK_NULL(clopts_result_get_value(result, f.output));
    clopts_result_destroy(result);

    result = parse_tokens(f.program, before);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    NAOS_CHECK_NULL(clopts_result_get_value(result, f.build_output));
    clopts_result_destroy(result);

    result = parse_tokens(f.program, both);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.quiet));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.build_cache));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.build_output), "x");
    clopts_result_destroy(result);

    result = parse_tokens(f.program, no_command);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "y");
    clopts_result_destroy(result);

    result = parse_tokens(f.program, other);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.quiet));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, f.output), "z");
    clopts_result_destroy(result);
    clopts_program_destroy(f.program);
}

/*
 * --no-x across the scopes: the selected command is searched completely
 * (exact name, then negation) before the global options. build's exact --no-color
 * beats the negation of the global --color, and build's negation --no-cache
 * beats the global option literally named no-cache.
 */
NAOS_TEST(shadowed_negation_resolves_by_scope)
{
    static const char *const build_no_color[] = { "build", "--no-color", NULL };
    static const char *const global_no_color[] = { "--no-color", "build", NULL };
    static const char *const build_no_cache[] = { "build", "--no-cache", NULL };
    static const char *const global_no_cache[] = { "--no-cache", "build", NULL };
    static const char *const build_color[] = { "build", "--color", "--cache", NULL };
    static const char *const copy_both[] = { "copy", "--no-color", "--no-cache", "f", NULL };
    shadow_fixture_t f = make_shadow_fixture();
    clopts_result_t *result;

    result = parse_tokens(f.program, build_no_color);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.build_no_color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.build_no_color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);

    result = parse_tokens(f.program, global_no_color);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.build_no_color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.build_no_color), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);

    result = parse_tokens(f.program, build_no_cache);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.build_cache));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.build_cache), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.no_cache));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.no_cache), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);

    result = parse_tokens(f.program, global_no_cache);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.no_cache));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.build_cache), CLOPTS_SOURCE_NONE);
    clopts_result_destroy(result);

    /* names only one scope has */
    result = parse_tokens(f.program, build_color);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.build_cache));
    clopts_result_destroy(result);

    /* a command without the names: the global ones */
    result = parse_tokens(f.program, copy_both);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, f.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, f.color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, f.no_cache));
    clopts_result_destroy(result);
    clopts_program_destroy(f.program);
}

/* --help, -h and --version declared as ordinary options parse as ordinary options. */
/* A global option is shadowed only when every form it can be given in is found in the command. */
NAOS_TEST(scope_shadows_option_when_every_form_is_found)
{
    shadow_fixture_t f = make_shadow_fixture();
    const clopts_scope_t *build = clopts_command_get_scope(f.build);
    const clopts_scope_t *copy = clopts_command_get_scope(f.copy);

    /* --output and -o */
    NAOS_CHECK_TRUE(clopts_scope_shadows_option(build, f.output));
    NAOS_CHECK_FALSE(clopts_scope_shadows_option(copy, f.output));
    /* --no-cache is found as the negation of the command's negatable --cache */
    NAOS_CHECK_TRUE(clopts_scope_shadows_option(build, f.no_cache));
    /* --no-color is found, --color is not */
    NAOS_CHECK_FALSE(clopts_scope_shadows_option(build, f.color));
    NAOS_CHECK_FALSE(clopts_scope_shadows_option(build, f.quiet));
    clopts_program_destroy(f.program);
}

NAOS_TEST(help_and_version_options_parse_as_declared)
{
    static const char *const tokens[] = { "--help", "-h", "--version", "x", "build", "--help", NULL };
    static const char *const help_command[] = { "help", "-h", NULL };
    clopts_program_t *program = new_program();
    clopts_command_t *build = command(program, "build");
    clopts_command_t *help = command(program, "help");
    clopts_option_t *global_help = global_add(program, CLOPTS_OPTION_COUNT, "help", 'h');
    clopts_option_t *version = global_add(program, CLOPTS_OPTION_VALUE, "version", '\0');
    clopts_option_t *build_help = add(build, CLOPTS_OPTION_FLAG, "help", '\0');
    clopts_option_t *help_h = add(help, CLOPTS_OPTION_FLAG, NULL, 'h');
    clopts_result_t *result;

    program = finish(program);
    result = parse_tokens(program, tokens);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), build);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, global_help), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, version), "x");
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, build_help));
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    clopts_result_destroy(result);

    result = parse_tokens(program, help_command);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), help);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, help_h));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, global_help), 0);
    clopts_result_destroy(result);
    clopts_program_destroy(program);
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(valid_command_name_rules),
    NAOS_ADD_TEST(valid_long_name_rules),
    NAOS_ADD_TEST(valid_short_name_rules),
    NAOS_ADD_TEST(command_name_invalid),
    NAOS_ADD_TEST(program_name_invalid),
    NAOS_ADD_TEST(program_create_is_empty_and_copies_the_name),
    NAOS_ADD_TEST(program_set_description),
    NAOS_ADD_TEST(program_read_functions_follow_the_order_of_attachment),
    NAOS_ADD_TEST(command_named_help_is_accepted),
    NAOS_ADD_TEST(program_add_command_duplicate_command),
    NAOS_ADD_TEST(command_names_are_case_sensitive),
    NAOS_ADD_TEST(program_add_command_with_program_positionals_is_mixed),
    NAOS_ADD_TEST(program_add_positional_with_commands_is_mixed),
    NAOS_ADD_TEST(program_failed_call_changes_nothing),
    NAOS_ADD_TEST(help_and_version_names_are_accepted),
    NAOS_ADD_TEST(option_same_name_in_program_and_command_is_accepted),
    NAOS_ADD_TEST(negation_across_program_and_command_is_accepted),
    NAOS_ADD_TEST(option_duplicate_in_same_scope),
    NAOS_ADD_TEST(option_duplicate_is_checked_per_scope),
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
    NAOS_ADD_TEST(find_long_and_short_compare_exactly_in_one_scope),
    NAOS_ADD_TEST(lookup_looks_in_the_command_first_then_the_program),
    NAOS_ADD_TEST(scope_shadows_option_when_every_form_is_found),
    NAOS_ADD_TEST(shadowed_long_name_resolves_by_position),
    NAOS_ADD_TEST(shadowed_short_name_resolves_by_position),
    NAOS_ADD_TEST(shadowed_negation_resolves_by_scope),
    NAOS_ADD_TEST(help_and_version_options_parse_as_declared)
)
