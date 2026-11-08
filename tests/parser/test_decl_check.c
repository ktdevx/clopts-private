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
    clopts_program_t *program = clopts_program_new("tool");

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

    expect_decl(clopts_program_new(NULL), CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", names[i]);
        expect_decl(clopts_program_new(names[i]), CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
    }
    expect_ok(clopts_program_new("my-tool"));
}

/* ---- row 2: clopts_program_set_version ---------------------------------- */

NAOS_TEST(version_after_version_option_in_root)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *version;

    version = clopts_command_add_flag(clopts_program_root(program), "version", 'V');
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

    build = clopts_program_add_command(program, "build");
    copy = clopts_program_add_command(program, "copy");
    first = clopts_command_add_value(build, "version", '\0');
    (void)clopts_command_add_value(copy, "version", '\0');
    clopts_program_set_version(program, "1.0");
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, build, first, NULL);
}

NAOS_TEST(version_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();

    (void)clopts_command_add_flag(clopts_program_root(program), "version", '\0');
    clopts_program_set_version(program, NULL);
    expect_ok(program);
}

NAOS_TEST(version_option_allowed_without_version)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = clopts_program_add_command(program, "build");
    (void)clopts_command_add_flag(clopts_program_root(program), "version", 'V');
    (void)clopts_command_add_value(build, "version-file", '\0');
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
    command = clopts_program_add_command(program, NULL);
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_UINT_EQ(program->commands.len, 1);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, command, NULL, NULL);

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("name=\"%s\"", names[i]);
        program = new_tool();
        command = clopts_program_add_command(program, names[i]);
        NAOS_REQUIRE_NOT_NULL(command);
        NAOS_CHECK_STR_EQ(command->name, names[i]);
        expect_decl(program, CLOPTS_DECL_INVALID_NAME, command, NULL, NULL);
    }
}

NAOS_TEST(command_help_is_reserved)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *help;

    help = clopts_program_add_command(program, "help");
    NAOS_REQUIRE_NOT_NULL(help);
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, help, NULL, NULL);

    /* only a command is reserved: a program may be named help */
    expect_ok(clopts_program_new("help"));
}

NAOS_TEST(command_duplicate)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *second;

    (void)clopts_program_add_command(program, "build");
    (void)clopts_program_add_command(program, "copy");
    second = clopts_program_add_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(second);
    NAOS_CHECK_UINT_EQ(program->commands.len, 3);
    expect_decl(program, CLOPTS_DECL_DUPLICATE_COMMAND, second, NULL, NULL);
}

NAOS_TEST(command_names_are_case_sensitive)
{
    clopts_program_t *program = new_tool();

    (void)clopts_program_add_command(program, "build");
    (void)clopts_program_add_command(program, "Build");
    (void)clopts_program_add_command(program, "buil");
    (void)clopts_program_add_command(program, "Help");
    expect_ok(program);
}

NAOS_TEST(command_after_root_positional_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    (void)clopts_command_add_positional(clopts_program_root(program), "file");
    build = clopts_program_add_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(build);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, build, NULL, NULL);
}

NAOS_TEST(command_after_root_handler_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    clopts_command_set_handler(clopts_program_root(program), dummy_handler, NULL);
    build = clopts_program_add_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(build);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, build, NULL, NULL);
}

/* ---- row 7: clopts_command_set_handler(root) ---------------------------- */

NAOS_TEST(root_handler_with_commands_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);

    (void)clopts_program_add_command(program, "build");
    clopts_command_set_handler(root, dummy_handler, NULL);
    NAOS_CHECK_TRUE(root->handler == NULL);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, NULL, NULL, NULL);
}

NAOS_TEST(handlers_allowed_on_commands_and_root_unset)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;

    build = clopts_program_add_command(program, "build");
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
    option = clopts_command_add_flag(clopts_program_root(program), NULL, '\0');
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_UINT_EQ(clopts_program_root(program)->options.len, 1);
    expect_decl(program, CLOPTS_DECL_NO_NAME, NULL, option, NULL);

    program = new_tool();
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_count(build, NULL, '\0');
    expect_decl(program, CLOPTS_DECL_NO_NAME, build, option, NULL);

    program = new_tool();
    option = clopts_command_add_value(clopts_program_root(program), NULL, '\0');
    expect_decl(program, CLOPTS_DECL_NO_NAME, NULL, option, NULL);

    program = new_tool();
    option = clopts_command_add_list(clopts_program_root(program), NULL, '\0');
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
        option = clopts_command_add_value(clopts_program_root(program), names[i], 'o');
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
        build = clopts_program_add_command(program, "build");
        option = clopts_command_add_flag(build, "flag", shorts[i]);
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
    option = clopts_command_add_flag(clopts_program_root(program), "help", '\0');
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);

    program = new_tool();
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_value(build, "host", 'h');
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, build, option, NULL);

    /* version before the option: the option is the offending one */
    program = new_tool();
    clopts_program_set_version(program, "1.0");
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_flag(build, "version", '\0');
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, build, option, NULL);

    /* names that only resemble the reserved ones are allowed */
    program = new_tool();
    clopts_program_set_version(program, "1.0");
    (void)clopts_command_add_flag(clopts_program_root(program), "Help", 'H');
    (void)clopts_command_add_flag(clopts_program_root(program), "versions", 'V');
    (void)clopts_command_add_flag(clopts_program_root(program), "helper", '\0');
    expect_ok(program);
}

NAOS_TEST(option_duplicate_in_same_scope)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    program = new_tool();
    (void)clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    option = clopts_command_add_value(clopts_program_root(program), "force", 'o');
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, NULL, option, NULL);

    program = new_tool();
    build = clopts_program_add_command(program, "build");
    (void)clopts_command_add_flag(build, "force", 'f');
    option = clopts_command_add_count(build, NULL, 'f');
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, build, option, NULL);
}

NAOS_TEST(option_duplicate_between_root_and_command)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    /* command option clashing with an earlier global */
    program = new_tool();
    (void)clopts_command_add_flag(clopts_program_root(program), "verbose", 'v');
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_flag(build, "verbose", '\0');
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, build, option, NULL);

    /* global clashing with an earlier command option */
    program = new_tool();
    build = clopts_program_add_command(program, "build");
    (void)clopts_command_add_flag(build, "release", 'r');
    option = clopts_command_add_value(clopts_program_root(program), "root", 'r');
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, NULL, option, NULL);
}

NAOS_TEST(option_same_name_in_different_commands_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_command_t *copy;

    build = clopts_program_add_command(program, "build");
    copy = clopts_program_add_command(program, "copy");
    (void)clopts_command_add_value(build, "output", 'o');
    (void)clopts_command_add_value(copy, "output", 'o');
    (void)clopts_command_add_flag(build, "Output", 'O');
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
    color = clopts_command_add_flag(clopts_program_root(program), "color", '\0');
    clopts_option_set_negatable(color, true);
    option = clopts_command_add_value(clopts_program_root(program), "no-color", '\0');
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);

    /* command option against a negatable global */
    program = new_tool();
    color = clopts_command_add_flag(clopts_program_root(program), "color", '\0');
    clopts_option_set_negatable(color, true);
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_flag(build, "no-color", 'n');
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, build, option, NULL);

    /* global against a negatable command flag that is not the first of its name */
    program = new_tool();
    build = clopts_program_add_command(program, "build");
    copy = clopts_program_add_command(program, "copy");
    (void)clopts_command_add_flag(build, "color", '\0');
    color = clopts_command_add_flag(copy, "color", '\0');
    clopts_option_set_negatable(color, true);
    option = clopts_command_add_flag(clopts_program_root(program), "no-color", '\0');
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);
}

NAOS_TEST(option_no_prefix_without_negatable_flag_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_option_t *color;

    build = clopts_program_add_command(program, "build");
    copy = clopts_program_add_command(program, "copy");
    (void)clopts_command_add_flag(clopts_program_root(program), "color", '\0');
    (void)clopts_command_add_flag(clopts_program_root(program), "no-color", '\0');
    color = clopts_command_add_flag(build, "cache", '\0');
    clopts_option_set_negatable(color, true);
    (void)clopts_command_add_flag(copy, "no-cache", '\0');
    (void)clopts_command_add_flag(build, "no-", '\0');
    expect_ok(program);
}

NAOS_TEST(option_checks_follow_table_order)
{
    clopts_program_t *program;
    clopts_option_t *option;

    /* INVALID_NAME before RESERVED_NAME */
    program = new_tool();
    option = clopts_command_add_flag(clopts_program_root(program), "-help", 'h');
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, option, NULL);

    /* RESERVED_NAME before DUPLICATE_OPTION */
    program = new_tool();
    (void)clopts_command_add_flag(clopts_program_root(program), "hidden", '\0');
    option = clopts_command_add_flag(clopts_program_root(program), "hidden", 'h');
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);

    /* DUPLICATE_OPTION before NEGATION_CONFLICT */
    program = new_tool();
    option = clopts_command_add_flag(clopts_program_root(program), "color", '\0');
    clopts_option_set_negatable(option, true);
    (void)clopts_command_add_flag(clopts_program_root(program), "quiet", 'q');
    option = clopts_command_add_flag(clopts_program_root(program), "no-color", 'q');
    expect_decl(program, CLOPTS_DECL_DUPLICATE_OPTION, NULL, option, NULL);
}

/* ---- rows 13-15: clopts_command_add_positional -------------------------- */

NAOS_TEST(positional_name_invalid)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_positional_t *positional;

    program = new_tool();
    positional = clopts_command_add_positional(clopts_program_root(program), NULL);
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_UINT_EQ(clopts_program_root(program)->positionals.len, 1);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, NULL, positional);

    program = new_tool();
    build = clopts_program_add_command(program, "build");
    positional = clopts_command_add_positional(build, "");
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, build, NULL, positional);
}

NAOS_TEST(root_positional_with_commands_is_mixed)
{
    clopts_program_t *program = new_tool();
    clopts_positional_t *positional;

    (void)clopts_program_add_command(program, "build");
    positional = clopts_command_add_positional(clopts_program_root(program), "file");
    NAOS_REQUIRE_NOT_NULL(positional);
    expect_decl(program, CLOPTS_DECL_MIXED_LAYOUT, NULL, NULL, positional);
}

NAOS_TEST(positional_after_variadic)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_positional_t *files;
    clopts_positional_t *extra;

    build = clopts_program_add_command(program, "build");
    files = clopts_command_add_positional(build, "files");
    clopts_positional_set_variadic(files, true);
    extra = clopts_command_add_positional(build, "extra");
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
    option = clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    clopts_option_set_env(option, "TOOL_FORCE");
    NAOS_CHECK_NULL(option->env);
    expect_decl(program, CLOPTS_DECL_ENV_NOT_ALLOWED, NULL, option, NULL);

    /* the kind is checked before the empty name */
    program = new_tool();
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_count(build, "verbose", 'v');
    clopts_option_set_env(option, "");
    expect_decl(program, CLOPTS_DECL_ENV_NOT_ALLOWED, build, option, NULL);
}

NAOS_TEST(env_empty_name)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *option;

    option = clopts_command_add_list(clopts_program_root(program), "include", 'I');
    clopts_option_set_env(option, "");
    NAOS_CHECK_NULL(option->env);
    expect_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, option, NULL);
}

NAOS_TEST(env_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    value = clopts_command_add_value(clopts_program_root(program), "output", 'o');
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
    option = clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    clopts_option_set_default(option, "1");
    NAOS_CHECK_NULL(option->default_value);
    expect_decl(program, CLOPTS_DECL_DEFAULT_NOT_ALLOWED, NULL, option, NULL);

    program = new_tool();
    option = clopts_command_add_count(clopts_program_root(program), "verbose", 'v');
    clopts_option_set_default(option, "2");
    expect_decl(program, CLOPTS_DECL_DEFAULT_NOT_ALLOWED, NULL, option, NULL);
}

NAOS_TEST(default_on_required)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *build;
    clopts_option_t *option;

    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_value(build, "output", 'o');
    clopts_option_set_required(option, true);
    clopts_option_set_default(option, "a.out");
    NAOS_CHECK_NULL(option->default_value);
    expect_decl(program, CLOPTS_DECL_REQUIRED_WITH_DEFAULT, build, option, NULL);
}

NAOS_TEST(default_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *flag;
    clopts_option_t *list;

    flag = clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    list = clopts_command_add_list(clopts_program_root(program), "include", 'I');
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
    option = clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    clopts_option_set_required(option, true);
    NAOS_CHECK_FALSE(option->required);
    expect_decl(program, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL, option, NULL);

    program = new_tool();
    option = clopts_command_add_count(clopts_program_root(program), "verbose", 'v');
    clopts_option_set_required(option, true);
    expect_decl(program, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL, option, NULL);
}

NAOS_TEST(required_on_option_with_default)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *option;

    option = clopts_command_add_list(clopts_program_root(program), "include", 'I');
    clopts_option_set_default(option, "a,b");
    clopts_option_set_required(option, true);
    NAOS_CHECK_FALSE(option->required);
    expect_decl(program, CLOPTS_DECL_REQUIRED_WITH_DEFAULT, NULL, option, NULL);
}

NAOS_TEST(required_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    value = clopts_command_add_value(clopts_program_root(program), "output", 'o');
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
    option = clopts_command_add_value(clopts_program_root(program), "color", '\0');
    clopts_option_set_negatable(option, true);
    NAOS_CHECK_FALSE(option->negatable);
    expect_decl(program, CLOPTS_DECL_NOT_NEGATABLE, NULL, option, NULL);

    program = new_tool();
    option = clopts_command_add_count(clopts_program_root(program), "verbose", 'v');
    clopts_option_set_negatable(option, true);
    expect_decl(program, CLOPTS_DECL_NOT_NEGATABLE, NULL, option, NULL);
}

NAOS_TEST(negatable_with_existing_no_option)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    /* same scope */
    program = new_tool();
    (void)clopts_command_add_value(clopts_program_root(program), "no-color", '\0');
    option = clopts_command_add_flag(clopts_program_root(program), "color", '\0');
    clopts_option_set_negatable(option, true);
    NAOS_CHECK_FALSE(option->negatable);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);

    /* command flag against a global no-x */
    program = new_tool();
    (void)clopts_command_add_flag(clopts_program_root(program), "no-cache", '\0');
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_flag(build, "cache", 'c');
    clopts_option_set_negatable(option, true);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, build, option, NULL);

    /* global flag against a command no-x */
    program = new_tool();
    build = clopts_program_add_command(program, "build");
    (void)clopts_command_add_flag(build, "no-cache", '\0');
    option = clopts_command_add_flag(clopts_program_root(program), "cache", '\0');
    clopts_option_set_negatable(option, true);
    expect_decl(program, CLOPTS_DECL_NEGATION_CONFLICT, NULL, option, NULL);
}

NAOS_TEST(negatable_flag_without_long_name_is_allowed)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *option;

    option = clopts_command_add_flag(clopts_program_root(program), NULL, 'c');
    clopts_option_set_negatable(option, true);
    NAOS_CHECK_TRUE(option->negatable);
    expect_ok(program);
}

NAOS_TEST(negatable_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *value;
    clopts_option_t *color;

    value = clopts_command_add_value(clopts_program_root(program), "output", 'o');
    (void)clopts_command_add_flag(clopts_program_root(program), "no-color", '\0');
    color = clopts_command_add_flag(clopts_program_root(program), "color", '\0');
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
    option = clopts_command_add_value(clopts_program_root(program), "output", 'o');
    clopts_option_set_delimiter(option, ',');
    NAOS_CHECK_INT_EQ(option->delimiter, '\0');
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, NULL, option, NULL);

    program = new_tool();
    option = clopts_command_add_count(clopts_program_root(program), "verbose", 'v');
    clopts_option_set_delimiter(option, ':');
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, NULL, option, NULL);
}

NAOS_TEST(count_max_on_non_count)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *option;

    program = new_tool();
    build = clopts_program_add_command(program, "build");
    option = clopts_command_add_flag(build, "force", 'f');
    clopts_option_set_count_max(option, 3);
    NAOS_CHECK_UINT_EQ(option->count_max, 0);
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, build, option, NULL);

    program = new_tool();
    option = clopts_command_add_list(clopts_program_root(program), "include", 'I');
    clopts_option_set_count_max(option, 1);
    expect_decl(program, CLOPTS_DECL_ATTRIBUTE_KIND, NULL, option, NULL);
}

NAOS_TEST(delimiter_and_count_max_unset_are_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_option_t *value;

    value = clopts_command_add_value(clopts_program_root(program), "output", 'o');
    clopts_option_set_delimiter(value, '\0');
    clopts_option_set_count_max(value, 0);
    expect_ok(program);
}

/* ---- rows 26-28: positional setters ------------------------------------- */

NAOS_TEST(required_positional_after_optional)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *copy;
    clopts_positional_t *src;
    clopts_positional_t *dst;

    copy = clopts_program_add_command(program, "copy");
    src = clopts_command_add_positional(copy, "src");
    dst = clopts_command_add_positional(copy, "dst");
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

    src = clopts_command_add_positional(root, "src");
    dst = clopts_command_add_positional(root, "dst");
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

    a = clopts_command_add_positional(root, "a");
    b = clopts_command_add_positional(root, "b");
    c = clopts_command_add_positional(root, "c");
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

    build = clopts_program_add_command(program, "build");
    first = clopts_command_add_positional(build, "first");
    (void)clopts_command_add_positional(build, "second");
    clopts_positional_set_variadic(first, true);
    NAOS_CHECK_FALSE(first->variadic);
    expect_decl(program, CLOPTS_DECL_VARIADIC_POSITION, build, NULL, first);
}

NAOS_TEST(variadic_unset_is_not_checked)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *first;

    first = clopts_command_add_positional(root, "first");
    (void)clopts_command_add_positional(root, "second");
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
    option = clopts_command_add_value(clopts_program_root(program), "version", '\0');
    clopts_program_set_version(program, "1.2.3");
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);

    /* version first: reported by the add */
    program = new_tool();
    clopts_program_set_version(program, "1.2.3");
    option = clopts_command_add_value(clopts_program_root(program), "version", '\0');
    expect_decl(program, CLOPTS_DECL_RESERVED_NAME, NULL, option, NULL);
}

NAOS_TEST(first_of_two_errors_wins)
{
    clopts_program_t *program;
    clopts_command_t *build;
    clopts_option_t *first;
    clopts_option_t *second;

    program = new_tool();
    build = clopts_program_add_command(program, "build");
    first = clopts_command_add_flag(build, "force", 'f');
    clopts_option_set_env(first, "TOOL_FORCE");
    second = clopts_command_add_flag(build, "force", 'f');
    NAOS_CHECK_NULL(second);
    expect_decl(program, CLOPTS_DECL_ENV_NOT_ALLOWED, build, first, NULL);

    /* an error that only appears with a later call is ordered by that call */
    program = new_tool();
    first = clopts_command_add_flag(clopts_program_root(program), "version", '\0');
    second = clopts_command_add_count(clopts_program_root(program), "verbose", 'v');
    clopts_option_set_required(second, true);
    clopts_program_set_version(program, "1.0");
    expect_decl(program, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL, second, NULL);
    (void)first;
}

NAOS_TEST(program_name_error_comes_first)
{
    clopts_program_t *program = clopts_program_new("");
    clopts_option_t *option;

    NAOS_REQUIRE_NOT_NULL(program);
    option = clopts_command_add_flag(clopts_program_root(program), NULL, '\0');
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
    option = clopts_command_add_count(root, "verbose", 'v');
    clopts_option_set_count_max(option, 3);
    option = clopts_command_add_flag(root, "color", '\0');
    clopts_option_set_negatable(option, true);
    option = clopts_command_add_value(root, "config", 'c');
    clopts_option_set_env(option, "TOOL_CONFIG");

    build = clopts_program_add_command(program, "build");
    clopts_command_set_summary(build, "Build things.");
    clopts_command_set_handler(build, dummy_handler, NULL);
    option = clopts_command_add_value(build, "output", 'o');
    clopts_option_set_env(option, "TOOL_OUTPUT");
    clopts_option_set_default(option, "a.out");
    option = clopts_command_add_list(build, "keys", 'k');
    clopts_option_set_delimiter(option, ',');
    clopts_option_set_required(option, true);
    option = clopts_command_add_count(build, "jobs", 'j');
    clopts_option_set_count_max(option, 8);
    (void)clopts_command_add_flag(build, "release", 'r');
    positional = clopts_command_add_positional(build, "src");
    clopts_positional_set_required(positional, true);
    positional = clopts_command_add_positional(build, "rest");
    clopts_positional_set_variadic(positional, true);

    copy = clopts_program_add_command(program, "copy");
    clopts_command_set_handler(copy, dummy_handler, NULL);
    (void)clopts_command_add_flag(copy, "release", 'r');
    (void)clopts_command_add_value(copy, "output", 'o');
    positional = clopts_command_add_positional(copy, "src");
    clopts_positional_set_required(positional, true);
    positional = clopts_command_add_positional(copy, "dst");
    clopts_positional_set_required(positional, true);

    NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OK);
    expect_ok(program);
}

NAOS_TEST(adds_with_errors_are_linked)
{
    clopts_program_t *program = new_tool();
    clopts_command_t *root = clopts_program_root(program);
    clopts_option_t *option;

    (void)clopts_command_add_flag(root, "force", 'f');
    option = clopts_command_add_value(root, "force", '\0');
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

    (void)clopts_program_add_command(program, "build");
    positional = clopts_command_add_positional(root, "file");
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
    f.verbose = clopts_command_add_count(root, "verbose", 'v');
    f.color = clopts_command_add_flag(root, "color", '\0');
    clopts_option_set_negatable(f.color, true);
    f.quiet = clopts_command_add_flag(root, "quiet", 'q');
    f.build = clopts_program_add_command(f.program, "build");
    f.copy = clopts_program_add_command(f.program, "copy");
    f.build_output = clopts_command_add_value(f.build, "output", 'o');
    f.build_cache = clopts_command_add_flag(f.build, "cache", 'c');
    clopts_option_set_negatable(f.build_cache, true);
    f.build_release = clopts_command_add_flag(f.build, NULL, 'r');
    clopts_option_set_negatable(f.build_release, true);
    f.copy_output = clopts_command_add_value(f.copy, "output", 'o');
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
