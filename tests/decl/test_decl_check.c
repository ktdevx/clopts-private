/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl: the checks of the program and command add functions and of the
 * setters that need the other options or positionals of the same scope
 * (clopts_option_set_negatable(), clopts_positional_set_required(),
 * clopts_positional_set_variadic()), the name comparisons of one scope
 * (scope.c), the lookup across the selected command and the program
 * (lookup.c), and how a parse resolves the names those checks allow. The
 * checks of an option or a positional alone are in the decl_option and
 * decl_positional suites; the declaration error itself in decl_error.
 *
 * A failing call changes nothing and leaves its error in the program; from
 * then on every builder call on the program does nothing. Each rule
 * therefore has a test that the offending call, made on a program of its own,
 * leaves the expected error and everything as it was (the add returns NULL,
 * the counts of the program and of the command are the same, the attribute
 * is unchanged, no block is left allocated), and a test of the allowed
 * counterparts.
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
 * The model and the name comparisons are internal and are read or called
 * directly.
 */

#include "clopts/parse/parse.h"
#include "decl/lookup.h"
#include "decl/name.h"
#include "decl/scope_internal.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Number of elements of the array a. */
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* Checks that program has the declaration error error (CLOPTS_ERROR_NONE: none). */
#define EXPECT_ERROR(program, error) NAOS_CHECK_INT_EQ(clopts_program_get_error(program), (error))

/*
 * What options and positionals are added to: the program (its global options
 * and positionals) or a command. command is NULL for the program. program is
 * the program they belong to, whose error the checks read; it is NULL for an
 * owner made by in_command().
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

/* A new empty program named name; the test stops when it cannot be made. */
static clopts_program_t *new_program_named(const char *name)
{
    clopts_program_t *program = test_decl_program(name);

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/* Adds the subcommand name to program; the test stops when the add fails. */
static clopts_command_t *add_command(clopts_program_t *program, const char *name)
{
    clopts_command_t *command = clopts_program_add_command(program, name);

    NAOS_REQUIRE_NOT_NULL(command);
    return command;
}

/*
 * A new empty owner of kind: the program "tool", or the command "build" of a
 * new program "tool". Notes the kind, so that a failing check names it.
 * Release it with owner_destroy().
 */
static owner_t new_owner(owner_kind_t kind)
{
    owner_t owner;

    owner.program = new_program_named("tool");
    owner.command = NULL;
    if (kind == OWNER_PROGRAM)
    {
        NAOS_NOTE("owner: program");
    }
    else
    {
        NAOS_NOTE("owner: command");
        owner.command = add_command(owner.program, "build");
    }
    return owner;
}

/* Releases the program of owner and everything added to it. */
static void owner_destroy(owner_t owner)
{
    clopts_program_destroy(owner.program);
}

/* clopts_program_add_option() or clopts_command_add_option(). */
static clopts_option_t *owner_add_option(owner_t owner, clopts_option_kind_t kind, const char *long_name,
                                         char short_name)
{
    return (owner.command == NULL) ? clopts_program_add_option(owner.program, kind, long_name, short_name)
                                   : clopts_command_add_option(owner.command, kind, long_name, short_name);
}

/* clopts_program_add_positional() or clopts_command_add_positional(). */
static clopts_positional_t *owner_add_positional_call(owner_t owner, const char *name)
{
    return (owner.command == NULL) ? clopts_program_add_positional(owner.program, name)
                                   : clopts_command_add_positional(owner.command, name);
}

static const clopts_scope_t *owner_scope(owner_t owner)
{
    return (owner.command == NULL) ? clopts_program_get_scope(owner.program) : clopts_command_get_scope(owner.command);
}

static size_t owner_option_count(owner_t owner)
{
    return clopts_scope_get_option_count(owner_scope(owner));
}

static size_t owner_positional_count(owner_t owner)
{
    return clopts_scope_get_positional_count(owner_scope(owner));
}

/* Adds an option to owner; the test stops when the add fails. */
static clopts_option_t *owner_add(owner_t owner, clopts_option_kind_t kind, const char *long_name, char short_name)
{
    clopts_option_t *option = owner_add_option(owner, kind, long_name, short_name);

    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/* Adds a negatable FLAG to owner; the test stops when a call fails. */
static clopts_option_t *owner_add_negatable(owner_t owner, const char *long_name, char short_name)
{
    clopts_option_t *option = owner_add(owner, CLOPTS_OPTION_FLAG, long_name, short_name);

    clopts_option_set_negatable(option, true);
    NAOS_REQUIRE_TRUE(clopts_option_is_negatable(option));
    return option;
}

/* Adds a positional to owner; the test stops when the add fails. */
static clopts_positional_t *owner_add_positional(owner_t owner, const char *name)
{
    clopts_positional_t *positional = owner_add_positional_call(owner, name);

    NAOS_REQUIRE_NOT_NULL(positional);
    return positional;
}

/* owner_add() on command. */
static clopts_option_t *add(clopts_command_t *command, clopts_option_kind_t kind, const char *long_name,
                            char short_name)
{
    return owner_add(in_command(command), kind, long_name, short_name);
}

/* owner_add() on program: a global option. */
static clopts_option_t *global_add(clopts_program_t *program, clopts_option_kind_t kind, const char *long_name,
                                   char short_name)
{
    return owner_add(in_program(program), kind, long_name, short_name);
}

/* owner_add_negatable() on command. */
static clopts_option_t *add_negatable(clopts_command_t *command, const char *long_name, char short_name)
{
    return owner_add_negatable(in_command(command), long_name, short_name);
}

/* owner_add_negatable() on program. */
static clopts_option_t *global_add_negatable(clopts_program_t *program, const char *long_name, char short_name)
{
    return owner_add_negatable(in_program(program), long_name, short_name);
}

/* owner_add_positional() on command. */
static clopts_positional_t *add_positional(clopts_command_t *command, const char *name)
{
    return owner_add_positional(in_command(command), name);
}

/* owner_add_positional() on program. */
static clopts_positional_t *global_add_positional(clopts_program_t *program, const char *name)
{
    return owner_add_positional(in_program(program), name);
}

/* The program, once declared: the test stops when it has an error. */
static clopts_program_t *finish(clopts_program_t *program)
{
    NAOS_REQUIRE_PTR_EQ(test_decl_finish(program), program);
    return program;
}

/* What a failing call must leave as it was: the counts of owner and the blocks allocated. */
typedef struct owner_mark
{
    size_t options;
    size_t positionals;
    size_t commands;
    long live;
} owner_mark_t;

static owner_mark_t owner_mark(owner_t owner)
{
    owner_mark_t mark;

    mark.options = owner_option_count(owner);
    mark.positionals = owner_positional_count(owner);
    mark.commands = clopts_program_get_command_count(owner.program);
    mark.live = test_alloc.live;
    return mark;
}

/* Checks that the last call failed with error and left owner as mark recorded it. */
static void check_failed(owner_t owner, const owner_mark_t *mark, clopts_error_t error)
{
    EXPECT_ERROR(owner.program, error);
    NAOS_CHECK_UINT_EQ(owner_option_count(owner), mark->options);
    NAOS_CHECK_UINT_EQ(owner_positional_count(owner), mark->positionals);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(owner.program), mark->commands);
    NAOS_CHECK_INT_EQ(test_alloc.live, mark->live);
}

/* Adds an option to owner expecting error: the add returns NULL and owner does not change. */
static void expect_add_option_error(owner_t owner, clopts_option_kind_t kind, const char *long_name,
                                    char short_name, clopts_error_t error)
{
    owner_mark_t mark = owner_mark(owner);

    NAOS_CHECK_NULL(owner_add_option(owner, kind, long_name, short_name));
    check_failed(owner, &mark, error);
}

/* As expect_add_option_error(), for a positional. */
static void expect_add_positional_error(owner_t owner, const char *name, clopts_error_t error)
{
    owner_mark_t mark = owner_mark(owner);

    NAOS_CHECK_NULL(owner_add_positional_call(owner, name));
    check_failed(owner, &mark, error);
}

/* As expect_add_option_error(), for clopts_program_add_command() (the owner is the program). */
static void expect_add_command_error(clopts_program_t *program, const char *name, clopts_error_t error)
{
    owner_t owner = in_program(program);
    owner_mark_t mark = owner_mark(owner);

    NAOS_CHECK_NULL(clopts_program_add_command(program, name));
    check_failed(owner, &mark, error);
}

/* An option to add in a test of the add checks. */
typedef struct option_spec
{
    clopts_option_kind_t kind;
    const char *long_name;
    char short_name;
} option_spec_t;

/*
 * For every kind of owner and every option of specs: makes a new owner,
 * declares base in it, adds the option expecting error, and releases the
 * owner.
 */
static void expect_add_option_errors(void (*base)(owner_t owner), const option_spec_t *specs, size_t count,
                                     clopts_error_t error)
{
    owner_t owner;
    size_t k;
    size_t i;

    test_alloc_begin(0);
    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        for (i = 0; i < count; i++)
        {
            owner = new_owner((owner_kind_t)k);
            NAOS_NOTE("option %u: --%s -%c", (unsigned)i, (specs[i].long_name != NULL) ? specs[i].long_name : "",
                      (specs[i].short_name != '\0') ? specs[i].short_name : ' ');
            base(owner);
            EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
            expect_add_option_error(owner, specs[i].kind, specs[i].long_name, specs[i].short_name, error);
            owner_destroy(owner);
        }
    }
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
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

/* ---- clopts_program_add_command: INVALID_NAME ----------------------------- */

/* A malformed command name gives INVALID_NAME before any allocation. */
NAOS_TEST(command_name_invalid)
{
    static const char *const names[] = { NULL, "", "-b", "b c", "b\nc", "my\ttool" };
    clopts_program_t *program;
    size_t calls;
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("name=\"%s\"", (names[i] != NULL) ? names[i] : "(null)");
        program = new_program_named("tool");
        add_command(program, "copy");
        calls = test_alloc.calls;
        expect_add_command_error(program, names[i], CLOPTS_ERROR_INVALID_NAME);
        NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
        clopts_program_destroy(program);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- clopts_program_create and the program read functions -------------------- */

/* A malformed program name gives a program with the error INVALID_NAME. */
NAOS_TEST(program_name_invalid)
{
    static const char *const names[] = { NULL, "", "-t", "my tool", "tool\n", "my\ttool" };
    clopts_program_t *program;
    size_t i;

    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("name=\"%s\"", (names[i] != NULL) ? names[i] : "(null)");
        program = clopts_program_create(names[i]);
        NAOS_REQUIRE_NOT_NULL(program);
        EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
        clopts_program_destroy(program);
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

    clopts_program_set_description(program, text);
    text[0] = 'x';
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "A tool.");
    clopts_program_set_description(program, "Another tool.");
    NAOS_CHECK_STR_EQ(clopts_program_get_description(program), "Another tool.");
    clopts_program_set_description(program, NULL);
    NAOS_CHECK_NULL(clopts_program_get_description(program));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/*
 * The read functions give the global options, positionals and subcommands in
 * the order of addition, as the very handles the adds returned. Global
 * options may still be added after subcommands (until the first parse).
 */
NAOS_TEST(program_read_functions_follow_the_order_of_addition)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_program_t *plain = new_program_named("filt");
    clopts_option_t *verbose = global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_t *color = global_add_negatable(program, "color", '\0');
    clopts_command_t *build = add_command(program, "build");
    clopts_command_t *copy = add_command(program, "copy");
    clopts_option_t *quiet = global_add(program, CLOPTS_OPTION_FLAG, "quiet", 'q');
    clopts_positional_t *first = global_add_positional(plain, "first");
    clopts_positional_t *rest = global_add_positional(plain, "rest");

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
    EXPECT_ERROR(plain, CLOPTS_ERROR_NONE);
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
    add_command(program, "build");
    help = add_command(program, "help");
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "help"), help);
    clopts_program_destroy(program);
}

/* ---- clopts_program_add_command: DUPLICATE ------------------------------ */

NAOS_TEST(program_add_command_duplicate_command)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = add_command(program, "build");

    add_command(program, "copy");
    expect_add_command_error(program, "build", CLOPTS_ERROR_DUPLICATE);
    NAOS_CHECK_PTR_EQ(clopts_program_find_command(program, "build"), build);
    NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, 0), build);
    /* the same call again does nothing and keeps the error */
    expect_add_command_error(program, "build", CLOPTS_ERROR_DUPLICATE);
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
        commands[i] = add_command(program, names[i]);
    }
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), COUNT_OF(names));
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_program_get_command_at(program, i), commands[i]);
    }
    clopts_program_destroy(program);
}

/* ---- CONFLICT: clopts_program_add_command and clopts_program_add_positional ---- */

/* A program with positionals takes no subcommand, whatever its positionals are. */
NAOS_TEST(program_add_command_with_program_positionals_is_mixed)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_positional_t *files;

    program = new_program_named("tool");
    global_add_positional(program, "file");
    expect_add_command_error(program, "build", CLOPTS_ERROR_CONFLICT);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 1);
    clopts_program_destroy(program);

    program = new_program_named("tool");
    files = global_add_positional(program, "files");
    clopts_positional_set_required(files, true);
    clopts_positional_set_variadic(files, true);
    expect_add_command_error(program, "other", CLOPTS_ERROR_CONFLICT);
    clopts_program_destroy(program);

    /* subcommands may have positionals, with global options */
    program = new_program_named("tool");
    global_add(program, CLOPTS_OPTION_FLAG, "verbose", 'v');
    build = add_command(program, "build");
    add_positional(build, "file");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(build)), 1);
    clopts_program_destroy(program);
}

/*
 * A program with subcommands takes no positional. INVALID_NAME comes before
 * CONFLICT. (A program never has both, so neither the order DUPLICATE /
 * CONFLICT of add_command nor CONFLICT / ORDER of add_positional can be seen
 * from outside.)
 */
NAOS_TEST(program_add_positional_with_commands_is_mixed)
{
    clopts_program_t *program;

    program = new_program_named("tool");
    add_command(program, "build");
    expect_add_positional_error(in_program(program), "file", CLOPTS_ERROR_CONFLICT);
    NAOS_CHECK_UINT_EQ(clopts_program_get_command_count(program), 1);
    clopts_program_destroy(program);

    program = new_program_named("tool");
    add_command(program, "build");
    expect_add_positional_error(in_program(program), "", CLOPTS_ERROR_INVALID_NAME);
    clopts_program_destroy(program);

    program = new_program_named("tool");
    add_command(program, "build");
    expect_add_positional_error(in_program(program), NULL, CLOPTS_ERROR_INVALID_NAME);
    clopts_program_destroy(program);

    /* global options are still allowed */
    program = new_program_named("tool");
    add_command(program, "build");
    global_add(program, CLOPTS_OPTION_FLAG, "verbose", 'v');
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/* ---- failing program calls ---------------------------------------------------- */

/*
 * A clopts_program_add_command() or clopts_program_add_positional() that
 * breaks a rule between commands fails before any allocation. An add that
 * runs out of memory leaves nothing allocated and nothing added, and a failed
 * clopts_program_create() leaves nothing allocated and gives NULL.
 */
NAOS_TEST(program_failed_call_changes_nothing)
{
    clopts_program_t *program;
    size_t calls;
    long live;

    test_alloc_begin(0);

    /* DUPLICATE, before any allocation */
    program = new_program_named("tool");
    add_command(program, "build");
    global_add(program, CLOPTS_OPTION_FLAG, "verbose", 'v');
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_add_command_error(program, "build", CLOPTS_ERROR_DUPLICATE);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    clopts_program_destroy(program);

    /* CONFLICT from add_positional, before any allocation */
    program = new_program_named("tool");
    add_command(program, "build");
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_add_positional_error(in_program(program), "file", CLOPTS_ERROR_CONFLICT);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    clopts_program_destroy(program);

    /* CONFLICT from add_command, before any allocation */
    program = new_program_named("tool");
    global_add_positional(program, "file");
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    expect_add_command_error(program, "build", CLOPTS_ERROR_CONFLICT);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    clopts_program_destroy(program);

    /* out of memory in add_command */
    program = new_program_named("tool");
    test_alloc_set_fail_at(test_alloc.calls + 1);
    expect_add_command_error(program, "build", CLOPTS_ERROR_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    clopts_program_destroy(program);

    /* out of memory in add_option and add_positional of the program */
    program = new_program_named("tool");
    test_alloc_set_fail_at(test_alloc.calls + 1);
    expect_add_option_error(in_program(program), CLOPTS_OPTION_FLAG, "verbose", 'v', CLOPTS_ERROR_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    clopts_program_destroy(program);

    program = new_program_named("filt");
    test_alloc_set_fail_at(test_alloc.calls + 1);
    expect_add_positional_error(in_program(program), "file", CLOPTS_ERROR_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    clopts_program_destroy(program);

    /* out of memory at each allocation of clopts_program_create */
    live = test_alloc.live;
    calls = test_alloc.calls;
    test_alloc_set_fail_at(calls + 1);
    program = clopts_program_create("tool");
    NAOS_CHECK_NULL(program);
    EXPECT_ERROR(program, CLOPTS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    test_alloc_set_fail_at(test_alloc.calls + 2);
    program = clopts_program_create("tool");
    NAOS_CHECK_NULL(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    test_alloc_set_fail_at(0);

    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- names shared by the program and a command -------------------------------- */

/* The names once reserved for the built-in help and version are ordinary names. */
NAOS_TEST(help_and_version_names_are_accepted)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = add_command(program, "build");
    clopts_command_t *help;

    global_add(program, CLOPTS_OPTION_FLAG, "help", 'h');
    global_add(program, CLOPTS_OPTION_FLAG, "version", 'V');
    add(build, CLOPTS_OPTION_VALUE, "help", 'h');
    add(build, CLOPTS_OPTION_VALUE, "version", '\0');
    help = add_command(program, "help");
    add(help, CLOPTS_OPTION_COUNT, NULL, 'h');
    add(help, CLOPTS_OPTION_LIST, "version", 'v');
    program = finish(program);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(help)), 2);
    clopts_program_destroy(program);
}

/* The same long or short name in the program and in commands, added in any order. */
NAOS_TEST(option_same_name_in_program_and_command_is_accepted)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = add_command(program, "build");
    clopts_command_t *copy = add_command(program, "copy");

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

    /* a global option added after the commands */
    global_add(program, CLOPTS_OPTION_COUNT, "output", '\0');
    program = finish(program);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 4);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(copy)), 3);
    clopts_program_destroy(program);
}

/*
 * A negatable FLAG x in one scope and an option named no-x in another one,
 * whether the flag is made negatable before or after no-x is added.
 */
NAOS_TEST(negation_across_program_and_command_is_accepted)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = add_command(program, "build");
    clopts_command_t *copy = add_command(program, "copy");

    /* a negatable global and no-x in a command, in both orders */
    global_add_negatable(program, "color", '\0');
    add(build, CLOPTS_OPTION_FLAG, "no-color", 'n');
    add(copy, CLOPTS_OPTION_VALUE, "no-cache", '\0');
    global_add_negatable(program, "cache", 'c');

    /* a global no-x and a negatable command flag, in both orders */
    global_add(program, CLOPTS_OPTION_FLAG, "no-quiet", '\0');
    add_negatable(build, "quiet", 'q');
    add_negatable(copy, "dry", '\0');
    global_add(program, CLOPTS_OPTION_LIST, "no-dry", '\0');

    /* a negatable flag of one command and no-x of another */
    add_negatable(build, "sign", '\0');
    add(copy, CLOPTS_OPTION_FLAG, "no-sign", '\0');

    program = finish(program);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 4);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 3);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(copy)), 3);
    clopts_program_destroy(program);
}

/* ---- clopts_program_add_option and clopts_command_add_option -------------------- */

/* force/f and release/r. */
static void duplicate_base(owner_t owner)
{
    owner_add(owner, CLOPTS_OPTION_FLAG, "force", 'f');
    owner_add(owner, CLOPTS_OPTION_FLAG, "release", 'r');
}

NAOS_TEST(option_duplicate_in_same_scope)
{
    static const option_spec_t duplicates[] = {
        { CLOPTS_OPTION_VALUE, "force", 'o' },
        { CLOPTS_OPTION_COUNT, NULL, 'f' },
        { CLOPTS_OPTION_FLAG, "force", 'f' },
        { CLOPTS_OPTION_LIST, "release", '\0' },
        { CLOPTS_OPTION_VALUE, "remote", 'r' },
    };
    owner_t owner;
    size_t k;

    expect_add_option_errors(duplicate_base, duplicates, COUNT_OF(duplicates), CLOPTS_ERROR_DUPLICATE);

    /* names are case-sensitive, and long and short names are separate namespaces */
    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner = new_owner((owner_kind_t)k);
        duplicate_base(owner);
        owner_add(owner, CLOPTS_OPTION_FLAG, "Release", 'R');
        owner_add(owner, CLOPTS_OPTION_FLAG, "r", '\0');
        owner_add(owner, CLOPTS_OPTION_FLAG, "f", 'F');
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 5);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        owner_destroy(owner);
    }
}

/* Only the scope itself is checked: the program and a command do not see each other. */
NAOS_TEST(option_duplicate_is_checked_per_scope)
{
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = add_command(program, "build");

    global_add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    global_add(program, CLOPTS_OPTION_FLAG, "release", 'r');
    expect_add_option_error(in_program(program), CLOPTS_OPTION_VALUE, "force", '\0', CLOPTS_ERROR_DUPLICATE);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 2);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(build)), 1);
    clopts_program_destroy(program);
}

/* negatable color, negatable cache/c, quiet/q and negatable sign. */
static void negation_base(owner_t owner)
{
    owner_add_negatable(owner, "color", '\0');
    owner_add_negatable(owner, "cache", 'c');
    owner_add(owner, CLOPTS_OPTION_FLAG, "quiet", 'q');
    owner_add_negatable(owner, "sign", '\0');
}

/*
 * no-x added after a negatable FLAG x in the same scope, of any kind, also
 * when the flag is not the first option of the scope. (Another scope is not
 * checked: negation_across_program_and_command_is_accepted.)
 */
NAOS_TEST(option_negation_conflict_on_add)
{
    static const option_spec_t negations[] = {
        { CLOPTS_OPTION_VALUE, "no-color", '\0' },
        { CLOPTS_OPTION_FLAG, "no-color", 'n' },
        { CLOPTS_OPTION_LIST, "no-cache", '\0' },
        { CLOPTS_OPTION_COUNT, "no-cache", 'x' },
        { CLOPTS_OPTION_FLAG, "no-sign", '\0' },
    };

    expect_add_option_errors(negation_base, negations, COUNT_OF(negations), CLOPTS_ERROR_CONFLICT);
}

/*
 * clopts_option_set_negatable(true) on a FLAG x when an option named no-x is
 * in the same scope gives CONFLICT. The failing call allocates nothing and
 * leaves the flag as it was; only the scope of the flag is checked.
 */
NAOS_TEST(set_negatable_conflicts_with_existing_no_option)
{
    owner_t owner;
    clopts_option_t *option;
    owner_mark_t mark;
    size_t calls;
    size_t k;

    test_alloc_begin(0);
    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        /* no-x is the first option */
        owner = new_owner((owner_kind_t)k);
        owner_add(owner, CLOPTS_OPTION_VALUE, "no-color", '\0');
        option = owner_add(owner, CLOPTS_OPTION_FLAG, "color", '\0');
        mark = owner_mark(owner);
        calls = test_alloc.calls;
        clopts_option_set_negatable(option, true);
        check_failed(owner, &mark, CLOPTS_ERROR_CONFLICT);
        NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
        NAOS_CHECK_FALSE(clopts_option_is_negatable(option));
        owner_destroy(owner);

        /* no-x is not the first option, and comes after x */
        owner = new_owner((owner_kind_t)k);
        owner_add(owner, CLOPTS_OPTION_FLAG, "quiet", 'q');
        option = owner_add(owner, CLOPTS_OPTION_FLAG, "cache", 'c');
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-cache", '\0');
        mark = owner_mark(owner);
        clopts_option_set_negatable(option, true);
        check_failed(owner, &mark, CLOPTS_ERROR_CONFLICT);
        NAOS_CHECK_FALSE(clopts_option_is_negatable(option));
        owner_destroy(owner);

        /* without negatable the same option is allowed, and false is not checked */
        owner = new_owner((owner_kind_t)k);
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-cache", '\0');
        option = owner_add(owner, CLOPTS_OPTION_FLAG, "cache", '\0');
        clopts_option_set_negatable(option, false);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 2);
        owner_destroy(owner);
    }

    /* only the scope of the flag is checked */
    {
        clopts_program_t *program = new_program_named("tool");
        clopts_command_t *build = add_command(program, "build");

        add(build, CLOPTS_OPTION_FLAG, "no-color", '\0');
        global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0');
        global_add_negatable(program, "color", '\0');
        add_negatable(build, "cache", '\0');
        EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
        clopts_program_destroy(program);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
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
        owner_add_negatable(owner, "Cache", '\0');
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-cache", '\0');
        owner_add(owner, CLOPTS_OPTION_FLAG, "No-Cache", '\0');
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 8);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
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
        option = owner_add_negatable(owner, NULL, 'c');
        NAOS_CHECK_TRUE(clopts_option_is_negatable(option));
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        owner_destroy(owner);
    }
}

/* hidden, negatable color, quiet/q and no-cache. */
static void order_base(owner_t owner)
{
    owner_add(owner, CLOPTS_OPTION_FLAG, "hidden", '\0');
    owner_add_negatable(owner, "color", '\0');
    owner_add(owner, CLOPTS_OPTION_FLAG, "quiet", 'q');
    owner_add(owner, CLOPTS_OPTION_FLAG, "no-cache", '\0');
}

/*
 * The checks of an add come in a fixed order: INVALID_NAME (the option
 * alone) before DUPLICATE before CONFLICT. clopts_option_set_negatable()
 * checks NOT_APPLICABLE before CONFLICT.
 */
NAOS_TEST(option_add_checks_follow_order)
{
    static const option_spec_t duplicate_first[] = {
        { CLOPTS_OPTION_FLAG, "no-color", 'q' },
    };
    static const option_spec_t invalid_first[] = {
        { CLOPTS_OPTION_FLAG, "quiet", '-' },
        { CLOPTS_OPTION_FLAG, "no-color", '=' },
    };
    owner_t owner;
    clopts_option_t *option;
    size_t k;

    expect_add_option_errors(order_base, duplicate_first, COUNT_OF(duplicate_first), CLOPTS_ERROR_DUPLICATE);
    expect_add_option_errors(order_base, invalid_first, COUNT_OF(invalid_first), CLOPTS_ERROR_INVALID_NAME);

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner = new_owner((owner_kind_t)k);
        order_base(owner);
        option = owner_add(owner, CLOPTS_OPTION_VALUE, "cache", '\0');
        clopts_option_set_negatable(option, true);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NOT_APPLICABLE);
        NAOS_CHECK_FALSE(clopts_option_is_negatable(option));
        owner_destroy(owner);
    }
}

/* ---- positionals: the add functions -------------------------------------- */

/* Nothing can be added after a variadic positional (ORDER); the add allocates nothing for good. */
NAOS_TEST(positional_after_variadic)
{
    owner_t owner;
    clopts_positional_t *files;
    size_t k;

    test_alloc_begin(0);
    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner = new_owner((owner_kind_t)k);
        files = owner_add_positional(owner, "files");
        clopts_positional_set_variadic(files, true);
        expect_add_positional_error(owner, "extra", CLOPTS_ERROR_ORDER);
        NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(owner_scope(owner), 0), files);
        NAOS_CHECK_TRUE(clopts_positional_is_variadic(files));
        owner_destroy(owner);

        /* a required variadic one too */
        owner = new_owner((owner_kind_t)k);
        files = owner_add_positional(owner, "files");
        clopts_positional_set_required(files, true);
        clopts_positional_set_variadic(files, true);
        expect_add_positional_error(owner, "extra", CLOPTS_ERROR_ORDER);
        owner_destroy(owner);

        /* INVALID_NAME before ORDER */
        owner = new_owner((owner_kind_t)k);
        files = owner_add_positional(owner, "files");
        clopts_positional_set_variadic(files, true);
        expect_add_positional_error(owner, "", CLOPTS_ERROR_INVALID_NAME);
        owner_destroy(owner);

        /* once it is not variadic any more, more can be added */
        owner = new_owner((owner_kind_t)k);
        files = owner_add_positional(owner, "files");
        clopts_positional_set_variadic(files, true);
        clopts_positional_set_variadic(files, false);
        owner_add_positional(owner, "extra");
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        owner_destroy(owner);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- positionals: clopts_positional_set_required() / set_variadic() --------- */

/*
 * A positional cannot be made required after an optional one (ORDER). The
 * failing call allocates nothing and leaves the positional as it was.
 */
NAOS_TEST(required_positional_after_optional)
{
    owner_t owner;
    clopts_positional_t *dst;
    owner_mark_t mark;
    size_t calls;
    size_t k;

    test_alloc_begin(0);
    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner = new_owner((owner_kind_t)k);
        owner_add_positional(owner, "src");
        dst = owner_add_positional(owner, "dst");
        mark = owner_mark(owner);
        calls = test_alloc.calls;
        clopts_positional_set_required(dst, true);
        check_failed(owner, &mark, CLOPTS_ERROR_ORDER);
        NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
        NAOS_CHECK_FALSE(clopts_positional_is_required(dst));
        owner_destroy(owner);

        /* a required variadic one too */
        owner = new_owner((owner_kind_t)k);
        owner_add_positional(owner, "src");
        dst = owner_add_positional(owner, "dst");
        clopts_positional_set_variadic(dst, true);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        clopts_positional_set_required(dst, true);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_ORDER);
        NAOS_CHECK_FALSE(clopts_positional_is_required(dst));
        NAOS_CHECK_TRUE(clopts_positional_is_variadic(dst));
        owner_destroy(owner);

        /* an optional one after an optional one is allowed */
        owner = new_owner((owner_kind_t)k);
        owner_add_positional(owner, "src");
        dst = owner_add_positional(owner, "dst");
        clopts_positional_set_required(dst, false);
        NAOS_CHECK_UINT_EQ(owner_positional_count(owner), 2);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        owner_destroy(owner);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * The other ways a setter could break the order: a variadic positional that
 * is not the last one, and an optional positional before a required one
 * (made optional after the later one was made required). Each failing call
 * allocates nothing and leaves the positional as it was.
 */
NAOS_TEST(positional_setters_keep_the_order)
{
    owner_t owner;
    clopts_positional_t *a;
    clopts_positional_t *b;
    owner_mark_t mark;
    size_t calls;
    size_t k;

    test_alloc_begin(0);
    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        /* variadic, not the last one */
        owner = new_owner((owner_kind_t)k);
        a = owner_add_positional(owner, "a");
        owner_add_positional(owner, "b");
        mark = owner_mark(owner);
        calls = test_alloc.calls;
        clopts_positional_set_variadic(a, true);
        check_failed(owner, &mark, CLOPTS_ERROR_ORDER);
        NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
        NAOS_CHECK_FALSE(clopts_positional_is_variadic(a));
        owner_destroy(owner);

        /* optional before a required one */
        owner = new_owner((owner_kind_t)k);
        a = owner_add_positional(owner, "a");
        b = owner_add_positional(owner, "b");
        clopts_positional_set_required(a, true);
        clopts_positional_set_required(b, true);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        clopts_positional_set_required(a, false);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_ORDER);
        NAOS_CHECK_TRUE(clopts_positional_is_required(a));
        NAOS_CHECK_TRUE(clopts_positional_is_required(b));
        owner_destroy(owner);

        /* the allowed counterparts: required before optional, variadic last, resets */
        owner = new_owner((owner_kind_t)k);
        a = owner_add_positional(owner, "a");
        b = owner_add_positional(owner, "b");
        clopts_positional_set_required(a, true);
        clopts_positional_set_variadic(b, true);
        clopts_positional_set_variadic(a, false);
        clopts_positional_set_required(b, false);
        clopts_positional_set_required(a, false);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        NAOS_CHECK_FALSE(clopts_positional_is_required(a));
        NAOS_CHECK_TRUE(clopts_positional_is_variadic(b));
        owner_destroy(owner);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
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

        a = owner_add_positional(owner, "a");
        clopts_positional_set_required(a, true);
        b = owner_add_positional(owner, "b");
        clopts_positional_set_required(b, true);
        clopts_positional_set_variadic(b, true);
        clopts_positional_set_variadic(b, false);
        c = owner_add_positional(owner, "c");
        clopts_positional_set_required(c, true);
        clopts_positional_set_required(c, false);
        d = owner_add_positional(owner, "d");
        clopts_positional_set_variadic(d, true);

        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        NAOS_REQUIRE_UINT_EQ(owner_positional_count(owner), 4);
        NAOS_CHECK_TRUE(clopts_positional_is_required(a));
        NAOS_CHECK_TRUE(clopts_positional_is_required(b));
        NAOS_CHECK_FALSE(clopts_positional_is_variadic(b));
        NAOS_CHECK_FALSE(clopts_positional_is_required(c));
        NAOS_CHECK_TRUE(clopts_positional_is_variadic(d));
        NAOS_CHECK_PTR_EQ(clopts_scope_get_positional_at(owner_scope(owner), 3), d);
        owner_destroy(owner);

        /* a single required variadic one */
        owner = new_owner((owner_kind_t)k);
        a = owner_add_positional(owner, "files");
        clopts_positional_set_required(a, true);
        clopts_positional_set_variadic(a, true);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(owner_positional_count(owner), 1);
        owner_destroy(owner);
    }
}

/* ---- negatable false ---------------------------------------------------- */

NAOS_TEST(negatable_unset_is_not_checked)
{
    size_t k;

    for (k = 0; k < OWNER_KIND_COUNT; k++)
    {
        owner_t owner = new_owner((owner_kind_t)k);
        clopts_option_t *value;
        clopts_option_t *color;

        value = owner_add(owner, CLOPTS_OPTION_VALUE, "output", 'o');
        clopts_option_set_negatable(value, false);
        owner_add(owner, CLOPTS_OPTION_FLAG, "no-color", '\0');
        color = owner_add(owner, CLOPTS_OPTION_FLAG, "color", '\0');
        clopts_option_set_negatable(color, false);
        NAOS_CHECK_UINT_EQ(owner_option_count(owner), 3);
        EXPECT_ERROR(owner.program, CLOPTS_ERROR_NONE);
        owner_destroy(owner);
    }
}

/* ---- releases ----------------------------------------------------------- */

NAOS_TEST(program_destroy_accepts_null)
{
    clopts_program_destroy(NULL);
    NAOS_PASS();
}

/*
 * An add that fails after it made the new option or positional (DUPLICATE,
 * CONFLICT, ORDER) releases it again: no block is left alive, and the
 * program releases the rest.
 */
NAOS_TEST(failed_add_releases_what_it_made)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_positional_t *rest;
    long live;

    test_alloc_begin(0);

    program = new_program_named("tool");
    build = add_command(program, "build");
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    live = test_alloc.live;
    NAOS_CHECK_NULL(clopts_command_add_option(build, CLOPTS_OPTION_VALUE, "out", 'o'));
    EXPECT_ERROR(program, CLOPTS_ERROR_DUPLICATE);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);

    program = new_program_named("tool");
    global_add_negatable(program, "color", '\0');
    live = test_alloc.live;
    NAOS_CHECK_NULL(clopts_program_add_option(program, CLOPTS_OPTION_VALUE, "no-color", 'n'));
    EXPECT_ERROR(program, CLOPTS_ERROR_CONFLICT);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);

    program = new_program_named("tool");
    build = add_command(program, "build");
    rest = add_positional(build, "rest");
    clopts_positional_set_variadic(rest, true);
    live = test_alloc.live;
    NAOS_CHECK_NULL(clopts_command_add_positional(build, "src"));
    EXPECT_ERROR(program, CLOPTS_ERROR_ORDER);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);

    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- valid declarations -------------------------------------------------- */

/*
 * A declaration shaped like the fixture "tool": globals, two commands, a
 * negatable flag, env and default, a list delimiter, a count maximum,
 * required options and positionals with a variadic tail, the help option of
 * the program and a terminal --help in a command. Every call succeeds, and
 * the program parses.
 */
NAOS_TEST(tool_like_declaration_is_ok)
{
    static const char *const tokens[] = { "copy", "a", "b", NULL };
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *option;
    clopts_positional_t *positional;
    clopts_result_t *result;

    clopts_program_set_description(program, "A sample tool.");
    option = global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_count_max(option, 3);
    global_add_negatable(program, "color", '\0');
    option = global_add(program, CLOPTS_OPTION_VALUE, "config", 'c');
    clopts_option_set_env(option, "TOOL_CONFIG");
    NAOS_REQUIRE_NOT_NULL(clopts_program_add_help_option(program));

    build = add_command(program, "build");
    clopts_command_set_summary(build, "Build things.");
    option = add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_env(option, "TOOL_OUTPUT");
    clopts_option_set_default(option, "a.out");
    option = add(build, CLOPTS_OPTION_LIST, "keys", 'k');
    clopts_option_set_delimiter(option, ',');
    clopts_option_set_required(option, true);
    option = add(build, CLOPTS_OPTION_COUNT, "jobs", 'j');
    clopts_option_set_count_max(option, 8);
    add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    option = add(build, CLOPTS_OPTION_FLAG, "help", 'h');
    clopts_option_set_terminal(option, true);
    positional = add_positional(build, "src");
    clopts_positional_set_required(positional, true);
    positional = add_positional(build, "rest");
    clopts_positional_set_variadic(positional, true);

    copy = add_command(program, "copy");
    add(copy, CLOPTS_OPTION_FLAG, "release", 'r');
    add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    positional = add_positional(copy, "src");
    clopts_positional_set_required(positional, true);
    positional = add_positional(copy, "dst");
    clopts_positional_set_required(positional, true);

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
    clopts_program_t *program = new_program_named("tool");

    f.verbose = global_add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    f.color = global_add_negatable(program, "color", '\0');
    f.quiet = global_add(program, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.no_cache = global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    f.build = add_command(program, "build");
    f.copy = add_command(program, "copy");
    f.build_output = add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_cache = add_negatable(f.build, "cache", 'c');
    f.build_release = add_negatable(f.build, NULL, 'r');
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
    bool negated = true;

    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "output", 6, &negated), f.build_output);
    NAOS_CHECK_FALSE(negated);
    /* exactly name_len bytes */
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "outputs", 6, &negated), f.build_output);
    /* an exact name comes before the negation of another option */
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "no-color", 8, &negated), f.build_no_color);
    NAOS_CHECK_FALSE(negated);
    /* --no-<name> of a negatable FLAG */
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(build, "no-cache", 8, &negated), f.build_cache);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_PTR_EQ(clopts_scope_find_long(global, "no-color", 8, &negated), f.color);
    NAOS_CHECK_TRUE(negated);
    /* no negation of an option that is not a negatable FLAG */
    NAOS_CHECK_NULL(clopts_scope_find_long(global, "no-quiet", 8, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(global, "no-verbose", 10, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "no-", 3, &negated));
    /* exact and case-sensitive, no prefix */
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "Output", 6, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "out", 3, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "", 0, &negated));
    /* a name of the program is not found in a command, nor the other way round */
    NAOS_CHECK_NULL(clopts_scope_find_long(build, "verbose", 7, &negated));
    NAOS_CHECK_NULL(clopts_scope_find_long(global, "output", 6, &negated));

    NAOS_CHECK_PTR_EQ(clopts_scope_find_short(build, 'r'), f.build_release);
    NAOS_CHECK_PTR_EQ(clopts_scope_find_short(global, 'q'), f.quiet);
    NAOS_CHECK_NULL(clopts_scope_find_short(build, 'O'));
    NAOS_CHECK_NULL(clopts_scope_find_short(build, 'v'));
    NAOS_CHECK_NULL(clopts_scope_find_short(build, '\0'));
    clopts_program_destroy(f.program);
}

NAOS_TEST(lookup_looks_in_the_command_first_then_the_program)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_lookup_t found;

    found = clopts_lookup_long(f.program, f.build, "output", 6);
    NAOS_CHECK_PTR_EQ(found.option, f.build_output);
    NAOS_CHECK_NULL(clopts_lookup_long(f.program, NULL, "output", 6).option);

    found = clopts_lookup_long(f.program, f.build, "verbose", 7);
    NAOS_CHECK_PTR_EQ(found.option, f.verbose);

    /* the command's exact name before the program's negation */
    found = clopts_lookup_long(f.program, f.build, "no-color", 8);
    NAOS_CHECK_PTR_EQ(found.option, f.build_no_color);
    NAOS_CHECK_FALSE(found.negated);
    found = clopts_lookup_long(f.program, f.copy, "no-color", 8);
    NAOS_CHECK_PTR_EQ(found.option, f.color);
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
    clopts_program_t *program = new_program_named("tool");

    f.output = global_add(program, CLOPTS_OPTION_VALUE, "output", 'o');
    f.color = global_add_negatable(program, "color", '\0');
    f.no_cache = global_add(program, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    f.quiet = global_add(program, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.build = add_command(program, "build");
    f.build_output = add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_no_color = add(f.build, CLOPTS_OPTION_FLAG, "no-color", '\0');
    f.build_cache = add_negatable(f.build, "cache", 'c');
    f.copy = add_command(program, "copy");
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

/* --help, -h and --version declared as ordinary options parse as ordinary options. */
NAOS_TEST(help_and_version_options_parse_as_declared)
{
    static const char *const tokens[] = { "--help", "-h", "--version", "x", "build", "--help", NULL };
    static const char *const help_command[] = { "help", "-h", NULL };
    clopts_program_t *program = new_program_named("tool");
    clopts_command_t *build = add_command(program, "build");
    clopts_command_t *help = add_command(program, "help");
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
    NAOS_ADD_TEST(program_read_functions_follow_the_order_of_addition),
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
    NAOS_ADD_TEST(set_negatable_conflicts_with_existing_no_option),
    NAOS_ADD_TEST(option_no_prefix_without_negatable_flag_is_allowed),
    NAOS_ADD_TEST(negatable_flag_without_long_name_is_allowed),
    NAOS_ADD_TEST(option_add_checks_follow_order),
    NAOS_ADD_TEST(positional_after_variadic),
    NAOS_ADD_TEST(required_positional_after_optional),
    NAOS_ADD_TEST(positional_setters_keep_the_order),
    NAOS_ADD_TEST(positional_order_allowed),
    NAOS_ADD_TEST(negatable_unset_is_not_checked),
    NAOS_ADD_TEST(program_destroy_accepts_null),
    NAOS_ADD_TEST(failed_add_releases_what_it_made),
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
