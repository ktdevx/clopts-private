/*
 * Suite build: the builder functions, the releases, string copies, the
 * last-call-wins and reset rules, handle stability, out of memory in every
 * allocating builder function, freezing by a parse, and the contents of the
 * shared fixtures (test_fixtures.c) compared with the first-version
 * declarations.
 *
 * Every builder function returns its error right away and a failing call
 * changes nothing; nothing is recorded in the program. The declaration rules
 * (and the NULL and already-attached arguments) are tested in the decl
 * suite. Stored values are read through the internal model, and the fault
 * allocator (test_alloc.h) shows which blocks are alive.
 */

#include "parser/parser_internal.h"
#include "test_alloc.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

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
    return *(clopts_command_t **)clopts_core_vec_at(&program->commands, index);
}

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

/* Parses argv = { name } with program, checks the status, and releases the result. */
static void parse_name_only(clopts_program_t *program, const char *name, clopts_status_t status)
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
    clopts_program_free(NULL);
    clopts_option_free(NULL);
    clopts_positional_free(NULL);
    NAOS_CHECK_NULL(clopts_program_root(NULL));
}

/* ---- program and root -------------------------------------------------- */

NAOS_TEST(program_new_embeds_root_and_copies_name)
{
    char name[] = "tool";
    clopts_program_t *program = &sentinel_program;
    clopts_command_t *root;

    EXPECT_OK(clopts_program_new(name, &program));
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_TRUE(program != &sentinel_program);
    name[0] = 'X';

    root = clopts_program_root(program);
    NAOS_REQUIRE_PTR_EQ(root, &program->root);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);
    NAOS_CHECK_PTR_EQ(root->program, program);
    NAOS_REQUIRE_NOT_NULL(root->name);
    NAOS_CHECK_STR_EQ(root->name, "tool");
    NAOS_CHECK_NULL(root->summary);
    NAOS_CHECK_NULL(root->description);
    NAOS_CHECK_TRUE(root->handler == NULL);
    NAOS_CHECK_NULL(root->userdata);
    NAOS_CHECK_UINT_EQ(root->options.len, 0);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 0);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_FALSE(program->frozen);

    clopts_program_free(program);
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
        option = &sentinel_option;
        EXPECT_OK(clopts_option_new(kinds[i], long_name, 'o', &option));
        NAOS_REQUIRE_NOT_NULL(option);
        NAOS_REQUIRE_TRUE(option != &sentinel_option);
        NAOS_CHECK_FALSE(option->attached);
        NAOS_CHECK_INT_EQ(option->kind, kinds[i]);
        NAOS_CHECK_STR_EQ(option->long_name, "output");
        NAOS_CHECK_TRUE(option->long_name != long_name);
        NAOS_CHECK_INT_EQ(option->short_name, 'o');
        NAOS_CHECK_NULL(option->help);
        NAOS_CHECK_NULL(option->value_name);
        NAOS_CHECK_NULL(option->default_value);
        NAOS_CHECK_NULL(option->env);
        NAOS_CHECK_INT_EQ(option->delimiter, '\0');
        NAOS_CHECK_UINT_EQ(option->count_max, 0);
        NAOS_CHECK_FALSE(option->required);
        NAOS_CHECK_FALSE(option->negatable);
        clopts_option_free(option);
    }

    option = make(CLOPTS_OPTION_VALUE, NULL, 'o');
    NAOS_CHECK_NULL(option->long_name);
    clopts_option_free(option);
    option = make(CLOPTS_OPTION_FLAG, "force", '\0');
    NAOS_CHECK_INT_EQ(option->short_name, '\0');
    clopts_option_free(option);

    positional = &sentinel_positional;
    EXPECT_OK(clopts_positional_new(pos_name, &positional));
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_REQUIRE_TRUE(positional != &sentinel_positional);
    pos_name[0] = 'X';
    NAOS_CHECK_FALSE(positional->attached);
    NAOS_CHECK_STR_EQ(positional->name, "src");
    NAOS_CHECK_NULL(positional->help);
    NAOS_CHECK_FALSE(positional->required);
    NAOS_CHECK_FALSE(positional->variadic);
    clopts_positional_free(positional);
}

/* An option or positional that is never attached is released by its own free, block by block. */
NAOS_TEST(unattached_objects_are_released_by_their_free)
{
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

    clopts_option_free(option);
    clopts_positional_free(positional);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
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

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

    flag = test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    count = test_decl_add(root, CLOPTS_OPTION_COUNT, "verbose", 'v');
    value = test_decl_add(root, CLOPTS_OPTION_VALUE, NULL, 'o');
    list = test_decl_add(root, CLOPTS_OPTION_LIST, "include", '\0');
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

    /* root options in order of attachment */
    NAOS_REQUIRE_UINT_EQ(root->options.len, 4);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), flag);
    NAOS_CHECK_PTR_EQ(option_at(root, 1), count);
    NAOS_CHECK_PTR_EQ(option_at(root, 2), value);
    NAOS_CHECK_PTR_EQ(option_at(root, 3), list);
    NAOS_CHECK_TRUE(flag->attached);
    NAOS_CHECK_TRUE(list->attached);

    NAOS_CHECK_INT_EQ(flag->kind, CLOPTS_PARSER_FLAG);
    NAOS_CHECK_INT_EQ(count->kind, CLOPTS_PARSER_COUNT);
    NAOS_CHECK_INT_EQ(value->kind, CLOPTS_PARSER_VALUE);
    NAOS_CHECK_INT_EQ(list->kind, CLOPTS_PARSER_LIST);
    NAOS_REQUIRE_NOT_NULL(flag->long_name);
    NAOS_CHECK_STR_EQ(flag->long_name, "force");
    NAOS_CHECK_INT_EQ(flag->short_name, 'f');
    NAOS_CHECK_NULL(value->long_name);
    NAOS_CHECK_INT_EQ(value->short_name, 'o');
    NAOS_CHECK_INT_EQ(list->short_name, '\0');

    /* commands in order of addition */
    NAOS_REQUIRE_UINT_EQ(program->commands.len, 2);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), build);
    NAOS_CHECK_PTR_EQ(command_at(program, 1), copy);
    NAOS_CHECK_PTR_EQ(build->program, program);
    NAOS_REQUIRE_NOT_NULL(build->name);
    NAOS_CHECK_STR_EQ(build->name, "build");
    NAOS_CHECK_STR_EQ(copy->name, "copy");
    NAOS_CHECK_NULL(build->summary);
    NAOS_CHECK_NULL(build->description);
    NAOS_CHECK_TRUE(build->handler == NULL);

    /* command options and positionals */
    NAOS_REQUIRE_UINT_EQ(build->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(build, 0), local);
    NAOS_CHECK_TRUE(local->attached);
    NAOS_CHECK_UINT_EQ(build->positionals.len, 0);

    NAOS_REQUIRE_UINT_EQ(copy->positionals.len, 2);
    NAOS_CHECK_PTR_EQ(positional_at(copy, 0), src);
    NAOS_CHECK_PTR_EQ(positional_at(copy, 1), dst);
    NAOS_CHECK_TRUE(src->attached);
    NAOS_CHECK_TRUE(dst->attached);
    NAOS_REQUIRE_NOT_NULL(src->name);
    NAOS_CHECK_STR_EQ(src->name, "src");
    NAOS_CHECK_UINT_EQ(copy->options.len, 0);

    clopts_program_free(program);
}

NAOS_TEST(add_command_returns_the_handle)
{
    clopts_program_t *program;
    clopts_command_t *build = &sentinel_command;

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    EXPECT_OK(clopts_program_add_command(program, "build", &build));
    NAOS_REQUIRE_NOT_NULL(build);
    NAOS_REQUIRE_TRUE(build != &sentinel_command);
    NAOS_REQUIRE_UINT_EQ(program->commands.len, 1);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), build);
    NAOS_CHECK_TRUE(build != clopts_program_root(program));
    clopts_program_free(program);
}

NAOS_TEST(root_takes_positionals_and_handler)
{
    int userdata = 7;
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_positional_t *file;

    program = test_decl_program("filt");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

    file = test_decl_add_positional(root, "file");
    EXPECT_OK(clopts_command_set_handler(root, dummy_handler, &userdata));
    EXPECT_OK(clopts_command_set_description(root, "Filters lines."));

    NAOS_REQUIRE_NOT_NULL(file);
    NAOS_REQUIRE_UINT_EQ(root->positionals.len, 1);
    NAOS_CHECK_PTR_EQ(positional_at(root, 0), file);
    NAOS_CHECK_TRUE(root->handler == dummy_handler);
    NAOS_CHECK_PTR_EQ(root->userdata, &userdata);
    NAOS_REQUIRE_NOT_NULL(root->description);
    NAOS_CHECK_STR_EQ(root->description, "Filters lines.");

    clopts_program_free(program);
}

/* ---- string copies --------------------------------------------------------- */

NAOS_TEST(strings_are_copied)
{
    char cmd_name[] = "build";
    char long_name[] = "output";
    char pos_name[] = "src";
    char text[32];
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_positional_t *src;

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

    build = test_decl_command(program, cmd_name);
    output = make(CLOPTS_OPTION_VALUE, long_name, 'o');
    src = make_positional(pos_name);
    NAOS_REQUIRE_NOT_NULL(build);
    memset(cmd_name, 'X', sizeof(cmd_name) - 1);
    memset(long_name, 'X', sizeof(long_name) - 1);
    memset(pos_name, 'X', sizeof(pos_name) - 1);

    strcpy(text, "1.2.3");
    EXPECT_OK(clopts_program_set_version(program, text));
    strcpy(text, "XXXXX");
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

    NAOS_CHECK_STR_EQ(build->name, "build");
    NAOS_CHECK_STR_EQ(output->long_name, "output");
    NAOS_CHECK_STR_EQ(src->name, "src");
    NAOS_REQUIRE_NOT_NULL(program->version);
    NAOS_CHECK_STR_EQ(program->version, "1.2.3");
    NAOS_REQUIRE_NOT_NULL(build->summary);
    NAOS_CHECK_STR_EQ(build->summary, "Builds.");
    NAOS_REQUIRE_NOT_NULL(build->description);
    NAOS_CHECK_STR_EQ(build->description, "Builds the tree.");
    NAOS_REQUIRE_NOT_NULL(root->description);
    NAOS_CHECK_STR_EQ(root->description, "Program text.");
    NAOS_REQUIRE_NOT_NULL(output->help);
    NAOS_CHECK_STR_EQ(output->help, "Output file.");
    NAOS_REQUIRE_NOT_NULL(output->value_name);
    NAOS_CHECK_STR_EQ(output->value_name, "FILE");
    NAOS_REQUIRE_NOT_NULL(output->env);
    NAOS_CHECK_STR_EQ(output->env, "TOOL_OUTPUT");
    NAOS_REQUIRE_NOT_NULL(output->default_value);
    NAOS_CHECK_STR_EQ(output->default_value, "a.out");
    NAOS_REQUIRE_NOT_NULL(src->help);
    NAOS_CHECK_STR_EQ(src->help, "Source.");

    clopts_program_free(program);
}

/* A string attribute set from its own current copy is copied before the old copy is released. */
NAOS_TEST(string_set_from_its_own_copy)
{
    clopts_program_t *program;
    clopts_option_t *output;

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    EXPECT_OK(clopts_program_set_version(program, "1.0"));
    EXPECT_OK(clopts_program_set_version(program, program->version));
    NAOS_CHECK_STR_EQ(program->version, "1.0");

    output = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(output, "Output file."));
    EXPECT_OK(clopts_option_set_help(output, output->help));
    NAOS_CHECK_STR_EQ(output->help, "Output file.");
    clopts_option_free(output);
    clopts_program_free(program);
}

/* ---- setting again --------------------------------------------------------- */

NAOS_TEST(setting_again_keeps_the_last_value)
{
    int first = 1;
    int second = 2;
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *value;
    clopts_option_t *list;
    clopts_option_t *count;
    clopts_option_t *flag;
    clopts_positional_t *src;

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    build = test_decl_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(build);
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    list = make(CLOPTS_OPTION_LIST, "include", 'I');
    count = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    flag = make(CLOPTS_OPTION_FLAG, "color", '\0');
    src = make_positional("src");

    EXPECT_OK(clopts_program_set_version(program, "1.0"));
    EXPECT_OK(clopts_program_set_version(program, "2.0"));
    EXPECT_OK(clopts_command_set_summary(build, "first"));
    EXPECT_OK(clopts_command_set_summary(build, "second"));
    EXPECT_OK(clopts_command_set_description(build, "first"));
    EXPECT_OK(clopts_command_set_description(build, "second"));
    EXPECT_OK(clopts_command_set_handler(build, dummy_handler, &first));
    EXPECT_OK(clopts_command_set_handler(build, other_handler, &second));
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
    EXPECT_OK(clopts_option_set_required(list, true));
    EXPECT_OK(clopts_positional_set_help(src, "first"));
    EXPECT_OK(clopts_positional_set_help(src, "second"));
    EXPECT_OK(clopts_positional_set_required(src, true));
    EXPECT_OK(clopts_positional_set_variadic(src, true));

    NAOS_CHECK_STR_EQ(program->version, "2.0");
    NAOS_CHECK_STR_EQ(build->summary, "second");
    NAOS_CHECK_STR_EQ(build->description, "second");
    NAOS_CHECK_TRUE(build->handler == other_handler);
    NAOS_CHECK_PTR_EQ(build->userdata, &second);
    NAOS_CHECK_STR_EQ(value->help, "second");
    NAOS_CHECK_STR_EQ(value->value_name, "B");
    NAOS_CHECK_STR_EQ(value->env, "ENV_B");
    NAOS_CHECK_STR_EQ(value->default_value, "y");
    NAOS_CHECK_INT_EQ(list->delimiter, ':');
    NAOS_CHECK_UINT_EQ(count->count_max, 8);
    NAOS_CHECK_TRUE(flag->negatable);
    NAOS_CHECK_TRUE(list->required);
    NAOS_CHECK_STR_EQ(src->help, "second");
    NAOS_CHECK_TRUE(src->required);
    NAOS_CHECK_TRUE(src->variadic);

    /* NULL, false, '\0' and 0 reset */
    EXPECT_OK(clopts_program_set_version(program, NULL));
    EXPECT_OK(clopts_command_set_summary(build, NULL));
    EXPECT_OK(clopts_command_set_description(build, NULL));
    EXPECT_OK(clopts_command_set_handler(build, NULL, NULL));
    EXPECT_OK(clopts_option_set_help(value, NULL));
    EXPECT_OK(clopts_option_set_value_name(value, NULL));
    EXPECT_OK(clopts_option_set_env(value, NULL));
    EXPECT_OK(clopts_option_set_default(value, NULL));
    EXPECT_OK(clopts_option_set_delimiter(list, '\0'));
    EXPECT_OK(clopts_option_set_count_max(count, 0));
    EXPECT_OK(clopts_option_set_negatable(flag, false));
    EXPECT_OK(clopts_option_set_required(list, false));
    EXPECT_OK(clopts_positional_set_help(src, NULL));
    EXPECT_OK(clopts_positional_set_variadic(src, false));
    EXPECT_OK(clopts_positional_set_required(src, false));

    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_NULL(build->summary);
    NAOS_CHECK_NULL(build->description);
    NAOS_CHECK_TRUE(build->handler == NULL);
    NAOS_CHECK_NULL(build->userdata);
    NAOS_CHECK_NULL(value->help);
    NAOS_CHECK_NULL(value->value_name);
    NAOS_CHECK_NULL(value->env);
    NAOS_CHECK_NULL(value->default_value);
    NAOS_CHECK_INT_EQ(list->delimiter, '\0');
    NAOS_CHECK_UINT_EQ(count->count_max, 0);
    NAOS_CHECK_FALSE(flag->negatable);
    NAOS_CHECK_FALSE(list->required);
    NAOS_CHECK_NULL(src->help);
    NAOS_CHECK_FALSE(src->variadic);
    NAOS_CHECK_FALSE(src->required);

    /* the attach keeps the attributes as they are */
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, value));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, list));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, count));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach(root, flag));
    NAOS_REQUIRE_NOT_NULL(test_decl_attach_positional(build, src));
    NAOS_CHECK_NULL(value->help);
    NAOS_CHECK_INT_EQ(list->delimiter, '\0');
    NAOS_CHECK_FALSE(src->required);

    clopts_program_free(program);
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

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

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

    NAOS_REQUIRE_UINT_EQ(root->options.len, 100);
    NAOS_REQUIRE_UINT_EQ(root->positionals.len, 100);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), first);
    NAOS_CHECK_TRUE(first->attached);
    NAOS_CHECK_INT_EQ(first->kind, CLOPTS_PARSER_VALUE);
    NAOS_CHECK_STR_EQ(first->long_name, "o0");
    NAOS_CHECK_INT_EQ(first->short_name, 'a');
    NAOS_CHECK_STR_EQ(first->help, "first help");
    NAOS_CHECK_STR_EQ(option_at(root, 99)->long_name, "o99");
    NAOS_CHECK_STR_EQ(positional_at(root, 99)->name, "p99");

    clopts_program_free(program);
}

NAOS_TEST(hundred_commands_keep_the_first_handle)
{
    char name[16];
    clopts_program_t *program;
    clopts_command_t *first;
    clopts_command_t *command;
    int i;

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    first = test_decl_command(program, "c0");
    NAOS_REQUIRE_NOT_NULL(first);
    EXPECT_OK(clopts_command_set_summary(first, "first summary"));
    NAOS_REQUIRE_NOT_NULL(test_decl_add(first, CLOPTS_OPTION_FLAG, "force", 'f'));
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "c%d", i);
        NAOS_NOTE("i=%d", i);
        command = test_decl_command(program, name);
        NAOS_REQUIRE_NOT_NULL(command);
        NAOS_CHECK_PTR_EQ(command_at(program, (size_t)i), command);
    }

    NAOS_REQUIRE_UINT_EQ(program->commands.len, 100);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), first);
    NAOS_CHECK_STR_EQ(first->name, "c0");
    NAOS_CHECK_STR_EQ(first->summary, "first summary");
    NAOS_CHECK_UINT_EQ(first->options.len, 1);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(program, "c99"), command_at(program, 99));

    clopts_program_free(program);
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
        if (error == CLOPTS_DECL_NONE)
        {
            return n - 1;
        }
        NAOS_CHECK_INT_EQ(error, CLOPTS_DECL_OUT_OF_MEMORY);
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

static clopts_decl_error_t oom_program_new(oom_call_t *c)
{
    c->program_out = &sentinel_program;
    return clopts_program_new("tool", &c->program_out);
}

static void oom_program_new_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->program_out);
}

static clopts_decl_error_t oom_set_version(oom_call_t *c)
{
    return clopts_program_set_version(c->program, "1.0");
}

static void oom_set_version_unchanged(oom_call_t *c)
{
    NAOS_CHECK_STR_EQ(c->program->version, "0.9");
}

static clopts_decl_error_t oom_add_command(oom_call_t *c)
{
    c->command_out = &sentinel_command;
    return clopts_program_add_command(c->program, "build", &c->command_out);
}

static void oom_add_command_unchanged(oom_call_t *c)
{
    NAOS_CHECK_NULL(c->command_out);
    NAOS_CHECK_UINT_EQ(c->program->commands.len, 0);
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
    NAOS_CHECK_STR_EQ(c->command->summary, "old");
    NAOS_CHECK_STR_EQ(c->command->description, "old");
}

static clopts_decl_error_t oom_option_new(oom_call_t *c)
{
    c->option_out = &sentinel_option;
    return clopts_option_new(CLOPTS_OPTION_VALUE, "output", 'o', &c->option_out);
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
    NAOS_CHECK_STR_EQ(c->option->help, "old");
    NAOS_CHECK_STR_EQ(c->option->value_name, "old");
    NAOS_CHECK_STR_EQ(c->option->env, "old");
    NAOS_CHECK_STR_EQ(c->option->default_value, "old");
}

static clopts_decl_error_t oom_add_option(oom_call_t *c)
{
    return clopts_command_add_option(c->command, c->option);
}

static void oom_add_option_unchanged(oom_call_t *c)
{
    NAOS_CHECK_FALSE(c->option->attached);
    NAOS_CHECK_UINT_EQ(c->command->options.len, 0);
}

static clopts_decl_error_t oom_positional_new(oom_call_t *c)
{
    c->positional_out = &sentinel_positional;
    return clopts_positional_new("src", &c->positional_out);
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
    NAOS_CHECK_STR_EQ(c->positional->help, "old");
}

static clopts_decl_error_t oom_add_positional(oom_call_t *c)
{
    return clopts_command_add_positional(c->command, c->positional);
}

static void oom_add_positional_unchanged(oom_call_t *c)
{
    NAOS_CHECK_FALSE(c->positional->attached);
    NAOS_CHECK_UINT_EQ(c->command->positionals.len, 0);
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

    c.call = oom_program_new;
    c.unchanged = oom_program_new_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_NOT_NULL(c.program_out);
    NAOS_REQUIRE_TRUE(c.program_out != &sentinel_program);
    c.program = c.program_out;

    EXPECT_OK(clopts_program_set_version(c.program, "0.9"));
    c.call = oom_set_version;
    c.unchanged = oom_set_version_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_STR_EQ(c.program->version, "1.0");

    c.call = oom_add_command;
    c.unchanged = oom_add_command_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_NOT_NULL(c.command_out);
    NAOS_REQUIRE_UINT_EQ(c.program->commands.len, 1);
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
    NAOS_REQUIRE_TRUE(c.option_out != &sentinel_option);
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
    NAOS_CHECK_STR_EQ(c.option->default_value, "new");

    c.call = oom_add_option;
    c.unchanged = oom_add_option_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_TRUE(c.option->attached);
    NAOS_REQUIRE_UINT_EQ(c.command->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(c.command, 0), c.option);

    /* the attach of a negatable flag allocates for its no-x lookup too */
    color = make(CLOPTS_OPTION_FLAG, "color", '\0');
    EXPECT_OK(clopts_option_set_negatable(color, true));
    c.option = color;
    c.command = clopts_program_root(c.program);
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_TRUE(color->attached);
    c.command = c.command_out;

    c.call = oom_positional_new;
    c.unchanged = oom_positional_new_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_REQUIRE_NOT_NULL(c.positional_out);
    NAOS_REQUIRE_TRUE(c.positional_out != &sentinel_positional);
    c.positional = c.positional_out;

    EXPECT_OK(clopts_positional_set_help(c.positional, "old"));
    c.call = oom_positional_help;
    c.unchanged = oom_positional_help_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_STR_EQ(c.positional->help, "new");

    c.call = oom_add_positional;
    c.unchanged = oom_add_positional_unchanged;
    NAOS_CHECK_UINT_GT(fail_each_point(&c), 0);
    NAOS_CHECK_TRUE(c.positional->attached);
    NAOS_REQUIRE_UINT_EQ(c.command->positionals.len, 1);

    clopts_program_free(c.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * Builds a small program through every allocating builder function, with
 * every call checked by test_decl_check() or made by a helper, so that
 * test_decl_failures counts the failed ones.
 */
static void build_sample(clopts_program_t *program)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *include;
    clopts_option_t *color;
    clopts_positional_t *src;

    root = clopts_program_root(program);
    test_decl_check(clopts_program_set_version(program, "1.0"));
    test_decl_check(clopts_command_set_description(root, "A tool."));
    test_decl_add(root, CLOPTS_OPTION_COUNT, "verbose", 'v');
    color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    test_decl_check(clopts_option_set_negatable(color, true));
    test_decl_attach(root, color);
    build = test_decl_command(program, "build");
    test_decl_check(clopts_command_set_summary(build, "Builds."));
    test_decl_check(clopts_command_set_description(build, "Builds the tree."));
    output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    test_decl_check(clopts_option_set_help(output, "Output file."));
    test_decl_check(clopts_option_set_value_name(output, "FILE"));
    test_decl_check(clopts_option_set_env(output, "TOOL_OUTPUT"));
    test_decl_check(clopts_option_set_default(output, "a.out"));
    test_decl_attach(build, output);
    include = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    test_decl_check(clopts_option_set_delimiter(include, ','));
    test_decl_attach(build, include);
    test_decl_add(build, CLOPTS_OPTION_FLAG, "release", '\0');
    src = test_decl_positional("src");
    test_decl_check(clopts_positional_set_help(src, "Source."));
    test_decl_attach_positional(build, src);
}

/*
 * Moves the failing fault point fail_at = 1, 2, ... over test_decl_program()
 * and build_sample() until a run passes no failing fault point
 * (test_alloc.calls < fail_at). fail_at = 1 fails test_decl_program()
 * itself, which then returns NULL. A run that passes a failing fault point
 * has failed calls; whatever was made is released by clopts_program_free()
 * (or, for a failed attach, by the helper).
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
        program = test_decl_program("tool");
        if (program == NULL)
        {
            /* Only the first fault point is in test_decl_program(). */
            NAOS_CHECK_UINT_EQ(fail_at, 1);
            NAOS_CHECK_UINT_EQ(test_alloc.calls, 1);
            NAOS_CHECK_INT_EQ(test_decl_last, CLOPTS_DECL_OUT_OF_MEMORY);
            NAOS_CHECK_TRUE(test_alloc_end());
            continue;
        }

        build_sample(program);
        if (test_alloc.calls < fail_at)
        {
            NAOS_CHECK_UINT_EQ(test_decl_failures, 0);
            succeeded = true;
        }
        else
        {
            NAOS_CHECK_UINT_GT(test_decl_failures, 0);
        }

        clopts_program_free(program);
        NAOS_CHECK_TRUE(test_alloc_end());
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(fail_at, 2);
}

/* ---- release --------------------------------------------------------------- */

/* clopts_program_free() releases every block: commands, attached options and positionals, and their strings. */
NAOS_TEST(program_free_releases_everything)
{
    clopts_program_t *program;

    test_alloc_begin(0);
    test_decl_reset();
    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    build_sample(program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    NAOS_CHECK_INT_GT(test_alloc.live, 1);

    clopts_program_free(program);
    NAOS_CHECK_INT_EQ(test_alloc.live, 0);
    NAOS_CHECK_UINT_EQ(test_alloc.bad_frees, 0);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- freezing ---------------------------------------------------------------- */

NAOS_TEST(parse_freezes_the_program)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *out = &sentinel_command;
    clopts_option_t *flag;
    clopts_option_t *option;
    clopts_positional_t *positional;

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    flag = test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_REQUIRE_NOT_NULL(flag);
    NAOS_CHECK_FALSE(program->frozen);

    parse_name_only(program, "tool", CLOPTS_OK);
    NAOS_CHECK_TRUE(program->frozen);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);

    /* a second parse of a frozen program is fine */
    parse_name_only(program, "tool", CLOPTS_OK);
    NAOS_CHECK_TRUE(program->frozen);

    /* NULL arguments are refused before the frozen check */
    NAOS_CHECK_INT_EQ(clopts_command_add_option(root, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(root, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, "build", NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_program_add_command(NULL, "build", &out), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_NULL(out);

    /* unattached options and positionals belong to no program and stay writable */
    option = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_help(option, "Output file."));
    clopts_option_free(option);
    positional = make_positional("src");
    EXPECT_OK(clopts_positional_set_required(positional, true));
    clopts_positional_free(positional);

    /* an attached one stays read-only */
    NAOS_CHECK_INT_EQ(clopts_option_set_help(flag, "help"), CLOPTS_DECL_ALREADY_ATTACHED);

    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    clopts_program_free(program);
}

/* The program is frozen before the result is allocated: a parse that runs out of memory freezes it too. */
NAOS_TEST(parse_out_of_memory_freezes_the_program)
{
    clopts_program_t *program;

    test_alloc_begin(0);
    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_NOT_NULL(test_decl_add(clopts_program_root(program), CLOPTS_OPTION_FLAG, "force", 'f'));

    test_alloc_set_fail_at(test_alloc.calls + 1);
    parse_name_only(program, "tool", CLOPTS_OUT_OF_MEMORY);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_TRUE(program->frozen);

    clopts_program_free(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(builder_after_parse_is_frozen)
{
#ifdef NDEBUG
    int userdata = 1;
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *out;
    clopts_option_t *flag;
    clopts_option_t *option;
    clopts_positional_t *positional;

    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    build = test_decl_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(build);
    flag = test_decl_add(root, CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_REQUIRE_NOT_NULL(flag);

    parse_name_only(program, "tool", CLOPTS_NO_COMMAND);
    NAOS_REQUIRE_TRUE(program->frozen);

    NAOS_CHECK_INT_EQ(clopts_program_set_version(program, "1.0"), CLOPTS_DECL_FROZEN);
    NAOS_CHECK_INT_EQ(clopts_program_set_version(program, NULL), CLOPTS_DECL_FROZEN);
    out = &sentinel_command;
    NAOS_CHECK_INT_EQ(clopts_program_add_command(program, "copy", &out), CLOPTS_DECL_FROZEN);
    NAOS_CHECK_NULL(out);
    NAOS_CHECK_INT_EQ(clopts_command_set_summary(build, "summary"), CLOPTS_DECL_FROZEN);
    NAOS_CHECK_INT_EQ(clopts_command_set_description(root, "description"), CLOPTS_DECL_FROZEN);
    NAOS_CHECK_INT_EQ(clopts_command_set_handler(build, dummy_handler, &userdata), CLOPTS_DECL_FROZEN);

    option = make(CLOPTS_OPTION_FLAG, "other", 'o');
    NAOS_CHECK_INT_EQ(clopts_command_add_option(root, option), CLOPTS_DECL_FROZEN);
    NAOS_CHECK_INT_EQ(clopts_command_add_option(build, option), CLOPTS_DECL_FROZEN);
    NAOS_CHECK_FALSE(option->attached);
    clopts_option_free(option);
    positional = make_positional("file");
    NAOS_CHECK_INT_EQ(clopts_command_add_positional(build, positional), CLOPTS_DECL_FROZEN);
    NAOS_CHECK_FALSE(positional->attached);
    clopts_positional_free(positional);

    /* the frozen check comes before the attachment */
    NAOS_CHECK_INT_EQ(clopts_command_add_option(root, flag), CLOPTS_DECL_FROZEN);

    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    NAOS_CHECK_UINT_EQ(build->options.len, 0);
    NAOS_CHECK_UINT_EQ(build->positionals.len, 0);
    NAOS_CHECK_UINT_EQ(program->commands.len, 1);
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_NULL(build->summary);
    NAOS_CHECK_NULL(root->description);
    NAOS_CHECK_TRUE(build->handler == NULL);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);

    clopts_program_free(program);
#else
    NAOS_SKIP("needs NDEBUG: a builder call on a frozen program is caught by assert");
#endif
}

/* ---- shared fixtures (test_fixtures.c) ------------------------------------- */

/* Expected attributes of one option, as declared by the first-version fixtures. */
typedef struct expected_option
{
    const clopts_command_t *scope;
    size_t index;
    clopts_parser_kind_t kind;
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
} expected_option_t;

/* Checks every model field of option, and that it is attached at index of its scope. */
static void check_option(const clopts_option_t *option, const expected_option_t *expected)
{
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_TRUE(option->attached);
    NAOS_REQUIRE_UINT_GT(expected->scope->options.len, expected->index);
    NAOS_CHECK_PTR_EQ(option_at(expected->scope, expected->index), option);
    NAOS_CHECK_INT_EQ(option->kind, expected->kind);
    if (expected->long_name == NULL)
    {
        NAOS_CHECK_NULL(option->long_name);
    }
    else
    {
        NAOS_CHECK_STR_EQ(option->long_name, expected->long_name);
    }
    NAOS_CHECK_INT_EQ(option->short_name, expected->short_name);
    NAOS_CHECK_STR_EQ(option->help, expected->help);
    if (expected->value_name == NULL)
    {
        NAOS_CHECK_NULL(option->value_name);
    }
    else
    {
        NAOS_CHECK_STR_EQ(option->value_name, expected->value_name);
    }
    if (expected->default_value == NULL)
    {
        NAOS_CHECK_NULL(option->default_value);
    }
    else
    {
        NAOS_CHECK_STR_EQ(option->default_value, expected->default_value);
    }
    if (expected->env == NULL)
    {
        NAOS_CHECK_NULL(option->env);
    }
    else
    {
        NAOS_CHECK_STR_EQ(option->env, expected->env);
    }
    NAOS_CHECK_INT_EQ(option->delimiter, expected->delimiter);
    NAOS_CHECK_UINT_EQ(option->count_max, expected->count_max);
    NAOS_CHECK_INT_EQ(option->required, expected->required);
    NAOS_CHECK_INT_EQ(option->negatable, expected->negatable);
}

/* Checks the model fields of positional, and that it is attached at index of scope. */
static void check_positional(const clopts_positional_t *positional, const clopts_command_t *scope, size_t index,
                             const char *name, const char *help, bool required, bool variadic)
{
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_TRUE(positional->attached);
    NAOS_REQUIRE_UINT_GT(scope->positionals.len, index);
    NAOS_CHECK_PTR_EQ(positional_at(scope, index), positional);
    NAOS_CHECK_STR_EQ(positional->name, name);
    NAOS_CHECK_STR_EQ(positional->help, help);
    NAOS_CHECK_INT_EQ(positional->required, required);
    NAOS_CHECK_INT_EQ(positional->variadic, variadic);
}

/* Checks a subcommand: its place in program, its name, summary and handler. */
static void check_command(const clopts_command_t *command, const clopts_program_t *program, size_t index,
                          const char *name, const char *summary, clopts_handler_fn handler, void *userdata)
{
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_PTR_EQ(command->program, program);
    NAOS_REQUIRE_UINT_GT(program->commands.len, index);
    NAOS_CHECK_PTR_EQ(command_at(program, index), command);
    NAOS_CHECK_STR_EQ(command->name, name);
    NAOS_CHECK_STR_EQ(command->summary, summary);
    NAOS_CHECK_NULL(command->description);
    NAOS_CHECK_TRUE(command->handler == handler);
    NAOS_CHECK_PTR_EQ(command->userdata, userdata);
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
    NAOS_REQUIRE_PTR_EQ(root, &program->root);
    NAOS_CHECK_STR_EQ(root->name, "tool");
    NAOS_CHECK_STR_EQ(root->description, "A fictional tool with subcommands.");
    NAOS_CHECK_NULL(root->summary);
    NAOS_CHECK_TRUE(root->handler == NULL);
    NAOS_REQUIRE_NOT_NULL(program->version);
    NAOS_CHECK_STR_EQ(program->version, "1.2.3");
    NAOS_CHECK_UINT_EQ(root->options.len, 3);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 0);

    {
        const expected_option_t verbose = {root, 0, CLOPTS_PARSER_COUNT, "verbose", 'v', "Increase verbosity.",
                                           NULL, NULL, NULL, '\0', 0, false, false};
        const expected_option_t color = {root, 1, CLOPTS_PARSER_FLAG, "color", '\0', "Colorize the output.",
                                         NULL, NULL, NULL, '\0', 0, false, true};
        const expected_option_t config = {root, 2, CLOPTS_PARSER_VALUE, "config", '\0', "Configuration file.",
                                          "PATH", NULL, "TOOL_CONFIG", '\0', 0, false, false};

        check_option(f.verbose, &verbose);
        check_option(f.color, &color);
        check_option(f.config, &config);
    }

    NAOS_REQUIRE_UINT_EQ(program->commands.len, 3);
    check_command(f.build, program, 0, "build", "Build a source.", fixture_handler, &fixture_build_handler_record);
    check_command(f.copy, program, 1, "copy", "Copy a file.", NULL, NULL);
    check_command(f.cat, program, 2, "cat", "Show files.", NULL, NULL);

    NAOS_CHECK_UINT_EQ(f.build->options.len, 3);
    NAOS_CHECK_UINT_EQ(f.build->positionals.len, 2);
    {
        const expected_option_t output = {f.build, 0, CLOPTS_PARSER_VALUE, "output", 'o', "Output file.",
                                          "FILE", NULL, NULL, '\0', 0, true, false};
        const expected_option_t keys = {f.build, 1, CLOPTS_PARSER_LIST, "keys", 'k', "Keys to build.",
                                        "KEY", NULL, NULL, ',', 0, false, false};
        const expected_option_t jobs = {f.build, 2, CLOPTS_PARSER_COUNT, NULL, 'j', "Add a parallel job.",
                                        NULL, NULL, NULL, '\0', 8, false, false};

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
        const expected_option_t tags = {f.cat, 0, CLOPTS_PARSER_LIST, "tags", '\0', "Tags to show.",
                                        "TAG", "a,b", "TOOL_TAGS", ',', 0, false, false};

        check_option(f.tags, &tags);
    }
    check_positional(f.cat_file, f.cat, 0, "FILE", "Files to show.", false, true);

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
    NAOS_REQUIRE_PTR_EQ(root, &program->root);
    NAOS_CHECK_STR_EQ(root->name, "filt");
    NAOS_CHECK_STR_EQ(root->description, "A fictional filter without subcommands.");
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    NAOS_CHECK_TRUE(root->handler == fixture_handler);
    NAOS_CHECK_PTR_EQ(root->userdata, &fixture_filt_handler_record);
    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 1);
    {
        const expected_option_t lines = {root, 0, CLOPTS_PARSER_VALUE, "lines", 'n', "Number of lines.",
                                         "N", "10", "FILT_LINES", '\0', 0, false, false};

        check_option(f.lines, &lines);
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
    NAOS_REQUIRE_PTR_EQ(root, &program->root);
    NAOS_CHECK_STR_EQ(root->name, "strict");
    NAOS_CHECK_STR_EQ(root->description, "A fictional program with a required global option.");
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_TRUE(root->handler == NULL);
    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 0);
    {
        const expected_option_t token = {root, 0, CLOPTS_PARSER_VALUE, "token", '\0', "Access token.",
                                         "TOKEN", NULL, NULL, '\0', 0, true, false};

        check_option(f.token, &token);
    }

    NAOS_REQUIRE_UINT_EQ(program->commands.len, 1);
    check_command(f.run, program, 0, "run", "Run it.", NULL, NULL);
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

NAOS_TEST(fixture_out_of_memory_is_reported_and_released)
{
    fixture_tool_t tool;
    size_t total;

    /* number of fault points of a full assembly, test_decl_program() included */
    test_alloc_begin(0);
    fixture_tool_init(&tool);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_REQUIRE_TRUE(tool.ok);
    total = test_alloc.calls;
    fixture_tool_free(&tool);
    NAOS_CHECK_TRUE(test_alloc_end());
    NAOS_REQUIRE_UINT_GT(total, 2);

    /* the first fault point is test_decl_program() itself */
    test_alloc_begin(1);
    fixture_tool_init(&tool);
    NAOS_CHECK_NULL(tool.program);
    NAOS_CHECK_FALSE(tool.ok);
    NAOS_CHECK_NULL(tool.root);
    NAOS_CHECK_NULL(tool.build);
    NAOS_CHECK_NULL(tool.verbose);
    fixture_tool_free(&tool);
    NAOS_CHECK_TRUE(test_alloc_end());

    /* the first fault point after it and the last one fail */
    test_alloc_begin(2);
    fixture_tool_init(&tool);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_CHECK_FALSE(tool.ok);
    fixture_tool_free(&tool);
    NAOS_CHECK_TRUE(test_alloc_end());

    test_alloc_begin(total);
    fixture_tool_init(&tool);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_CHECK_FALSE(tool.ok);
    NAOS_CHECK_UINT_EQ(test_alloc.calls, total);
    fixture_tool_free(&tool);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(fixture_handler_records_calls)
{
    fixture_handler_record_t record;
    int marker = 0;

    fixture_handler_reset(&record, 7);
    NAOS_CHECK_INT_EQ(record.calls, 0);
    NAOS_CHECK_NULL(record.last_result);
    NAOS_CHECK_NULL(record.last_userdata);

    NAOS_CHECK_INT_EQ(fixture_handler((const clopts_result_t *)&marker, &record), 7);
    NAOS_CHECK_INT_EQ(fixture_handler((const clopts_result_t *)&marker, &record), 7);
    NAOS_CHECK_INT_EQ(record.calls, 2);
    NAOS_CHECK_PTR_EQ(record.last_result, (const clopts_result_t *)&marker);
    NAOS_CHECK_PTR_EQ(record.last_userdata, &record);
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
    NAOS_ADD_TEST(program_new_embeds_root_and_copies_name),
    NAOS_ADD_TEST(new_option_and_positional_start_reset_and_unattached),
    NAOS_ADD_TEST(unattached_objects_are_released_by_their_free),
    NAOS_ADD_TEST(attach_links_objects_in_order),
    NAOS_ADD_TEST(add_command_returns_the_handle),
    NAOS_ADD_TEST(root_takes_positionals_and_handler),
    NAOS_ADD_TEST(strings_are_copied),
    NAOS_ADD_TEST(string_set_from_its_own_copy),
    NAOS_ADD_TEST(setting_again_keeps_the_last_value),
    NAOS_ADD_TEST(hundred_options_keep_the_first_handle),
    NAOS_ADD_TEST(hundred_commands_keep_the_first_handle),
    NAOS_ADD_TEST(out_of_memory_changes_nothing),
    NAOS_ADD_TEST(every_failure_point_is_released),
    NAOS_ADD_TEST(program_free_releases_everything),
    NAOS_ADD_TEST(parse_freezes_the_program),
    NAOS_ADD_TEST(parse_out_of_memory_freezes_the_program),
    NAOS_ADD_TEST(builder_after_parse_is_frozen),
    NAOS_ADD_TEST(fixtures_are_built_completely),
    NAOS_ADD_TEST(fixture_tool_declaration),
    NAOS_ADD_TEST(fixture_filt_declaration),
    NAOS_ADD_TEST(fixture_strict_declaration),
    NAOS_ADD_TEST(fixtures_release_every_block),
    NAOS_ADD_TEST(fixture_out_of_memory_is_reported_and_released),
    NAOS_ADD_TEST(fixture_handler_records_calls),
    NAOS_ADD_TEST(fixture_env_lends_from_its_buffers),
    NAOS_ADD_TEST(fixture_env_fail_next_is_one_shot),
    NAOS_ADD_TEST(fixture_env_clobber_overwrites_lent_values)
)
