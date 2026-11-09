/*
 * Suite decl: the immediate checks of the builder and the lookups shared
 * with the scanner.
 *
 * Every builder function returns its clopts_decl_error_t right away, and a
 * failing call changes nothing. Each rule therefore has a test that the
 * offending call returns the expected error and leaves everything as it was
 * (the out pointer NULL, the vecs of the command and the program the same
 * length, the attribute unchanged, an option or positional still unattached
 * and owned by the caller, who releases it), and a test of the allowed
 * counterparts. Checks of an option or positional alone are made by its
 * constructor and setters; checks against its siblings by the attach
 * functions, clopts_program_add_command(), clopts_command_set_handler() and
 * clopts_program_set_version(). The order of the checks within one call is
 * that of parser_build.c.
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

/*
 * Objects whose address an out pointer holds before a call, so that a test
 * can see the call set it to NULL. They are never read or written.
 */
static clopts_program_t sentinel_program;
static clopts_command_t sentinel_command;
static clopts_option_t sentinel_option;
static clopts_positional_t sentinel_positional;

static int dummy_handler(const clopts_result_t *result, void *userdata)
{
    (void)result;
    (void)userdata;
    return 0;
}

static int other_handler(const clopts_result_t *result, void *userdata)
{
    (void)result;
    (void)userdata;
    return 1;
}

/* Returns option index of scope. */
static clopts_option_t *option_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_option_t **)clopts_core_vec_at(&scope->options, index);
}

/* Returns positional index of scope. */
static clopts_positional_t *positional_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, index);
}

/* A new program named "tool"; the test stops when it cannot be made. */
static clopts_program_t *new_tool(void)
{
    clopts_program_t *program = test_decl_program("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/* A new command of program; the test stops when it cannot be added. */
static clopts_command_t *command(clopts_program_t *program, const char *name)
{
    clopts_command_t *result = test_decl_command(program, name);

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
 * are unchanged and that option is still unattached, then releases it (the
 * caller still owns it).
 */
static void expect_attach_error(clopts_command_t *scope, clopts_option_t *option, clopts_decl_error_t error)
{
    size_t len;

    NAOS_REQUIRE_NOT_NULL(scope);
    NAOS_REQUIRE_NOT_NULL(option);
    len = scope->options.len;
    NAOS_CHECK_INT_EQ(clopts_command_add_option(scope, option), error);
    NAOS_CHECK_UINT_EQ(scope->options.len, len);
    NAOS_CHECK_FALSE(option->attached);
    if (!option->attached)
    {
        clopts_option_free(option);
    }
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
    NAOS_CHECK_FALSE(positional->attached);
    if (!positional->attached)
    {
        clopts_positional_free(positional);
    }
}

/* Calls clopts_option_new() expecting error and checks that *out is set to NULL. */
static void expect_option_new_error(clopts_option_kind_t kind, const char *long_name, char short_name,
                                    clopts_decl_error_t error)
{
    clopts_option_t *out = &sentinel_option;

    NAOS_CHECK_INT_EQ(clopts_option_new(kind, long_name, short_name, &out), error);
    NAOS_CHECK_NULL(out);
    if ((out != NULL) && (out != &sentinel_option))
    {
        clopts_option_free(out);
    }
}

/*
 * Calls clopts_program_add_command() expecting error and checks that *out is
 * set to NULL and that no command was added.
 */
static void expect_command_error(clopts_program_t *program, const char *name, clopts_decl_error_t error)
{
    clopts_command_t *out = &sentinel_command;
    size_t len;

    NAOS_REQUIRE_NOT_NULL(program);
    len = program->commands.len;
    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, name, &out), error);
    NAOS_CHECK_NULL(out);
    NAOS_CHECK_UINT_EQ(program->commands.len, len);
}

/* ---- name rules -------------------------------------------------------- */

NAOS_TEST(valid_command_name_rules)
{
    NAOS_CHECK_TRUE(clopts_parser_valid_command_name("build"));
    NAOS_CHECK_TRUE(clopts_parser_valid_command_name("a-b"));
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

/* ---- clopts_program_new -------------------------------------------------- */

NAOS_TEST(program_name_invalid)
{
    static const char *const names[] = { NULL, "", "-tool", "my tool", "my\ttool" };
    clopts_program_t *program;
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", (names[i] != NULL) ? names[i] : "(null)");
        program = &sentinel_program;
        NAOS_CHECK_INT_EQ(clopts_program_new(names[i], &program), CLOPTS_DECL_INVALID_NAME);
        NAOS_CHECK_NULL(program);
    }

    /* a program may be named help: only a command is reserved */
    program = NULL;
    EXPECT_OK(clopts_program_new("help", &program));
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_free(program);

    program = NULL;
    EXPECT_OK(clopts_program_new("my-tool", &program));
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_CHECK_STR_EQ(program->root.name, "my-tool");
    clopts_program_free(program);
}

/* ---- clopts_program_set_version ------------------------------------------ */

NAOS_TEST(version_with_version_option_in_root)
{
    clopts_program_t *program = new_tool();

    add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "version", 'V');
    NAOS_CHECK_INT_EQ(clopts_program_set_version(program, "1.0"), CLOPTS_DECL_RESERVED_NAME);
    NAOS_CHECK_NULL(program->version);
    clopts_program_free(program);
}

NAOS_TEST(version_with_version_option_in_command)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_command_t *copy;

    build = command(program, "build");
    copy = command(program, "copy");
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    add(copy, CLOPTS_OPTION_VALUE, "version", '\0');
    NAOS_CHECK_INT_EQ(clopts_program_set_version(program, "1.0"), CLOPTS_DECL_RESERVED_NAME);
    NAOS_CHECK_NULL(program->version);
    clopts_program_free(program);
}

NAOS_TEST(version_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();

    add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "version", '\0');
    EXPECT_OK(clopts_program_set_version(program, NULL));
    NAOS_CHECK_NULL(program->version);
    clopts_program_free(program);
}

NAOS_TEST(version_option_allowed_without_version)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = command(program, "build");
    add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "version", 'V');
    add(build, CLOPTS_OPTION_VALUE, "version-file", '\0');
    NAOS_CHECK_UINT_EQ(program->root.options.len, 1);
    NAOS_CHECK_UINT_EQ(build->options.len, 1);
    clopts_program_free(program);
}

NAOS_TEST(version_option_conflict_in_both_orders)
{
    clopts_program_t *program;
    clopts_command_t *build;

    /* option first: clopts_program_set_version() fails */
    program = new_tool();
    add(clopts_program_root(program), CLOPTS_OPTION_VALUE, "version", '\0');
    NAOS_CHECK_INT_EQ(clopts_program_set_version(program, "1.2.3"), CLOPTS_DECL_RESERVED_NAME);
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_UINT_EQ(program->root.options.len, 1);
    clopts_program_free(program);

    /* version first: the attach fails, in the root and in a command */
    program = new_tool();
    EXPECT_OK(clopts_program_set_version(program, "1.2.3"));
    build = command(program, "build");
    expect_attach_error(clopts_program_root(program), make(CLOPTS_OPTION_VALUE, "version", '\0'),
                        CLOPTS_DECL_RESERVED_NAME);
    expect_attach_error(build, make(CLOPTS_OPTION_FLAG, "version", '\0'), CLOPTS_DECL_RESERVED_NAME);
    NAOS_CHECK_STR_EQ(program->version, "1.2.3");

    /* resetting the version releases the name */
    EXPECT_OK(clopts_program_set_version(program, NULL));
    add(build, CLOPTS_OPTION_FLAG, "version", '\0');

    /* a version replaced by another one is still checked */
    NAOS_CHECK_INT_EQ(clopts_program_set_version(program, "2.0"), CLOPTS_DECL_RESERVED_NAME);
    NAOS_CHECK_NULL(program->version);
    clopts_program_free(program);
}

/* ---- clopts_program_add_command ------------------------------------------ */

NAOS_TEST(command_name_invalid)
{
    static const char *const names[] = { NULL, "", "-b", "b c", "b\nc" };
    clopts_program_t *program = new_tool();
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", (names[i] != NULL) ? names[i] : "(null)");
        expect_command_error(program, names[i], CLOPTS_DECL_INVALID_NAME);
    }
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    clopts_program_free(program);
}

NAOS_TEST(command_help_is_reserved)
{
    clopts_program_t *program = new_tool();

    expect_command_error(program, "help", CLOPTS_DECL_RESERVED_NAME);
    command(program, "build");
    expect_command_error(program, "help", CLOPTS_DECL_RESERVED_NAME);
    NAOS_CHECK_UINT_EQ(program->commands.len, 1);
    clopts_program_free(program);
}

NAOS_TEST(command_duplicate)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = command(program, "build");
    command(program, "copy");
    expect_command_error(program, "build", CLOPTS_DECL_DUPLICATE_COMMAND);
    NAOS_CHECK_UINT_EQ(program->commands.len, 2);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(program, "build"), build);
    clopts_program_free(program);
}

NAOS_TEST(command_names_are_case_sensitive)
{
    clopts_program_t *program = new_tool();

    command(program, "build");
    command(program, "Build");
    command(program, "buil");
    command(program, "Help");
    command(program, "helper");
    NAOS_CHECK_UINT_EQ(program->commands.len, 5);
    clopts_program_free(program);
}

NAOS_TEST(command_after_root_positional_is_mixed)
{
    clopts_program_t *program = new_tool();

    add_positional(clopts_program_root(program), "file");
    expect_command_error(program, "build", CLOPTS_DECL_MIXED_LAYOUT);
    clopts_program_free(program);
}

NAOS_TEST(command_after_root_handler_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);

    EXPECT_OK(clopts_command_set_handler(root, dummy_handler, NULL));
    expect_command_error(program, "build", CLOPTS_DECL_MIXED_LAYOUT);

    /* resetting the handler allows commands again */
    EXPECT_OK(clopts_command_set_handler(root, NULL, NULL));
    command(program, "build");
    clopts_program_free(program);
}

NAOS_TEST(command_checks_follow_order)
{
    clopts_program_t *program = new_tool();

    /* INVALID_NAME and RESERVED_NAME before MIXED_LAYOUT */
    EXPECT_OK(clopts_command_set_handler(clopts_program_root(program), dummy_handler, NULL));
    expect_command_error(program, "", CLOPTS_DECL_INVALID_NAME);
    expect_command_error(program, "help", CLOPTS_DECL_RESERVED_NAME);
    expect_command_error(program, "build", CLOPTS_DECL_MIXED_LAYOUT);
    clopts_program_free(program);
}

/* ---- clopts_command_set_handler(root) ------------------------------------ */

NAOS_TEST(root_handler_with_commands_is_mixed)
{
    int first = 1;
    int second = 2;
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);

    command(program, "build");
    NAOS_CHECK_INT_EQ(clopts_command_set_handler(root, dummy_handler, &second), CLOPTS_DECL_MIXED_LAYOUT);
    NAOS_CHECK_TRUE(root->handler == NULL);
    NAOS_CHECK_NULL(root->userdata);

    /* resetting is not checked */
    EXPECT_OK(clopts_command_set_handler(root, NULL, &first));
    NAOS_CHECK_INT_EQ(clopts_command_set_handler(root, other_handler, &second), CLOPTS_DECL_MIXED_LAYOUT);
    NAOS_CHECK_TRUE(root->handler == NULL);
    NAOS_CHECK_PTR_EQ(root->userdata, &first);
    clopts_program_free(program);
}

NAOS_TEST(handlers_allowed_on_commands_and_root_unset)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = command(program, "build");
    EXPECT_OK(clopts_command_set_handler(build, dummy_handler, NULL));
    EXPECT_OK(clopts_command_set_handler(clopts_program_root(program), NULL, NULL));
    NAOS_CHECK_TRUE(build->handler == dummy_handler);
    clopts_program_free(program);
}

/* ---- clopts_option_new ---------------------------------------------------- */

NAOS_TEST(option_invalid_kind)
{
    expect_option_new_error((clopts_option_kind_t)4, "flag", 'f', CLOPTS_DECL_INVALID_KIND);
    expect_option_new_error((clopts_option_kind_t)-1, "flag", 'f', CLOPTS_DECL_INVALID_KIND);
    expect_option_new_error((clopts_option_kind_t)100, NULL, 'f', CLOPTS_DECL_INVALID_KIND);

    /* the kind is checked first */
    expect_option_new_error((clopts_option_kind_t)4, NULL, '\0', CLOPTS_DECL_INVALID_KIND);
    expect_option_new_error((clopts_option_kind_t)4, "-x", '=', CLOPTS_DECL_INVALID_KIND);
}

NAOS_TEST(option_without_name)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE,
                                                  CLOPTS_OPTION_LIST };
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        expect_option_new_error(kinds[i], NULL, '\0', CLOPTS_DECL_NO_NAME);
    }
}

NAOS_TEST(option_long_name_invalid)
{
    static const char *const names[] = { "", "-o", "--o", "out=put", "out put", "out\vput" };
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("long=\"%s\"", names[i]);
        expect_option_new_error(CLOPTS_OPTION_VALUE, names[i], 'o', CLOPTS_DECL_INVALID_NAME);
        expect_option_new_error(CLOPTS_OPTION_FLAG, names[i], '\0', CLOPTS_DECL_INVALID_NAME);
    }
}

NAOS_TEST(option_short_name_invalid)
{
    static const char shorts[] = { '-', '=', ' ', '\t' };
    size_t i;

    for (i = 0; i < sizeof(shorts); i++)
    {
        NAOS_NOTE("short=%d", (int)shorts[i]);
        expect_option_new_error(CLOPTS_OPTION_FLAG, "flag", shorts[i], CLOPTS_DECL_INVALID_NAME);
        expect_option_new_error(CLOPTS_OPTION_COUNT, NULL, shorts[i], CLOPTS_DECL_INVALID_NAME);
    }
}

NAOS_TEST(option_new_valid_names)
{
    clopts_option_t *option = NULL;

    EXPECT_OK(clopts_option_new(CLOPTS_OPTION_LIST, "no-", '4', &option));
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_FALSE(option->attached);
    NAOS_CHECK_INT_EQ(option->kind, CLOPTS_PARSER_LIST);
    NAOS_CHECK_STR_EQ(option->long_name, "no-");
    NAOS_CHECK_INT_EQ(option->short_name, '4');
    clopts_option_free(option);

    /* names checked only at attach (reserved ones) are accepted here */
    option = NULL;
    EXPECT_OK(clopts_option_new(CLOPTS_OPTION_FLAG, "help", 'h', &option));
    NAOS_REQUIRE_NOT_NULL(option);
    clopts_option_free(option);
}

/* ---- clopts_command_add_option ------------------------------------------- */

NAOS_TEST(option_reserved_names)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;

    program = new_tool();
    root = clopts_program_root(program);
    build = command(program, "build");
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "help", '\0'), CLOPTS_DECL_RESERVED_NAME);
    expect_attach_error(build, make(CLOPTS_OPTION_FLAG, "help", 'x'), CLOPTS_DECL_RESERVED_NAME);
    expect_attach_error(root, make(CLOPTS_OPTION_VALUE, "host", 'h'), CLOPTS_DECL_RESERVED_NAME);
    expect_attach_error(build, make(CLOPTS_OPTION_COUNT, NULL, 'h'), CLOPTS_DECL_RESERVED_NAME);

    /* --version only while a version is set */
    add(build, CLOPTS_OPTION_FLAG, "version", '\0');
    clopts_program_free(program);

    program = new_tool();
    EXPECT_OK(clopts_program_set_version(program, "1.0"));
    build = command(program, "build");
    expect_attach_error(build, make(CLOPTS_OPTION_FLAG, "version", '\0'), CLOPTS_DECL_RESERVED_NAME);

    /* names that only resemble the reserved ones are allowed */
    root = clopts_program_root(program);
    add(root, CLOPTS_OPTION_FLAG, "Help", 'H');
    add(root, CLOPTS_OPTION_FLAG, "versions", 'V');
    add(root, CLOPTS_OPTION_FLAG, "helper", '\0');
    add(build, CLOPTS_OPTION_FLAG, "Version", 'v');
    NAOS_CHECK_UINT_EQ(root->options.len, 3);
    NAOS_CHECK_UINT_EQ(build->options.len, 1);
    clopts_program_free(program);
}

NAOS_TEST(option_duplicate_in_same_scope)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_command_t *build;
    clopts_option_t *force;

    force = add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    expect_attach_error(root, make(CLOPTS_OPTION_VALUE, "force", 'o'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(root, make(CLOPTS_OPTION_COUNT, NULL, 'f'), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), force);

    build = command(program, "build");
    add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    expect_attach_error(build, make(CLOPTS_OPTION_LIST, "release", '\0'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(build, make(CLOPTS_OPTION_VALUE, "root", 'r'), CLOPTS_DECL_DUPLICATE_OPTION);

    /* long and short names are separate namespaces */
    add(build, CLOPTS_OPTION_FLAG, "r", 'R');
    NAOS_CHECK_UINT_EQ(build->options.len, 2);
    clopts_program_free(program);
}

NAOS_TEST(option_duplicate_between_root_and_command)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;

    /* command option clashing with an earlier global */
    program = new_tool();
    root = clopts_program_root(program);
    add(root, CLOPTS_OPTION_FLAG, "verbose", 'v');
    build = command(program, "build");
    expect_attach_error(build, make(CLOPTS_OPTION_FLAG, "verbose", '\0'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(build, make(CLOPTS_OPTION_VALUE, "value", 'v'), CLOPTS_DECL_DUPLICATE_OPTION);
    clopts_program_free(program);

    /* global clashing with an earlier command option, in any command */
    program = new_tool();
    root = clopts_program_root(program);
    command(program, "build");
    build = command(program, "copy");
    add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    expect_attach_error(root, make(CLOPTS_OPTION_VALUE, "root", 'r'), CLOPTS_DECL_DUPLICATE_OPTION);
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "release", '\0'), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_UINT_EQ(root->options.len, 0);
    clopts_program_free(program);
}

NAOS_TEST(option_same_name_in_different_commands_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_command_t *copy;

    build = command(program, "build");
    copy = command(program, "copy");
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    add(build, CLOPTS_OPTION_FLAG, "Output", 'O');
    add(copy, CLOPTS_OPTION_FLAG, NULL, 'O');
    NAOS_CHECK_UINT_EQ(build->options.len, 2);
    NAOS_CHECK_UINT_EQ(copy->options.len, 2);

    /* but then the root cannot take the name */
    expect_attach_error(clopts_program_root(program), make(CLOPTS_OPTION_FLAG, "output", '\0'),
                        CLOPTS_DECL_DUPLICATE_OPTION);
    clopts_program_free(program);
}

/* no-x attached after a negatable FLAG x */
NAOS_TEST(option_negation_conflict_on_add)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *color;

    /* same scope, any kind */
    program = new_tool();
    root = clopts_program_root(program);
    color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    attach(root, color);
    expect_attach_error(root, make(CLOPTS_OPTION_VALUE, "no-color", '\0'), CLOPTS_DECL_NEGATION_CONFLICT);
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "no-color", 'n'), CLOPTS_DECL_NEGATION_CONFLICT);
    clopts_program_free(program);

    /* command option against a negatable global */
    program = new_tool();
    root = clopts_program_root(program);
    color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    attach(root, color);
    build = command(program, "build");
    expect_attach_error(build, make(CLOPTS_OPTION_FLAG, "no-color", 'n'), CLOPTS_DECL_NEGATION_CONFLICT);
    clopts_program_free(program);

    /* global against a negatable command flag that is not the first of its name */
    program = new_tool();
    root = clopts_program_root(program);
    build = command(program, "build");
    copy = command(program, "copy");
    add(build, CLOPTS_OPTION_FLAG, "color", '\0');
    color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    attach(copy, color);
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "no-color", '\0'), CLOPTS_DECL_NEGATION_CONFLICT);
    clopts_program_free(program);
}

/* a negatable FLAG x attached after no-x */
NAOS_TEST(negatable_with_existing_no_option)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *option;

    /* same scope */
    program = new_tool();
    root = clopts_program_root(program);
    add(root, CLOPTS_OPTION_VALUE, "no-color", '\0');
    option = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    expect_attach_error(root, option, CLOPTS_DECL_NEGATION_CONFLICT);
    clopts_program_free(program);

    /* command flag against a global no-x */
    program = new_tool();
    root = clopts_program_root(program);
    add(root, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    build = command(program, "build");
    option = make(CLOPTS_OPTION_FLAG, "cache", 'c');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    expect_attach_error(build, option, CLOPTS_DECL_NEGATION_CONFLICT);
    clopts_program_free(program);

    /* global flag against a command no-x */
    program = new_tool();
    root = clopts_program_root(program);
    command(program, "build");
    build = command(program, "copy");
    add(build, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    option = make(CLOPTS_OPTION_FLAG, "cache", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    expect_attach_error(root, option, CLOPTS_DECL_NEGATION_CONFLICT);

    /* without negatable the same option is allowed */
    option = make(CLOPTS_OPTION_FLAG, "cache", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    EXPECT_OK(clopts_option_set_negatable(option, false));
    attach(root, option);
    clopts_program_free(program);
}

NAOS_TEST(option_no_prefix_without_negatable_flag_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *cache;

    build = command(program, "build");
    copy = command(program, "copy");
    add(root, CLOPTS_OPTION_FLAG, "color", '\0');
    add(root, CLOPTS_OPTION_FLAG, "no-color", '\0');

    /* a negatable flag of one command and no-x of another */
    cache = make(CLOPTS_OPTION_FLAG, "cache", '\0');
    EXPECT_OK(clopts_option_set_negatable(cache, true));
    attach(build, cache);
    add(copy, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    add(build, CLOPTS_OPTION_FLAG, "no-", '\0');
    NAOS_CHECK_UINT_EQ(root->options.len, 2);
    NAOS_CHECK_UINT_EQ(build->options.len, 2);
    NAOS_CHECK_UINT_EQ(copy->options.len, 1);
    clopts_program_free(program);
}

NAOS_TEST(negatable_flag_without_long_name_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_option_t *option;

    add(root, CLOPTS_OPTION_FLAG, "no-", '\0');
    option = make(CLOPTS_OPTION_FLAG, NULL, 'c');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    option = attach(root, option);
    NAOS_CHECK_TRUE(option->negatable);
    clopts_program_free(program);
}

NAOS_TEST(option_attach_checks_follow_order)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_option_t *option;

    /* ALREADY_ATTACHED before the name checks */
    option = add(root, CLOPTS_OPTION_FLAG, "hidden", '\0');
    NAOS_CHECK_INT_EQ(clopts_command_add_option(root, option), CLOPTS_DECL_ALREADY_ATTACHED);

    /* RESERVED_NAME before DUPLICATE_OPTION */
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "hidden", 'h'), CLOPTS_DECL_RESERVED_NAME);

    /* DUPLICATE_OPTION before NEGATION_CONFLICT, in both directions */
    option = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    attach(root, option);
    add(root, CLOPTS_OPTION_FLAG, "quiet", 'q');
    add(root, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    expect_attach_error(root, make(CLOPTS_OPTION_FLAG, "no-color", 'q'), CLOPTS_DECL_DUPLICATE_OPTION);
    option = make(CLOPTS_OPTION_FLAG, "cache", 'q');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    expect_attach_error(root, option, CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_UINT_EQ(root->options.len, 4);
    clopts_program_free(program);
}

/* ---- clopts_command_add_positional ---------------------------------------- */

NAOS_TEST(positional_name_invalid)
{
    clopts_positional_t *positional;

    positional = &sentinel_positional;
    NAOS_CHECK_INT_EQ(clopts_positional_new(NULL, &positional), CLOPTS_DECL_INVALID_NAME);
    NAOS_CHECK_NULL(positional);

    positional = &sentinel_positional;
    NAOS_CHECK_INT_EQ(clopts_positional_new("", &positional), CLOPTS_DECL_INVALID_NAME);
    NAOS_CHECK_NULL(positional);

    positional = NULL;
    EXPECT_OK(clopts_positional_new("FILE", &positional));
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_FALSE(positional->attached);
    NAOS_CHECK_STR_EQ(positional->name, "FILE");
    clopts_positional_free(positional);
}

NAOS_TEST(root_positional_with_commands_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = command(program, "build");
    expect_attach_positional_error(clopts_program_root(program), make_positional("file"), CLOPTS_DECL_MIXED_LAYOUT);
    add_positional(build, "file");
    clopts_program_free(program);
}

NAOS_TEST(positional_after_variadic)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_positional_t *files;
    clopts_positional_t *extra;

    build = command(program, "build");
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
    clopts_program_free(program);
}

NAOS_TEST(required_positional_after_optional)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *copy;
    clopts_positional_t *dst;

    copy = command(program, "copy");
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
    clopts_program_free(program);
}

NAOS_TEST(positional_order_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *a;
    clopts_positional_t *b;
    clopts_positional_t *c;
    clopts_positional_t *d;

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
    clopts_program_free(program);

    /* a single required variadic one */
    program = new_tool();
    root = clopts_program_root(program);
    a = make_positional("files");
    EXPECT_OK(clopts_positional_set_required(a, true));
    EXPECT_OK(clopts_positional_set_variadic(a, true));
    attach_positional(root, a);
    clopts_program_free(program);
}

/* The setters of an unattached positional check nothing: the order is checked at attach. */
NAOS_TEST(positional_setters_before_attach_never_fail)
{
    clopts_positional_t *positional = make_positional("file");

    EXPECT_OK(clopts_positional_set_variadic(positional, true));
    EXPECT_OK(clopts_positional_set_required(positional, true));
    EXPECT_OK(clopts_positional_set_variadic(positional, false));
    EXPECT_OK(clopts_positional_set_required(positional, false));
    EXPECT_OK(clopts_positional_set_help(positional, "File."));
    EXPECT_OK(clopts_positional_set_help(positional, NULL));
    NAOS_CHECK_FALSE(positional->required);
    NAOS_CHECK_FALSE(positional->variadic);
    NAOS_CHECK_NULL(positional->help);
    clopts_positional_free(positional);
}

/* ---- clopts_option_set_env ------------------------------------------------- */

NAOS_TEST(env_on_flag_or_count)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, "TOOL_FORCE"), CLOPTS_DECL_ENV_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->env);
    clopts_option_free(option);

    /* the kind is checked before the empty name */
    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, ""), CLOPTS_DECL_ENV_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->env);
    clopts_option_free(option);
}

NAOS_TEST(env_empty_name)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_LIST, "include", 'I');
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, ""), CLOPTS_DECL_INVALID_NAME);
    NAOS_CHECK_NULL(option->env);

    /* a failing call keeps the previous value */
    EXPECT_OK(clopts_option_set_env(option, "TOOL_INCLUDE"));
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, ""), CLOPTS_DECL_INVALID_NAME);
    NAOS_CHECK_STR_EQ(option->env, "TOOL_INCLUDE");
    clopts_option_free(option);
}

NAOS_TEST(env_unset_is_not_checked)
{
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = make(CLOPTS_OPTION_FLAG, "force", 'f');
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_env(flag, NULL));
    EXPECT_OK(clopts_option_set_env(value, "TOOL_OUTPUT"));
    NAOS_CHECK_STR_EQ(value->env, "TOOL_OUTPUT");
    EXPECT_OK(clopts_option_set_env(value, NULL));
    NAOS_CHECK_NULL(value->env);
    clopts_option_free(flag);
    clopts_option_free(value);
}

/* ---- clopts_option_set_default ------------------------------------------- */

NAOS_TEST(default_on_flag_or_count)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_INT_EQ(clopts_option_set_default(option, "1"), CLOPTS_DECL_DEFAULT_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->default_value);
    clopts_option_free(option);

    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    NAOS_CHECK_INT_EQ(clopts_option_set_default(option, "2"), CLOPTS_DECL_DEFAULT_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->default_value);
    clopts_option_free(option);
}

NAOS_TEST(default_on_required)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_required(option, true));
    NAOS_CHECK_INT_EQ(clopts_option_set_default(option, "a.out"), CLOPTS_DECL_REQUIRED_WITH_DEFAULT);
    NAOS_CHECK_NULL(option->default_value);
    NAOS_CHECK_TRUE(option->required);

    /* allowed once required is reset */
    EXPECT_OK(clopts_option_set_required(option, false));
    EXPECT_OK(clopts_option_set_default(option, "a.out"));
    NAOS_CHECK_STR_EQ(option->default_value, "a.out");
    clopts_option_free(option);
}

NAOS_TEST(default_unset_is_not_checked)
{
    clopts_option_t *flag;
    clopts_option_t *list;

    flag = make(CLOPTS_OPTION_FLAG, "force", 'f');
    list = make(CLOPTS_OPTION_LIST, "include", 'I');
    EXPECT_OK(clopts_option_set_default(flag, NULL));
    EXPECT_OK(clopts_option_set_required(list, true));
    EXPECT_OK(clopts_option_set_default(list, NULL));
    NAOS_CHECK_TRUE(list->required);
    clopts_option_free(flag);
    clopts_option_free(list);
}

/* ---- clopts_option_set_required(o, true) --------------------------------- */

NAOS_TEST(required_on_flag_or_count)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_INT_EQ(clopts_option_set_required(option, true), CLOPTS_DECL_REQUIRED_NOT_ALLOWED);
    NAOS_CHECK_FALSE(option->required);
    clopts_option_free(option);

    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    NAOS_CHECK_INT_EQ(clopts_option_set_required(option, true), CLOPTS_DECL_REQUIRED_NOT_ALLOWED);
    NAOS_CHECK_FALSE(option->required);
    clopts_option_free(option);
}

NAOS_TEST(required_on_option_with_default)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_LIST, "include", 'I');
    EXPECT_OK(clopts_option_set_default(option, "a,b"));
    NAOS_CHECK_INT_EQ(clopts_option_set_required(option, true), CLOPTS_DECL_REQUIRED_WITH_DEFAULT);
    NAOS_CHECK_FALSE(option->required);
    NAOS_CHECK_STR_EQ(option->default_value, "a,b");

    /* allowed once the default is reset */
    EXPECT_OK(clopts_option_set_default(option, NULL));
    EXPECT_OK(clopts_option_set_required(option, true));
    NAOS_CHECK_TRUE(option->required);
    clopts_option_free(option);
}

NAOS_TEST(required_unset_is_not_checked)
{
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = make(CLOPTS_OPTION_FLAG, "force", 'f');
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_required(flag, false));
    EXPECT_OK(clopts_option_set_default(value, "a.out"));
    EXPECT_OK(clopts_option_set_required(value, false));
    clopts_option_free(flag);
    clopts_option_free(value);
}

/* ---- clopts_option_set_negatable(o, true) -------------------------------- */

NAOS_TEST(negatable_on_non_flag)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = make(kinds[i], "color", '\0');
        NAOS_CHECK_INT_EQ(clopts_option_set_negatable(option, true), CLOPTS_DECL_NOT_NEGATABLE);
        NAOS_CHECK_FALSE(option->negatable);
        clopts_option_free(option);
    }
}

NAOS_TEST(negatable_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
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
    clopts_program_free(program);
}

/* ---- delimiter and count maximum ------------------------------------------ */

NAOS_TEST(delimiter_on_non_list)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE };
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = make(kinds[i], "output", 'o');
        NAOS_CHECK_INT_EQ(clopts_option_set_delimiter(option, ','), CLOPTS_DECL_ATTRIBUTE_KIND);
        NAOS_CHECK_INT_EQ(option->delimiter, '\0');
        clopts_option_free(option);
    }
}

NAOS_TEST(count_max_on_non_count)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = make(kinds[i], "force", 'f');
        NAOS_CHECK_INT_EQ(clopts_option_set_count_max(option, 3), CLOPTS_DECL_ATTRIBUTE_KIND);
        NAOS_CHECK_UINT_EQ(option->count_max, 0);
        clopts_option_free(option);
    }
}

NAOS_TEST(delimiter_and_count_max_unset_are_not_checked)
{
    clopts_option_t *value;

    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_delimiter(value, '\0'));
    EXPECT_OK(clopts_option_set_count_max(value, 0));
    clopts_option_free(value);
}

/* ---- already attached --------------------------------------------------- */

NAOS_TEST(option_attached_twice)
{
    clopts_program_t *program = new_tool();
    clopts_program_t *other = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_command_t *build;
    clopts_option_t *option;

    build = command(program, "build");
    option = add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_TRUE(option->attached);

    /* the same command, another command, the root, another program */
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_command_add_option(command(program, "copy"), option), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_command_add_option(root, option), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_command_add_option(clopts_program_root(other), option), CLOPTS_DECL_ALREADY_ATTACHED);

    NAOS_CHECK_UINT_EQ(build->options.len, 1);
    NAOS_CHECK_UINT_EQ(root->options.len, 0);
    NAOS_CHECK_UINT_EQ(other->root.options.len, 0);
    NAOS_CHECK_TRUE(option->attached);
    clopts_program_free(other);
    clopts_program_free(program);
}

NAOS_TEST(positional_attached_twice)
{
    clopts_program_t *program = new_tool();
    clopts_program_t *other = new_tool();
    clopts_command_t *build;
    clopts_positional_t *positional;

    build = command(program, "build");
    positional = add_positional(build, "file");
    NAOS_CHECK_TRUE(positional->attached);

    NAOS_CHECK_INT_EQ(clopts_command_add_positional(build, positional), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(command(program, "copy"), positional),
                      CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(clopts_program_root(other), positional),
                      CLOPTS_DECL_ALREADY_ATTACHED);

    /* ALREADY_ATTACHED before MIXED_LAYOUT */
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(clopts_program_root(program), positional),
                      CLOPTS_DECL_ALREADY_ATTACHED);

    NAOS_CHECK_UINT_EQ(build->positionals.len, 1);
    NAOS_CHECK_UINT_EQ(other->root.positionals.len, 0);
    clopts_program_free(other);
    clopts_program_free(program);
}

/* Every option setter on an attached option, with a value that would otherwise be accepted. */
NAOS_TEST(option_setters_after_attach)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_option_t *value;
    clopts_option_t *token;
    clopts_option_t *list;
    clopts_option_t *count;
    clopts_option_t *flag;

    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(value, "Output file."));
    EXPECT_OK(clopts_option_set_value_name(value, "FILE"));
    EXPECT_OK(clopts_option_set_env(value, "TOOL_OUTPUT"));
    EXPECT_OK(clopts_option_set_default(value, "a.out"));
    attach(root, value);
    token = make(CLOPTS_OPTION_VALUE, "token", '\0');
    EXPECT_OK(clopts_option_set_required(token, true));
    attach(root, token);
    list = make(CLOPTS_OPTION_LIST, "include", 'I');
    EXPECT_OK(clopts_option_set_delimiter(list, ','));
    attach(root, list);
    count = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    EXPECT_OK(clopts_option_set_count_max(count, 3));
    attach(root, count);
    flag = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(flag, true));
    attach(root, flag);

    /* setting and resetting */
    NAOS_CHECK_INT_EQ(clopts_option_set_help(value, "Other."), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_help(value, NULL), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_value_name(value, "PATH"), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_value_name(value, NULL), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_env(value, "OTHER"), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_env(value, NULL), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_default(value, "b.out"), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_default(value, NULL), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_required(token, false), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_required(token, true), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_delimiter(list, ':'), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_delimiter(list, '\0'), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_count_max(count, 8), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_count_max(count, 0), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_negatable(flag, false), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_negatable(flag, true), CLOPTS_DECL_ALREADY_ATTACHED);

    /* the attachment is checked before the kind */
    NAOS_CHECK_INT_EQ(clopts_option_set_negatable(value, true), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_delimiter(value, ','), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_count_max(flag, 2), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_env(flag, ""), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_default(flag, "1"), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_required(count, true), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_option_set_required(value, true), CLOPTS_DECL_ALREADY_ATTACHED);

    /* nothing changed */
    NAOS_CHECK_STR_EQ(value->help, "Output file.");
    NAOS_CHECK_STR_EQ(value->value_name, "FILE");
    NAOS_CHECK_STR_EQ(value->env, "TOOL_OUTPUT");
    NAOS_CHECK_STR_EQ(value->default_value, "a.out");
    NAOS_CHECK_FALSE(value->required);
    NAOS_CHECK_FALSE(value->negatable);
    NAOS_CHECK_INT_EQ(value->delimiter, '\0');
    NAOS_CHECK_TRUE(token->required);
    NAOS_CHECK_INT_EQ(list->delimiter, ',');
    NAOS_CHECK_UINT_EQ(count->count_max, 3);
    NAOS_CHECK_FALSE(count->required);
    NAOS_CHECK_TRUE(flag->negatable);
    NAOS_CHECK_UINT_EQ(flag->count_max, 0);
    NAOS_CHECK_NULL(flag->env);
    NAOS_CHECK_NULL(flag->default_value);
    clopts_program_free(program);
}

NAOS_TEST(positional_setters_after_attach)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *src;
    clopts_positional_t *rest;

    src = make_positional("src");
    EXPECT_OK(clopts_positional_set_help(src, "Source."));
    EXPECT_OK(clopts_positional_set_required(src, true));
    attach_positional(root, src);
    rest = make_positional("rest");
    EXPECT_OK(clopts_positional_set_variadic(rest, true));
    attach_positional(root, rest);

    NAOS_CHECK_INT_EQ(clopts_positional_set_help(src, "Other."), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_positional_set_help(src, NULL), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_positional_set_required(src, false), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_positional_set_required(rest, true), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_positional_set_variadic(rest, false), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_INT_EQ(clopts_positional_set_variadic(src, true), CLOPTS_DECL_ALREADY_ATTACHED);

    NAOS_CHECK_STR_EQ(src->help, "Source.");
    NAOS_CHECK_TRUE(src->required);
    NAOS_CHECK_FALSE(src->variadic);
    NAOS_CHECK_FALSE(rest->required);
    NAOS_CHECK_TRUE(rest->variadic);
    clopts_program_free(program);
}

/* ---- invalid arguments -------------------------------------------------- */

NAOS_TEST(null_handles_are_invalid_arguments)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_command_t *out;
    clopts_option_t *option;
    clopts_positional_t *positional;

    option = make(CLOPTS_OPTION_FLAG, "force", 'f');
    positional = make_positional("file");

    /* program */
    NAOS_CHECK_NULL(clopts_program_root(NULL));
    NAOS_CHECK_INT_EQ(clopts_program_set_version(NULL, "1.0"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_program_set_version(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    out = &sentinel_command;
    NAOS_CHECK_INT_EQ(clopts_program_add_command(NULL, "build", &out), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_NULL(out);

    /* command */
    NAOS_CHECK_INT_EQ(clopts_command_set_summary(NULL, "summary"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_set_summary(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_set_description(NULL, "description"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_set_description(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_set_handler(NULL, dummy_handler, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_set_handler(NULL, NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_add_option(NULL, option), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_add_option(root, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_add_option(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(NULL, positional), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(root, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);

    /* option */
    NAOS_CHECK_INT_EQ(clopts_option_set_help(NULL, "help"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_help(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_value_name(NULL, "NAME"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_value_name(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_env(NULL, "ENV"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_env(NULL, ""), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_env(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_default(NULL, "default"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_default(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_required(NULL, true), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_required(NULL, false), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_negatable(NULL, true), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_negatable(NULL, false), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_delimiter(NULL, ','), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_delimiter(NULL, '\0'), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_count_max(NULL, 3), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_set_count_max(NULL, 0), CLOPTS_DECL_INVALID_ARGUMENT);

    /* positional */
    NAOS_CHECK_INT_EQ(clopts_positional_set_help(NULL, "help"), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_positional_set_help(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_positional_set_required(NULL, true), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_positional_set_required(NULL, false), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_positional_set_variadic(NULL, true), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_positional_set_variadic(NULL, false), CLOPTS_DECL_INVALID_ARGUMENT);

    /* the releases accept NULL */
    clopts_program_free(NULL);
    clopts_option_free(NULL);
    clopts_positional_free(NULL);

    /* nothing changed: the handles are still the caller's */
    NAOS_CHECK_FALSE(option->attached);
    NAOS_CHECK_FALSE(positional->attached);
    NAOS_CHECK_UINT_EQ(root->options.len, 0);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 0);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    clopts_option_free(option);
    clopts_positional_free(positional);
    clopts_program_free(program);
}

NAOS_TEST(null_out_pointers_are_invalid_arguments)
{
    clopts_program_t *program;

    /* the out pointer is checked before the name and the kind */
    NAOS_CHECK_INT_EQ(clopts_program_new("tool", NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_program_new(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_new(CLOPTS_OPTION_FLAG, "force", 'f', NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_option_new((clopts_option_kind_t)9, NULL, '\0', NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_positional_new("file", NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_positional_new(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);

    program = new_tool();
    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, "build", NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, "", NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_program_add_command(NULL, "build", NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    clopts_program_free(program);
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
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *option;
    clopts_positional_t *positional;
    long live;

    test_alloc_begin(0);
    program = new_tool();
    root = clopts_program_root(program);
    build = command(program, "build");
    copy = command(program, "copy");
    add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    add(build, CLOPTS_OPTION_VALUE, "output", 'o');

    /* released by the caller */
    option = make(CLOPTS_OPTION_VALUE, "force", '\0');
    EXPECT_OK(clopts_option_set_help(option, "Force."));
    EXPECT_OK(clopts_option_set_value_name(option, "WHEN"));
    EXPECT_OK(clopts_option_set_env(option, "TOOL_FORCE"));
    EXPECT_OK(clopts_option_set_default(option, "always"));
    live = test_alloc.live;
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_FALSE(option->attached);
    NAOS_CHECK_STR_EQ(option->help, "Force.");
    EXPECT_OK(clopts_option_set_help(option, "Still mine."));
    clopts_option_free(option);
    NAOS_CHECK_INT_LT(test_alloc.live, live);

    /* attached elsewhere after the failure, then released with the program */
    option = make(CLOPTS_OPTION_VALUE, "out", 'o');
    EXPECT_OK(clopts_option_set_help(option, "Output."));
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_DUPLICATE_OPTION);
    EXPECT_OK(clopts_command_add_option(copy, option));
    NAOS_CHECK_TRUE(option->attached);
    NAOS_CHECK_PTR_EQ(option_at(copy, 0), option);

    /* the same for positionals */
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_help(positional, "Source."));
    live = test_alloc.live;
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(root, positional), CLOPTS_DECL_MIXED_LAYOUT);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_FALSE(positional->attached);
    EXPECT_OK(clopts_command_add_positional(copy, positional));
    NAOS_CHECK_PTR_EQ(positional_at(copy, 0), positional);

    positional = make_positional("dst");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(copy, positional), CLOPTS_DECL_POSITIONAL_ORDER);
    clopts_positional_free(positional);

    clopts_program_free(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- valid declarations -------------------------------------------------- */

/*
 * A declaration shaped like the fixture "tool": globals, two commands, a
 * negatable flag, env and default, a list delimiter, a count maximum,
 * required options and positionals with a variadic tail, and handlers. Every
 * call succeeds, and a program built only from successful calls parses.
 */
NAOS_TEST(tool_like_declaration_is_ok)
{
    char arg0[] = "tool";
    char arg1[] = "copy";
    char arg2[] = "a";
    char arg3[] = "b";
    char *argv[] = { arg0, arg1, arg2, arg3, NULL };
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *option;
    clopts_positional_t *positional;
    clopts_result_t *result;

    EXPECT_OK(clopts_program_set_version(program, "1.0.0"));
    EXPECT_OK(clopts_command_set_description(root, "A sample tool."));
    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    EXPECT_OK(clopts_option_set_count_max(option, 3));
    attach(root, option);
    option = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(option, true));
    attach(root, option);
    option = make(CLOPTS_OPTION_VALUE, "config", 'c');
    EXPECT_OK(clopts_option_set_env(option, "TOOL_CONFIG"));
    attach(root, option);

    build = command(program, "build");
    EXPECT_OK(clopts_command_set_summary(build, "Build things."));
    EXPECT_OK(clopts_command_set_handler(build, dummy_handler, NULL));
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
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    attach_positional(build, positional);
    positional = make_positional("rest");
    EXPECT_OK(clopts_positional_set_variadic(positional, true));
    attach_positional(build, positional);

    copy = command(program, "copy");
    EXPECT_OK(clopts_command_set_handler(copy, dummy_handler, NULL));
    add(copy, CLOPTS_OPTION_FLAG, "release", 'r');
    add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    attach_positional(copy, positional);
    positional = make_positional("dst");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    attach_positional(copy, positional);

    NAOS_CHECK_UINT_EQ(root->options.len, 3);
    NAOS_CHECK_UINT_EQ(build->options.len, 4);
    NAOS_CHECK_UINT_EQ(build->positionals.len, 2);
    NAOS_CHECK_UINT_EQ(copy->options.len, 2);
    NAOS_CHECK_UINT_EQ(copy->positionals.len, 2);

    result = clopts_parse(program, 4, argv);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), copy);
    clopts_result_free(result);
    clopts_program_free(program);
}

/* ---- lookups ------------------------------------------------------------- */

/* A program with globals verbose/v, color (negatable), output/o and commands build / copy. */
typedef struct lookup_fixture
{
    clopts_program_t *program;
    clopts_option_t *verbose;
    clopts_option_t *color;
    clopts_option_t *quiet;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *build_output;
    clopts_option_t *build_cache;
    clopts_option_t *build_release;
    clopts_option_t *copy_output;
} lookup_fixture_t;

static lookup_fixture_t make_lookup_fixture(void)
{
    lookup_fixture_t f;
    clopts_command_t *root;

    f.program = new_tool();
    root = clopts_program_root(f.program);
    f.verbose = add(root, CLOPTS_OPTION_COUNT, "verbose", 'v');
    f.color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(f.color, true));
    f.color = attach(root, f.color);
    f.quiet = add(root, CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.build = command(f.program, "build");
    f.copy = command(f.program, "copy");
    f.build_output = add(f.build, CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_cache = make(CLOPTS_OPTION_FLAG, "cache", 'c');
    EXPECT_OK(clopts_option_set_negatable(f.build_cache, true));
    f.build_cache = attach(f.build, f.build_cache);
    f.build_release = make(CLOPTS_OPTION_FLAG, NULL, 'r');
    EXPECT_OK(clopts_option_set_negatable(f.build_release, true));
    f.build_release = attach(f.build, f.build_release);
    f.copy_output = add(f.copy, CLOPTS_OPTION_VALUE, "output", 'o');
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
    clopts_program_free(f.program);
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
    clopts_program_free(f.program);
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
    clopts_program_free(f.program);
}

NAOS_TEST(find_negation_only_for_negatable_flags_with_long_name)
{
    lookup_fixture_t f = make_lookup_fixture();
    clopts_parser_ref_t ref;

    ref = clopts_parser_find_negation(f.program, NULL, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 1);
    NAOS_CHECK_TRUE(ref.negated);

    ref = clopts_parser_find_negation(f.program, f.build, "no-cache", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.build_cache);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 1);
    NAOS_CHECK_TRUE(ref.negated);

    ref = clopts_parser_find_negation(f.program, f.build, "no-color=1", 8);
    NAOS_CHECK_PTR_EQ(ref.option, f.color);

    /* not negatable, not a FLAG, not selected, malformed */
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, NULL, "no-quiet", 8).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, f.build, "no-output", 9).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, f.copy, "no-cache", 8).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, NULL, "color", 5).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, NULL, "no-", 3).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, NULL, "No-color", 8).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, NULL, "no-Color", 8).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, NULL, "no-col", 6).option);
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, NULL, "nocolor", 7).option);
    /* a negatable FLAG without a long name has no negation */
    NAOS_CHECK_NULL(clopts_parser_find_negation(f.program, f.build, "no-r", 4).option);
    clopts_program_free(f.program);
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
    clopts_program_free(f.program);
}

NAOS_TEST(find_conflict_and_named_cover_the_lookup_range)
{
    lookup_fixture_t f = make_lookup_fixture();
    const clopts_command_t *root = clopts_program_root(f.program);

    /* a command scope sees itself and the root */
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(f.build, "output", '\0'), f.build_output);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(f.build, NULL, 'v'), f.verbose);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(f.copy, "verbose", '\0'), f.verbose);
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.copy, "cache", 'c'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.build, "Output", 'O'));
    NAOS_CHECK_NULL(clopts_parser_find_conflict(f.build, NULL, '\0'));

    /* the root sees every command, the first match in command order */
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(root, "cache", '\0'), f.build_cache);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(root, NULL, 'o'), f.build_output);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_conflict(root, "quiet", 'x'), f.quiet);

    /* find_named compares exactly name_len bytes */
    NAOS_CHECK_PTR_EQ(clopts_parser_find_named(f.build, "color", 5), f.color);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_named(root, "cachexyz", 5), f.build_cache);
    NAOS_CHECK_NULL(clopts_parser_find_named(f.copy, "cache", 5));
    NAOS_CHECK_NULL(clopts_parser_find_named(f.build, "colo", 4));
    NAOS_CHECK_NULL(clopts_parser_find_named(f.build, "", 0));
    clopts_program_free(f.program);
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(valid_command_name_rules),
    NAOS_ADD_TEST(valid_long_name_rules),
    NAOS_ADD_TEST(valid_short_name_rules),
    NAOS_ADD_TEST(program_name_invalid),
    NAOS_ADD_TEST(version_with_version_option_in_root),
    NAOS_ADD_TEST(version_with_version_option_in_command),
    NAOS_ADD_TEST(version_unset_is_not_checked),
    NAOS_ADD_TEST(version_option_allowed_without_version),
    NAOS_ADD_TEST(version_option_conflict_in_both_orders),
    NAOS_ADD_TEST(command_name_invalid),
    NAOS_ADD_TEST(command_help_is_reserved),
    NAOS_ADD_TEST(command_duplicate),
    NAOS_ADD_TEST(command_names_are_case_sensitive),
    NAOS_ADD_TEST(command_after_root_positional_is_mixed),
    NAOS_ADD_TEST(command_after_root_handler_is_mixed),
    NAOS_ADD_TEST(command_checks_follow_order),
    NAOS_ADD_TEST(root_handler_with_commands_is_mixed),
    NAOS_ADD_TEST(handlers_allowed_on_commands_and_root_unset),
    NAOS_ADD_TEST(option_invalid_kind),
    NAOS_ADD_TEST(option_without_name),
    NAOS_ADD_TEST(option_long_name_invalid),
    NAOS_ADD_TEST(option_short_name_invalid),
    NAOS_ADD_TEST(option_new_valid_names),
    NAOS_ADD_TEST(option_reserved_names),
    NAOS_ADD_TEST(option_duplicate_in_same_scope),
    NAOS_ADD_TEST(option_duplicate_between_root_and_command),
    NAOS_ADD_TEST(option_same_name_in_different_commands_is_allowed),
    NAOS_ADD_TEST(option_negation_conflict_on_add),
    NAOS_ADD_TEST(negatable_with_existing_no_option),
    NAOS_ADD_TEST(option_no_prefix_without_negatable_flag_is_allowed),
    NAOS_ADD_TEST(negatable_flag_without_long_name_is_allowed),
    NAOS_ADD_TEST(option_attach_checks_follow_order),
    NAOS_ADD_TEST(positional_name_invalid),
    NAOS_ADD_TEST(root_positional_with_commands_is_mixed),
    NAOS_ADD_TEST(positional_after_variadic),
    NAOS_ADD_TEST(required_positional_after_optional),
    NAOS_ADD_TEST(positional_order_allowed),
    NAOS_ADD_TEST(positional_setters_before_attach_never_fail),
    NAOS_ADD_TEST(env_on_flag_or_count),
    NAOS_ADD_TEST(env_empty_name),
    NAOS_ADD_TEST(env_unset_is_not_checked),
    NAOS_ADD_TEST(default_on_flag_or_count),
    NAOS_ADD_TEST(default_on_required),
    NAOS_ADD_TEST(default_unset_is_not_checked),
    NAOS_ADD_TEST(required_on_flag_or_count),
    NAOS_ADD_TEST(required_on_option_with_default),
    NAOS_ADD_TEST(required_unset_is_not_checked),
    NAOS_ADD_TEST(negatable_on_non_flag),
    NAOS_ADD_TEST(negatable_unset_is_not_checked),
    NAOS_ADD_TEST(delimiter_on_non_list),
    NAOS_ADD_TEST(count_max_on_non_count),
    NAOS_ADD_TEST(delimiter_and_count_max_unset_are_not_checked),
    NAOS_ADD_TEST(option_attached_twice),
    NAOS_ADD_TEST(positional_attached_twice),
    NAOS_ADD_TEST(option_setters_after_attach),
    NAOS_ADD_TEST(positional_setters_after_attach),
    NAOS_ADD_TEST(null_handles_are_invalid_arguments),
    NAOS_ADD_TEST(null_out_pointers_are_invalid_arguments),
    NAOS_ADD_TEST(failed_attach_leaves_ownership_with_the_caller),
    NAOS_ADD_TEST(tool_like_declaration_is_ok),
    NAOS_ADD_TEST(find_command_is_exact_and_case_sensitive),
    NAOS_ADD_TEST(find_long_searches_command_then_root),
    NAOS_ADD_TEST(find_long_is_exact_case_sensitive_and_uses_name_len),
    NAOS_ADD_TEST(find_negation_only_for_negatable_flags_with_long_name),
    NAOS_ADD_TEST(find_short_searches_command_then_root),
    NAOS_ADD_TEST(find_conflict_and_named_cover_the_lookup_range)
)
