/*
 * Suite decl: the immediate checks of the builder (at least one case per row
 * of the rule table of the design, in table order) and the lookups shared
 * with the scanner.
 *
 * Declaration errors are read through clopts_program_validate() and the
 * public error accessors; the root is reported as NULL. The lookups are
 * internal functions and are called directly.
 */

#include "parser/parser_internal.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

static int dummy_handler(const clopts_result_t *result, void *userdata)
{
    (void)result;
    (void)userdata;
    return 0;
}

/*
 * Validates program and checks that the result is the declaration error kind
 * reported at command / option / positional. Releases the result and program.
 */
static void expect_decl(clopts_program_t *program, clopts_decl_error_t kind, const clopts_command_t *command,
                        const clopts_option_t *option, const clopts_positional_t *positional)
{
    clopts_result_t *result;

    NAOS_REQUIRE_NOT_NULL(program);
    result = clopts_program_validate(program);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_DECL_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), kind);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), command);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), option);
    NAOS_CHECK_PTR_EQ(clopts_result_error_positional(result), positional);
    clopts_result_free(result);
    clopts_program_free(program);
}

/* Validates program and checks that it is OK. Releases the result and program. */
static void expect_ok(clopts_program_t *program)
{
    clopts_result_t *result;

    NAOS_REQUIRE_NOT_NULL(program);
    result = clopts_program_validate(program);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    clopts_result_free(result);
    clopts_program_free(program);
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

/* A new program named "tool"; the test stops when it cannot be allocated. */
static clopts_program_t *new_tool(void)
{
    clopts_program_t *program = test_decl_program("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
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

/* ---- row 1: clopts_program_new ------------------------------------------ */

NAOS_TEST(program_name_invalid)
{
    static const char *const names[] = { "", "-tool", "my tool", "my\ttool" };
    size_t i;

    expect_decl(test_decl_program(NULL), CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", names[i]);
        expect_decl(test_decl_program(names[i]), CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
    }
    expect_ok(test_decl_program("my-tool"));
}

/* ---- row 2: clopts_program_set_version ---------------------------------- */

NAOS_TEST(version_after_version_option_in_root)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *version;

    version = test_decl_option(CLOPTS_OPTION_FLAG, "version", 'V');
    version = test_decl_attach(clopts_program_root(program), version);
    clopts_program_set_version(program, "1.0");
    NAOS_CHECK_NULL(program->version);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, version, NULL);
}

NAOS_TEST(version_after_version_option_in_command)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *first;

    build = test_decl_command(program, "build");
    copy = test_decl_command(program, "copy");
    first = test_decl_option(CLOPTS_OPTION_VALUE, "version", '\0');
    first = test_decl_attach(build, first);
    test_decl_add(copy, CLOPTS_OPTION_VALUE, "version", '\0');
    clopts_program_set_version(program, "1.0");
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, build, first, NULL);
}

NAOS_TEST(version_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();

    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "version", '\0');
    clopts_program_set_version(program, NULL);
    expect_ok(program);
}

NAOS_TEST(version_option_allowed_without_version)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = test_decl_command(program, "build");
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "version", 'V');
    test_decl_add(build, CLOPTS_OPTION_VALUE, "version-file", '\0');
    expect_ok(program);
}

/* ---- rows 3-6: clopts_program_add_command ------------------------------- */

NAOS_TEST(command_name_invalid)
{
    static const char *const names[] = { "", "-b", "b c", "b\nc" };
    clopts_program_t *program;
    clopts_command_t *command;
    size_t i;

    program = new_tool();
    command = test_decl_command(program, NULL);
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_UINT_EQ(program->commands.len, 1);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, command, NULL, NULL);

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", names[i]);
        program = new_tool();
        command = test_decl_command(program, names[i]);
        NAOS_REQUIRE_NOT_NULL(command);
        NAOS_CHECK_STR_EQ(command->name, names[i]);
        expect_decl(program, CLOPTS_DECL_INVALID_NAME, command, NULL, NULL);
    }
}

NAOS_TEST(command_help_is_reserved)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *help;

    help = test_decl_command(program, "help");
    NAOS_REQUIRE_NOT_NULL(help);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, help, NULL, NULL);

    /* only a command is reserved: a program may be named help */
    expect_ok(test_decl_program("help"));
}

NAOS_TEST(command_duplicate)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *second;

    test_decl_command(program, "build");
    test_decl_command(program, "copy");
    second = test_decl_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(second);
    NAOS_CHECK_UINT_EQ(program->commands.len, 3);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_COMMAND, second, NULL, NULL);
}

NAOS_TEST(command_names_are_case_sensitive)
{
    clopts_program_t *program = new_tool();

    test_decl_command(program, "build");
    test_decl_command(program, "Build");
    test_decl_command(program, "buil");
    test_decl_command(program, "Help");
    expect_ok(program);
}

NAOS_TEST(command_after_root_positional_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    test_decl_add_positional(clopts_program_root(program), "file");
    build = test_decl_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(build);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, build, NULL, NULL);
}

NAOS_TEST(command_after_root_handler_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    clopts_command_set_handler(clopts_program_root(program), dummy_handler, NULL);
    build = test_decl_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(build);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, build, NULL, NULL);
}

/* ---- row 7: clopts_command_set_handler(root) ---------------------------- */

NAOS_TEST(root_handler_with_commands_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);

    test_decl_command(program, "build");
    clopts_command_set_handler(root, dummy_handler, NULL);
    NAOS_CHECK_TRUE(root->handler == NULL);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, NULL, NULL, NULL);
}

NAOS_TEST(handlers_allowed_on_commands_and_root_unset)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = test_decl_command(program, "build");
    clopts_command_set_handler(build, dummy_handler, NULL);
    clopts_command_set_handler(clopts_program_root(program), NULL, NULL);
    expect_ok(program);
}

/* ---- rows 8-12: clopts_command_add_<kind> ------------------------------- */

NAOS_TEST(option_without_name)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_FLAG, NULL, '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_UINT_EQ(clopts_program_root(program)->options.len, 1);
    expect_decl(program, CLOPTS_DECL_NO_NAME, NULL, option, NULL);

    program = new_tool();
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_COUNT, NULL, '\0');
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_NO_NAME, build, option, NULL);

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_VALUE, NULL, '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_NO_NAME, NULL, option, NULL);

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_LIST, NULL, '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_NO_NAME, NULL, option, NULL);
}

NAOS_TEST(option_long_name_invalid)
{
    static const char *const names[] = { "", "-o", "--o", "out=put", "out put", "out\vput" };
    clopts_program_t *program;
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("long=\"%s\"", names[i]);
        program = new_tool();
        option = test_decl_option(CLOPTS_OPTION_VALUE, names[i], 'o');
        option = test_decl_attach(clopts_program_root(program), option);
        NAOS_REQUIRE_NOT_NULL(option);
        NAOS_CHECK_STR_EQ(option->long_name, names[i]);
        expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, option, NULL);
    }
}

NAOS_TEST(option_short_name_invalid)
{
    static const char shorts[] = { '-', '=', ' ', '\t' };
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(shorts); i++)
    {
        NAOS_NOTE("short=%d", (int)shorts[i]);
        program = new_tool();
        build = test_decl_command(program, "build");
        option = test_decl_option(CLOPTS_OPTION_FLAG, "flag", shorts[i]);
        option = test_decl_attach(build, option);
        NAOS_REQUIRE_NOT_NULL(option);
        expect_decl(program, CLOPTS_DECL_INVALID_NAME, build, option, NULL);
    }
}

NAOS_TEST(option_reserved_names)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_FLAG, "help", '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);

    program = new_tool();
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_VALUE, "host", 'h');
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, build, option, NULL);

    /* version before the option: the option is the offending one */
    program = new_tool();
    clopts_program_set_version(program, "1.0");
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "version", '\0');
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, build, option, NULL);

    /* names that only resemble the reserved ones are allowed */
    program = new_tool();
    clopts_program_set_version(program, "1.0");
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "Help", 'H');
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "versions", 'V');
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "helper", '\0');
    expect_ok(program);
}

NAOS_TEST(option_duplicate_in_same_scope)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    program = new_tool();
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "force", 'f');
    option = test_decl_option(CLOPTS_OPTION_VALUE, "force", 'o');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, NULL, option, NULL);

    program = new_tool();
    build = test_decl_command(program, "build");
    test_decl_add(build, CLOPTS_OPTION_FLAG, "force", 'f');
    option = test_decl_option(CLOPTS_OPTION_COUNT, NULL, 'f');
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, build, option, NULL);
}

NAOS_TEST(option_duplicate_between_root_and_command)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    /* command option clashing with an earlier global */
    program = new_tool();
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "verbose", 'v');
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "verbose", '\0');
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, build, option, NULL);

    /* global clashing with an earlier command option */
    program = new_tool();
    build = test_decl_command(program, "build");
    test_decl_add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    option = test_decl_option(CLOPTS_OPTION_VALUE, "root", 'r');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, NULL, option, NULL);
}

NAOS_TEST(option_same_name_in_different_commands_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_command_t *copy;

    build = test_decl_command(program, "build");
    copy = test_decl_command(program, "copy");
    test_decl_add(build, CLOPTS_OPTION_VALUE, "output", 'o');
    test_decl_add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    test_decl_add(build, CLOPTS_OPTION_FLAG, "Output", 'O');
    expect_ok(program);
}

NAOS_TEST(option_negation_conflict_on_add)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *color;
    clopts_option_t *option;

    /* same scope */
    program = new_tool();
    color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(color, true);
    color = test_decl_attach(clopts_program_root(program), color);
    option = test_decl_option(CLOPTS_OPTION_VALUE, "no-color", '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);

    /* command option against a negatable global */
    program = new_tool();
    color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(color, true);
    color = test_decl_attach(clopts_program_root(program), color);
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "no-color", 'n');
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, build, option, NULL);

    /* global against a negatable command flag that is not the first of its name */
    program = new_tool();
    build = test_decl_command(program, "build");
    copy = test_decl_command(program, "copy");
    test_decl_add(build, CLOPTS_OPTION_FLAG, "color", '\0');
    color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(color, true);
    color = test_decl_attach(copy, color);
    option = test_decl_option(CLOPTS_OPTION_FLAG, "no-color", '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);
}

NAOS_TEST(option_no_prefix_without_negatable_flag_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *color;

    build = test_decl_command(program, "build");
    copy = test_decl_command(program, "copy");
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "color", '\0');
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "no-color", '\0');
    color = test_decl_option(CLOPTS_OPTION_FLAG, "cache", '\0');
    clopts_option_set_negatable(color, true);
    color = test_decl_attach(build, color);
    test_decl_add(copy, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    test_decl_add(build, CLOPTS_OPTION_FLAG, "no-", '\0');
    expect_ok(program);
}

NAOS_TEST(option_checks_follow_table_order)
{
    clopts_program_t *program;
    clopts_option_t *option;

    /* INVALID_NAME before RESERVED_NAME */
    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_FLAG, "-help", 'h');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, option, NULL);

    /* RESERVED_NAME before DUPLICATE_OPTION */
    program = new_tool();
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "hidden", '\0');
    option = test_decl_option(CLOPTS_OPTION_FLAG, "hidden", 'h');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);

    /* DUPLICATE_OPTION before NEGATION_CONFLICT */
    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "quiet", 'q');
    option = test_decl_option(CLOPTS_OPTION_FLAG, "no-color", 'q');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, NULL, option, NULL);
}

/* ---- rows 13-15: clopts_command_add_positional -------------------------- */

NAOS_TEST(positional_name_invalid)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_positional_t *positional;

    program = new_tool();
    positional = test_decl_positional(NULL);
    positional = test_decl_attach_positional(clopts_program_root(program), positional);
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_UINT_EQ(clopts_program_root(program)->positionals.len, 1);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, NULL, positional);

    program = new_tool();
    build = test_decl_command(program, "build");
    positional = test_decl_positional("");
    positional = test_decl_attach_positional(build, positional);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, build, NULL, positional);
}

NAOS_TEST(root_positional_with_commands_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_positional_t *positional;

    test_decl_command(program, "build");
    positional = test_decl_positional("file");
    positional = test_decl_attach_positional(clopts_program_root(program), positional);
    NAOS_REQUIRE_NOT_NULL(positional);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, NULL, NULL, positional);
}

NAOS_TEST(positional_after_variadic)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_positional_t *files;
    clopts_positional_t *extra;

    build = test_decl_command(program, "build");
    files = test_decl_positional("files");
    clopts_positional_set_variadic(files, true);
    files = test_decl_attach_positional(build, files);
    extra = test_decl_positional("extra");
    extra = test_decl_attach_positional(build, extra);
    NAOS_REQUIRE_NOT_NULL(extra);
    expect_decl(program, CLOPTS_DECL_VARIADIC_POSITION, build, NULL, extra);
}

/* ---- rows 16-17: clopts_option_set_env ---------------------------------- */

NAOS_TEST(env_on_flag_or_count)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_env(option, "TOOL_FORCE");
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_NULL(option->env);
    expect_decl(program, CLOPTS_DECL_ENV_NOT_ALLOWED, NULL, option, NULL);

    /* the kind is checked before the empty name */
    program = new_tool();
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_env(option, "");
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_ENV_NOT_ALLOWED, build, option, NULL);
}

NAOS_TEST(env_empty_name)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *option;

    option = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_env(option, "");
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_NULL(option->env);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, option, NULL);
}

NAOS_TEST(env_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    flag = test_decl_attach(clopts_program_root(program), flag);
    value = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    value = test_decl_attach(clopts_program_root(program), value);
    clopts_option_set_env(flag, NULL);
    clopts_option_set_env(value, "TOOL_OUTPUT");
    clopts_option_set_env(value, NULL);
    expect_ok(program);
}

/* ---- rows 18-19: clopts_option_set_default ------------------------------ */

NAOS_TEST(default_on_flag_or_count)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_default(option, "1");
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_NULL(option->default_value);
    expect_decl(program, CLOPTS_DECL_DEFAULT_NOT_ALLOWED, NULL, option, NULL);

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_default(option, "2");
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_DEFAULT_NOT_ALLOWED, NULL, option, NULL);
}

NAOS_TEST(default_on_required)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_option_t *option;

    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_required(option, true);
    clopts_option_set_default(option, "a.out");
    option = test_decl_attach(build, option);
    NAOS_CHECK_NULL(option->default_value);
    expect_decl(program, CLOPTS_DECL_REQUIRED_WITH_DEFAULT, build, option, NULL);
}

NAOS_TEST(default_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *flag;
    clopts_option_t *list;

    flag = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    flag = test_decl_attach(clopts_program_root(program), flag);
    list = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    list = test_decl_attach(clopts_program_root(program), list);
    clopts_option_set_default(flag, NULL);
    clopts_option_set_required(list, true);
    clopts_option_set_default(list, NULL);
    expect_ok(program);
}

/* ---- rows 20-21: clopts_option_set_required(o, true) -------------------- */

NAOS_TEST(required_on_flag_or_count)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_required(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_FALSE(option->required);
    expect_decl(program, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL, option, NULL);

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_required(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL, option, NULL);
}

NAOS_TEST(required_on_option_with_default)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *option;

    option = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_default(option, "a,b");
    clopts_option_set_required(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_FALSE(option->required);
    expect_decl(program, CLOPTS_DECL_REQUIRED_WITH_DEFAULT, NULL, option, NULL);
}

NAOS_TEST(required_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    flag = test_decl_attach(clopts_program_root(program), flag);
    value = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    value = test_decl_attach(clopts_program_root(program), value);
    clopts_option_set_required(flag, false);
    clopts_option_set_default(value, "a.out");
    clopts_option_set_required(value, false);
    expect_ok(program);
}

/* ---- rows 22-23: clopts_option_set_negatable(o, true) ------------------- */

NAOS_TEST(negatable_on_non_flag)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_VALUE, "color", '\0');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_FALSE(option->negatable);
    expect_decl(program, CLOPTS_DECL_NOT_NEGATABLE, NULL, option, NULL);

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_NOT_NEGATABLE, NULL, option, NULL);
}

NAOS_TEST(negatable_with_existing_no_option)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    /* same scope */
    program = new_tool();
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_VALUE, "no-color", '\0');
    option = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_FALSE(option->negatable);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);

    /* command flag against a global no-x */
    program = new_tool();
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "no-cache", '\0');
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "cache", 'c');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(build, option);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, build, option, NULL);

    /* global flag against a command no-x */
    program = new_tool();
    build = test_decl_command(program, "build");
    test_decl_add(build, CLOPTS_OPTION_FLAG, "no-cache", '\0');
    option = test_decl_option(CLOPTS_OPTION_FLAG, "cache", '\0');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);
}

NAOS_TEST(negatable_flag_without_long_name_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *option;

    option = test_decl_option(CLOPTS_OPTION_FLAG, NULL, 'c');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_TRUE(option->negatable);
    expect_ok(program);
}

NAOS_TEST(negatable_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *value;
    clopts_option_t *color;

    value = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    value = test_decl_attach(clopts_program_root(program), value);
    test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "no-color", '\0');
    color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    color = test_decl_attach(clopts_program_root(program), color);
    clopts_option_set_negatable(value, false);
    clopts_option_set_negatable(color, false);
    expect_ok(program);
}

/* ---- rows 24-25: delimiter and count maximum ---------------------------- */

NAOS_TEST(delimiter_on_non_list)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_delimiter(option, ',');
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_INT_EQ(option->delimiter, '\0');
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, NULL, option, NULL);

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_delimiter(option, ':');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, NULL, option, NULL);
}

NAOS_TEST(count_max_on_non_count)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    program = new_tool();
    build = test_decl_command(program, "build");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_count_max(option, 3);
    option = test_decl_attach(build, option);
    NAOS_CHECK_UINT_EQ(option->count_max, 0);
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, build, option, NULL);

    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_count_max(option, 1);
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, NULL, option, NULL);
}

NAOS_TEST(delimiter_and_count_max_unset_are_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *value;

    value = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_delimiter(value, '\0');
    clopts_option_set_count_max(value, 0);
    value = test_decl_attach(clopts_program_root(program), value);
    expect_ok(program);
}

/* ---- rows 26-28: positional setters ------------------------------------- */

NAOS_TEST(required_positional_after_optional)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *copy;
    clopts_positional_t *src;
    clopts_positional_t *dst;

    copy = test_decl_command(program, "copy");
    src = test_decl_positional("src");
    src = test_decl_attach_positional(copy, src);
    dst = test_decl_positional("dst");
    dst = test_decl_attach_positional(copy, dst);
    (void)src;
    clopts_positional_set_required(dst, true);
    NAOS_CHECK_FALSE(dst->required);
    expect_decl(program, CLOPTS_DECL_POSITIONAL_ORDER, copy, NULL, dst);
}

NAOS_TEST(optional_positional_before_required)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *src;
    clopts_positional_t *dst;

    src = test_decl_positional("src");
    src = test_decl_attach_positional(root, src);
    dst = test_decl_positional("dst");
    dst = test_decl_attach_positional(root, dst);
    clopts_positional_set_required(src, true);
    clopts_positional_set_required(dst, true);
    clopts_positional_set_required(src, false);
    NAOS_CHECK_TRUE(src->required);
    expect_decl(program, CLOPTS_DECL_POSITIONAL_ORDER, NULL, NULL, src);
}

NAOS_TEST(positional_required_order_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *a;
    clopts_positional_t *b;
    clopts_positional_t *c;

    a = test_decl_positional("a");
    a = test_decl_attach_positional(root, a);
    b = test_decl_positional("b");
    b = test_decl_attach_positional(root, b);
    c = test_decl_positional("c");
    c = test_decl_attach_positional(root, c);
    clopts_positional_set_required(a, true);
    clopts_positional_set_required(b, true);
    clopts_positional_set_required(b, false);
    clopts_positional_set_required(c, false);
    clopts_positional_set_variadic(c, true);
    NAOS_CHECK_TRUE(a->required);
    NAOS_CHECK_FALSE(b->required);
    expect_ok(program);
}

NAOS_TEST(variadic_not_last)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_positional_t *first;

    build = test_decl_command(program, "build");
    first = test_decl_positional("first");
    first = test_decl_attach_positional(build, first);
    test_decl_add_positional(build, "second");
    clopts_positional_set_variadic(first, true);
    NAOS_CHECK_FALSE(first->variadic);
    expect_decl(program, CLOPTS_DECL_VARIADIC_POSITION, build, NULL, first);
}

NAOS_TEST(variadic_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *first;

    first = test_decl_positional("first");
    first = test_decl_attach_positional(root, first);
    test_decl_add_positional(root, "second");
    clopts_positional_set_variadic(first, false);
    expect_ok(program);
}

/* ---- ordering of errors -------------------------------------------------- */

NAOS_TEST(version_option_conflict_in_both_orders)
{
    clopts_program_t *program;
    clopts_option_t *option;

    /* option first: reported by clopts_program_set_version */
    program = new_tool();
    option = test_decl_option(CLOPTS_OPTION_VALUE, "version", '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    clopts_program_set_version(program, "1.2.3");
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);

    /* version first: reported by the add */
    program = new_tool();
    clopts_program_set_version(program, "1.2.3");
    option = test_decl_option(CLOPTS_OPTION_VALUE, "version", '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);
}

NAOS_TEST(first_of_two_errors_wins)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *first;
    clopts_option_t *second;

    program = new_tool();
    build = test_decl_command(program, "build");
    first = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_env(first, "TOOL_FORCE");
    first = test_decl_attach(build, first);
    second = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    second = test_decl_attach(build, second);
    NAOS_CHECK_NULL(second);
    expect_decl(program, CLOPTS_DECL_ENV_NOT_ALLOWED, build, first, NULL);

    /* an error that only appears with a later call is ordered by that call */
    program = new_tool();
    first = test_decl_option(CLOPTS_OPTION_FLAG, "version", '\0');
    first = test_decl_attach(clopts_program_root(program), first);
    second = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_required(second, true);
    second = test_decl_attach(clopts_program_root(program), second);
    clopts_program_set_version(program, "1.0");
    expect_decl(program, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL, second, NULL);
    (void)first;
}

NAOS_TEST(program_name_error_comes_first)
{
    clopts_program_t *program = test_decl_program("");
    clopts_option_t *option;

    NAOS_REQUIRE_NOT_NULL(program);
    option = test_decl_option(CLOPTS_OPTION_FLAG, NULL, '\0');
    option = test_decl_attach(clopts_program_root(program), option);
    NAOS_CHECK_NULL(option);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
}

/* ---- valid declarations -------------------------------------------------- */

/*
 * A declaration shaped like the fixture "tool": globals, two commands, a
 * negatable flag, env and default, a list delimiter, a count maximum,
 * required options and positionals with a variadic tail, and handlers.
 */
NAOS_TEST(tool_like_declaration_is_ok)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *option;
    clopts_positional_t *positional;

    clopts_program_set_version(program, "1.0.0");
    clopts_command_set_description(root, "A sample tool.");
    option = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_count_max(option, 3);
    option = test_decl_attach(root, option);
    option = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(option, true);
    option = test_decl_attach(root, option);
    option = test_decl_option(CLOPTS_OPTION_VALUE, "config", 'c');
    clopts_option_set_env(option, "TOOL_CONFIG");
    option = test_decl_attach(root, option);

    build = test_decl_command(program, "build");
    clopts_command_set_summary(build, "Build things.");
    clopts_command_set_handler(build, dummy_handler, NULL);
    option = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_env(option, "TOOL_OUTPUT");
    clopts_option_set_default(option, "a.out");
    option = test_decl_attach(build, option);
    option = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');
    clopts_option_set_delimiter(option, ',');
    clopts_option_set_required(option, true);
    option = test_decl_attach(build, option);
    option = test_decl_option(CLOPTS_OPTION_COUNT, "jobs", 'j');
    clopts_option_set_count_max(option, 8);
    option = test_decl_attach(build, option);
    test_decl_add(build, CLOPTS_OPTION_FLAG, "release", 'r');
    positional = test_decl_positional("src");
    clopts_positional_set_required(positional, true);
    positional = test_decl_attach_positional(build, positional);
    positional = test_decl_positional("rest");
    clopts_positional_set_variadic(positional, true);
    positional = test_decl_attach_positional(build, positional);

    copy = test_decl_command(program, "copy");
    clopts_command_set_handler(copy, dummy_handler, NULL);
    test_decl_add(copy, CLOPTS_OPTION_FLAG, "release", 'r');
    test_decl_add(copy, CLOPTS_OPTION_VALUE, "output", 'o');
    positional = test_decl_positional("src");
    clopts_positional_set_required(positional, true);
    positional = test_decl_attach_positional(copy, positional);
    positional = test_decl_positional("dst");
    clopts_positional_set_required(positional, true);
    positional = test_decl_attach_positional(copy, positional);

    NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OK);
    expect_ok(program);
}

NAOS_TEST(adds_with_errors_are_linked)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_option_t *option;

    test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    option = test_decl_option(CLOPTS_OPTION_VALUE, "force", '\0');
    option = test_decl_attach(root, option);
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_UINT_EQ(root->options.len, 2);
    NAOS_CHECK_PTR_EQ(option_at(root, 1), option);
    NAOS_CHECK_UINT_EQ(option->index, 1);
    NAOS_CHECK_STR_EQ(option->long_name, "force");
    NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_DECL_ERROR);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, NULL, option, NULL);
}

NAOS_TEST(added_positional_with_error_is_linked)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *positional;

    test_decl_command(program, "build");
    positional = test_decl_positional("file");
    positional = test_decl_attach_positional(root, positional);
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_PTR_EQ(positional_at(root, 0), positional);
    NAOS_CHECK_STR_EQ(positional->name, "file");
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, NULL, NULL, positional);
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
    f.verbose = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    f.verbose = test_decl_attach(root, f.verbose);
    f.color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(f.color, true);
    f.color = test_decl_attach(root, f.color);
    f.quiet = test_decl_option(CLOPTS_OPTION_FLAG, "quiet", 'q');
    f.quiet = test_decl_attach(root, f.quiet);
    f.build = test_decl_command(f.program, "build");
    f.copy = test_decl_command(f.program, "copy");
    f.build_output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    f.build_output = test_decl_attach(f.build, f.build_output);
    f.build_cache = test_decl_option(CLOPTS_OPTION_FLAG, "cache", 'c');
    clopts_option_set_negatable(f.build_cache, true);
    f.build_cache = test_decl_attach(f.build, f.build_cache);
    f.build_release = test_decl_option(CLOPTS_OPTION_FLAG, NULL, 'r');
    clopts_option_set_negatable(f.build_release, true);
    f.build_release = test_decl_attach(f.build, f.build_release);
    f.copy_output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    f.copy_output = test_decl_attach(f.copy, f.copy_output);
    NAOS_REQUIRE_INT_EQ(f.program->error.status, CLOPTS_OK);
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
    NAOS_ADD_TEST(version_after_version_option_in_root),
    NAOS_ADD_TEST(version_after_version_option_in_command),
    NAOS_ADD_TEST(version_unset_is_not_checked),
    NAOS_ADD_TEST(version_option_allowed_without_version),
    NAOS_ADD_TEST(command_name_invalid),
    NAOS_ADD_TEST(command_help_is_reserved),
    NAOS_ADD_TEST(command_duplicate),
    NAOS_ADD_TEST(command_names_are_case_sensitive),
    NAOS_ADD_TEST(command_after_root_positional_is_mixed),
    NAOS_ADD_TEST(command_after_root_handler_is_mixed),
    NAOS_ADD_TEST(root_handler_with_commands_is_mixed),
    NAOS_ADD_TEST(handlers_allowed_on_commands_and_root_unset),
    NAOS_ADD_TEST(option_without_name),
    NAOS_ADD_TEST(option_long_name_invalid),
    NAOS_ADD_TEST(option_short_name_invalid),
    NAOS_ADD_TEST(option_reserved_names),
    NAOS_ADD_TEST(option_duplicate_in_same_scope),
    NAOS_ADD_TEST(option_duplicate_between_root_and_command),
    NAOS_ADD_TEST(option_same_name_in_different_commands_is_allowed),
    NAOS_ADD_TEST(option_negation_conflict_on_add),
    NAOS_ADD_TEST(option_no_prefix_without_negatable_flag_is_allowed),
    NAOS_ADD_TEST(option_checks_follow_table_order),
    NAOS_ADD_TEST(positional_name_invalid),
    NAOS_ADD_TEST(root_positional_with_commands_is_mixed),
    NAOS_ADD_TEST(positional_after_variadic),
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
    NAOS_ADD_TEST(negatable_with_existing_no_option),
    NAOS_ADD_TEST(negatable_flag_without_long_name_is_allowed),
    NAOS_ADD_TEST(negatable_unset_is_not_checked),
    NAOS_ADD_TEST(delimiter_on_non_list),
    NAOS_ADD_TEST(count_max_on_non_count),
    NAOS_ADD_TEST(delimiter_and_count_max_unset_are_not_checked),
    NAOS_ADD_TEST(required_positional_after_optional),
    NAOS_ADD_TEST(optional_positional_before_required),
    NAOS_ADD_TEST(positional_required_order_allowed),
    NAOS_ADD_TEST(variadic_not_last),
    NAOS_ADD_TEST(variadic_unset_is_not_checked),
    NAOS_ADD_TEST(version_option_conflict_in_both_orders),
    NAOS_ADD_TEST(first_of_two_errors_wins),
    NAOS_ADD_TEST(program_name_error_comes_first),
    NAOS_ADD_TEST(tool_like_declaration_is_ok),
    NAOS_ADD_TEST(adds_with_errors_are_linked),
    NAOS_ADD_TEST(added_positional_with_error_is_linked),
    NAOS_ADD_TEST(find_command_is_exact_and_case_sensitive),
    NAOS_ADD_TEST(find_long_searches_command_then_root),
    NAOS_ADD_TEST(find_long_is_exact_case_sensitive_and_uses_name_len),
    NAOS_ADD_TEST(find_negation_only_for_negatable_flags_with_long_name),
    NAOS_ADD_TEST(find_short_searches_command_then_root),
    NAOS_ADD_TEST(find_conflict_and_named_cover_the_lookup_range)
)
