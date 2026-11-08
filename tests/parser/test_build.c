/*
 * Suite build: the builder functions, NULL safety, the first recorded error,
 * string copies, freezing, handle stability, the release of a program and
 * the entry of clopts_program_validate(), and the contents of the shared
 * fixtures (test_fixtures.c) compared with the first-version declarations.
 *
 * The declaration rules of the immediate check are tested in the decl suite
 * (task 6.1). Stored values are read through the internal model.
 */

#include "parser/parser_internal.h"
#include "test_fixtures.h"

#include <naos/naos.h>

#include <stdio.h>
#include <string.h>

/* Returns a probe with the given fail_at and zero counters. */
static clopts_parser_probe_t make_probe(size_t fail_at)
{
    clopts_parser_probe_t probe;

    probe.fail_at = fail_at;
    probe.calls = 0;
    probe.live = 0;
    return probe;
}

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

/* ---- NULL handles -------------------------------------------------------- */

NAOS_TEST(null_handles_do_nothing)
{
    clopts_program_free(NULL);
    clopts_program_set_version(NULL, "1.0");
    NAOS_CHECK_NULL(clopts_program_root(NULL));
    NAOS_CHECK_NULL(clopts_program_add_command(NULL, "build"));

    clopts_command_set_summary(NULL, "summary");
    clopts_command_set_description(NULL, "description");
    clopts_command_set_handler(NULL, dummy_handler, NULL);
    NAOS_CHECK_NULL(clopts_command_add_flag(NULL, "flag", 'f'));
    NAOS_CHECK_NULL(clopts_command_add_count(NULL, "count", 'c'));
    NAOS_CHECK_NULL(clopts_command_add_value(NULL, "value", 'v'));
    NAOS_CHECK_NULL(clopts_command_add_list(NULL, "list", 'l'));
    NAOS_CHECK_NULL(clopts_command_add_positional(NULL, "file"));

    clopts_option_set_help(NULL, "help");
    clopts_option_set_value_name(NULL, "NAME");
    clopts_option_set_env(NULL, "ENV");
    clopts_option_set_default(NULL, "default");
    clopts_option_set_required(NULL, true);
    clopts_option_set_negatable(NULL, true);
    clopts_option_set_delimiter(NULL, ',');
    clopts_option_set_count_max(NULL, 3);

    clopts_positional_set_help(NULL, "help");
    clopts_positional_set_required(NULL, true);
    clopts_positional_set_variadic(NULL, true);
}

NAOS_TEST(free_null_program_does_nothing)
{
    clopts_program_free(NULL);
    NAOS_PASS();
}

/* ---- program and root -------------------------------------------------- */

NAOS_TEST(program_new_embeds_root_and_copies_name)
{
    char name[] = "tool";
    clopts_program_t *program;
    clopts_command_t *root;

    program = clopts_program_new(name);
    NAOS_REQUIRE_NOT_NULL(program);
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
    NAOS_CHECK_UINT_EQ(root->options.len, 0);
    NAOS_CHECK_UINT_EQ(root->positionals.len, 0);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_FALSE(program->frozen);
    NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OK);
    NAOS_CHECK_INT_EQ(program->error.kind, CLOPTS_DECL_NONE);
    NAOS_CHECK_NULL(program->probe);

    clopts_program_free(program);
}

/* ---- adders ------------------------------------------------------------ */

NAOS_TEST(adders_link_objects_in_order)
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

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

    flag = clopts_command_add_flag(root, "force", 'f');
    count = clopts_command_add_count(root, "verbose", 'v');
    value = clopts_command_add_value(root, NULL, 'o');
    list = clopts_command_add_list(root, "include", '\0');
    build = clopts_program_add_command(program, "build");
    copy = clopts_program_add_command(program, "copy");
    local = clopts_command_add_flag(build, "release", 'r');
    src = clopts_command_add_positional(copy, "src");
    dst = clopts_command_add_positional(copy, "dst");

    NAOS_REQUIRE_NOT_NULL(flag);
    NAOS_REQUIRE_NOT_NULL(count);
    NAOS_REQUIRE_NOT_NULL(value);
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_REQUIRE_NOT_NULL(build);
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_REQUIRE_NOT_NULL(local);
    NAOS_REQUIRE_NOT_NULL(src);
    NAOS_REQUIRE_NOT_NULL(dst);
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);

    /* root options in order of addition */
    NAOS_REQUIRE_UINT_EQ(root->options.len, 4);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), flag);
    NAOS_CHECK_PTR_EQ(option_at(root, 1), count);
    NAOS_CHECK_PTR_EQ(option_at(root, 2), value);
    NAOS_CHECK_PTR_EQ(option_at(root, 3), list);

    NAOS_CHECK_INT_EQ(flag->kind, CLOPTS_PARSER_FLAG);
    NAOS_CHECK_INT_EQ(count->kind, CLOPTS_PARSER_COUNT);
    NAOS_CHECK_INT_EQ(value->kind, CLOPTS_PARSER_VALUE);
    NAOS_CHECK_INT_EQ(list->kind, CLOPTS_PARSER_LIST);
    NAOS_CHECK_UINT_EQ(flag->index, 0);
    NAOS_CHECK_UINT_EQ(count->index, 1);
    NAOS_CHECK_UINT_EQ(value->index, 2);
    NAOS_CHECK_UINT_EQ(list->index, 3);
    NAOS_CHECK_PTR_EQ(flag->program, program);
    NAOS_CHECK_PTR_EQ(flag->scope, root);
    NAOS_CHECK_PTR_EQ(list->scope, root);

    NAOS_REQUIRE_NOT_NULL(flag->long_name);
    NAOS_CHECK_STR_EQ(flag->long_name, "force");
    NAOS_CHECK_INT_EQ(flag->short_name, 'f');
    NAOS_CHECK_NULL(value->long_name);
    NAOS_CHECK_INT_EQ(value->short_name, 'o');
    NAOS_CHECK_INT_EQ(list->short_name, '\0');

    /* new options start with every attribute reset */
    NAOS_CHECK_NULL(value->help);
    NAOS_CHECK_NULL(value->value_name);
    NAOS_CHECK_NULL(value->default_value);
    NAOS_CHECK_NULL(value->env);
    NAOS_CHECK_INT_EQ(list->delimiter, '\0');
    NAOS_CHECK_UINT_EQ(count->count_max, 0);
    NAOS_CHECK_FALSE(value->required);
    NAOS_CHECK_FALSE(flag->negatable);

    /* commands in order of addition */
    NAOS_REQUIRE_UINT_EQ(program->commands.len, 2);
    NAOS_CHECK_PTR_EQ(command_at(program, 0), build);
    NAOS_CHECK_PTR_EQ(command_at(program, 1), copy);
    NAOS_CHECK_PTR_EQ(build->program, program);
    NAOS_REQUIRE_NOT_NULL(build->name);
    NAOS_CHECK_STR_EQ(build->name, "build");
    NAOS_CHECK_STR_EQ(copy->name, "copy");
    NAOS_CHECK_NULL(build->summary);
    NAOS_CHECK_TRUE(build->handler == NULL);

    /* command options and positionals */
    NAOS_REQUIRE_UINT_EQ(build->options.len, 1);
    NAOS_CHECK_PTR_EQ(option_at(build, 0), local);
    NAOS_CHECK_PTR_EQ(local->scope, build);
    NAOS_CHECK_PTR_EQ(local->program, program);
    NAOS_CHECK_UINT_EQ(local->index, 0);

    NAOS_REQUIRE_UINT_EQ(copy->positionals.len, 2);
    NAOS_CHECK_PTR_EQ(positional_at(copy, 0), src);
    NAOS_CHECK_PTR_EQ(positional_at(copy, 1), dst);
    NAOS_CHECK_UINT_EQ(src->index, 0);
    NAOS_CHECK_UINT_EQ(dst->index, 1);
    NAOS_CHECK_PTR_EQ(src->scope, copy);
    NAOS_CHECK_PTR_EQ(src->program, program);
    NAOS_REQUIRE_NOT_NULL(src->name);
    NAOS_CHECK_STR_EQ(src->name, "src");
    NAOS_CHECK_NULL(src->help);
    NAOS_CHECK_FALSE(src->required);
    NAOS_CHECK_FALSE(src->variadic);

    clopts_program_free(program);
}

NAOS_TEST(root_takes_positionals_and_handler)
{
    int userdata = 7;
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_positional_t *file;

    program = clopts_program_new("filt");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

    file = clopts_command_add_positional(root, "file");
    clopts_command_set_handler(root, dummy_handler, &userdata);
    clopts_command_set_description(root, "Filters lines.");

    NAOS_REQUIRE_NOT_NULL(file);
    NAOS_REQUIRE_UINT_EQ(root->positionals.len, 1);
    NAOS_CHECK_PTR_EQ(positional_at(root, 0), file);
    NAOS_CHECK_PTR_EQ(file->scope, root);
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

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

    build = clopts_program_add_command(program, cmd_name);
    output = clopts_command_add_value(build, long_name, 'o');
    src = clopts_command_add_positional(build, pos_name);
    NAOS_REQUIRE_NOT_NULL(build);
    NAOS_REQUIRE_NOT_NULL(output);
    NAOS_REQUIRE_NOT_NULL(src);
    memset(cmd_name, 'X', sizeof(cmd_name) - 1);
    memset(long_name, 'X', sizeof(long_name) - 1);
    memset(pos_name, 'X', sizeof(pos_name) - 1);

    strcpy(text, "1.2.3");
    clopts_program_set_version(program, text);
    strcpy(text, "XXXXX");
    strcpy(text, "Builds.");
    clopts_command_set_summary(build, text);
    strcpy(text, "XXXXXXX");
    strcpy(text, "Builds the tree.");
    clopts_command_set_description(build, text);
    strcpy(text, "XXXXXXXXXXXXXXXX");
    strcpy(text, "Program text.");
    clopts_command_set_description(root, text);
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

    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);
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

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    build = clopts_program_add_command(program, "build");
    value = clopts_command_add_value(root, "output", 'o');
    list = clopts_command_add_list(root, "include", 'I');
    count = clopts_command_add_count(root, "verbose", 'v');
    flag = clopts_command_add_flag(root, "color", '\0');
    src = clopts_command_add_positional(build, "src");
    NAOS_REQUIRE_NOT_NULL(src);

    clopts_program_set_version(program, "1.0");
    clopts_program_set_version(program, "2.0");
    clopts_command_set_summary(build, "first");
    clopts_command_set_summary(build, "second");
    clopts_command_set_description(build, "first");
    clopts_command_set_description(build, "second");
    clopts_command_set_handler(build, dummy_handler, &first);
    clopts_command_set_handler(build, other_handler, &second);
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
    clopts_option_set_required(list, true);
    clopts_positional_set_help(src, "first");
    clopts_positional_set_help(src, "second");
    clopts_positional_set_required(src, true);
    clopts_positional_set_variadic(src, true);

    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);
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
    clopts_program_set_version(program, NULL);
    clopts_command_set_summary(build, NULL);
    clopts_command_set_description(build, NULL);
    clopts_command_set_handler(build, NULL, NULL);
    clopts_option_set_help(value, NULL);
    clopts_option_set_value_name(value, NULL);
    clopts_option_set_env(value, NULL);
    clopts_option_set_default(value, NULL);
    clopts_option_set_delimiter(list, '\0');
    clopts_option_set_count_max(count, 0);
    clopts_option_set_negatable(flag, false);
    clopts_option_set_required(list, false);
    clopts_positional_set_help(src, NULL);
    clopts_positional_set_variadic(src, false);
    clopts_positional_set_required(src, false);

    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);
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
    int i;

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);

    first = clopts_command_add_value(root, "o0", 'a');
    NAOS_REQUIRE_NOT_NULL(first);
    clopts_option_set_help(first, "first help");
    for (i = 1; i < 100; i++)
    {
        (void)snprintf(name, sizeof(name), "o%d", i);
        option = clopts_command_add_flag(root, name, '\0');
        NAOS_NOTE("i=%d", i);
        NAOS_REQUIRE_NOT_NULL(option);
        NAOS_CHECK_UINT_EQ(option->index, (size_t)i);
    }

    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);
    NAOS_REQUIRE_UINT_EQ(root->options.len, 100);
    NAOS_CHECK_PTR_EQ(option_at(root, 0), first);
    NAOS_CHECK_PTR_EQ(first->program, program);
    NAOS_CHECK_PTR_EQ(first->scope, root);
    NAOS_CHECK_UINT_EQ(first->index, 0);
    NAOS_CHECK_INT_EQ(first->kind, CLOPTS_PARSER_VALUE);
    NAOS_CHECK_STR_EQ(first->long_name, "o0");
    NAOS_CHECK_INT_EQ(first->short_name, 'a');
    NAOS_CHECK_STR_EQ(first->help, "first help");
    NAOS_CHECK_STR_EQ(option_at(root, 99)->long_name, "o99");

    clopts_program_free(program);
}

/* ---- out of memory --------------------------------------------------------- */

NAOS_TEST(first_out_of_memory_stops_the_builder)
{
    clopts_parser_probe_t probe;
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_result_t *result;

    probe = make_probe(1);
    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_parser_set_probe(program, &probe);
    NAOS_CHECK_PTR_EQ(program->probe, &probe);
    root = clopts_program_root(program);

    NAOS_CHECK_NULL(clopts_command_add_flag(root, "force", 'f'));
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OUT_OF_MEMORY);

    /* later calls do nothing, even when allocation would succeed again */
    probe.fail_at = 0;
    probe.calls = 0;
    NAOS_CHECK_NULL(clopts_command_add_value(root, "output", 'o'));
    NAOS_CHECK_NULL(clopts_program_add_command(program, "build"));
    NAOS_CHECK_NULL(clopts_command_add_positional(root, "file"));
    clopts_program_set_version(program, "1.0");
    clopts_command_set_description(root, "text");
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
    NAOS_CHECK_UINT_EQ(root->options.len, 0);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_NULL(root->description);
    NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OUT_OF_MEMORY);

    /* the root is still returned */
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), root);

    result = clopts_program_validate(program);
    NAOS_CHECK_PTR_EQ(result, clopts_parser_result_oom());
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(program->frozen);
    clopts_result_free(result);

    clopts_program_free(program);
    NAOS_CHECK_INT_EQ(probe.live, 0);
}

NAOS_TEST(setter_out_of_memory_keeps_the_old_copy)
{
    clopts_parser_probe_t probe;
    clopts_program_t *program;
    clopts_option_t *output;

    probe = make_probe(0);
    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_parser_set_probe(program, &probe);
    output = clopts_command_add_value(clopts_program_root(program), "output", 'o');
    NAOS_REQUIRE_NOT_NULL(output);
    clopts_option_set_help(output, "old");
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);

    probe.fail_at = probe.calls + 1;
    clopts_option_set_help(output, "new");
    NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OUT_OF_MEMORY);
    NAOS_REQUIRE_NOT_NULL(output->help);
    NAOS_CHECK_STR_EQ(output->help, "old");

    clopts_program_free(program);
    NAOS_CHECK_INT_EQ(probe.live, 0);
}

/* Builds a small program through every allocating builder function. */
static void build_sample(clopts_program_t *program)
{
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_option_t *include;
    clopts_positional_t *src;

    root = clopts_program_root(program);
    clopts_program_set_version(program, "1.0");
    clopts_command_set_description(root, "A tool.");
    (void)clopts_command_add_count(root, "verbose", 'v');
    build = clopts_program_add_command(program, "build");
    clopts_command_set_summary(build, "Builds.");
    clopts_command_set_description(build, "Builds the tree.");
    output = clopts_command_add_value(build, "output", 'o');
    clopts_option_set_help(output, "Output file.");
    clopts_option_set_value_name(output, "FILE");
    clopts_option_set_env(output, "TOOL_OUTPUT");
    clopts_option_set_default(output, "a.out");
    include = clopts_command_add_list(build, "include", 'I');
    clopts_option_set_delimiter(include, ',');
    (void)clopts_command_add_flag(build, "release", '\0');
    src = clopts_command_add_positional(build, "src");
    clopts_positional_set_help(src, "Source.");
}

NAOS_TEST(every_failure_point_is_released)
{
    clopts_parser_probe_t probe;
    clopts_program_t *program;
    size_t fail_at;
    bool succeeded = false;

    for (fail_at = 1; (fail_at < 200) && (!succeeded); fail_at++)
    {
        NAOS_NOTE("fail_at=%u", (unsigned)fail_at);
        probe = make_probe(fail_at);
        program = clopts_program_new("tool");
        NAOS_REQUIRE_NOT_NULL(program);
        clopts_parser_set_probe(program, &probe);

        build_sample(program);
        if (probe.calls < fail_at)
        {
            NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OK);
            succeeded = true;
        }
        else
        {
            NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OUT_OF_MEMORY);
        }

        clopts_program_free(program);
        NAOS_CHECK_INT_EQ(probe.live, 0);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(fail_at, 2);
}

/* ---- validation entry ------------------------------------------------------ */

NAOS_TEST(validate_null_program_returns_sentinel)
{
    clopts_result_t *result;

    result = clopts_program_validate(NULL);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_PTR_EQ(result, clopts_parser_result_oom());
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    clopts_result_free(result);
}

NAOS_TEST(validate_fresh_program_returns_ok)
{
    clopts_parser_probe_t probe;
    clopts_program_t *program;
    clopts_result_t *result;

    probe = make_probe(0);
    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_parser_set_probe(program, &probe);
    build_sample(program);
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);

    result = clopts_program_validate(program);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_PTR_EQ(result->program, program);
    NAOS_CHECK_PTR_EQ(result->probe, &probe);
    NAOS_CHECK_FALSE(result->parsed);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);
    NAOS_CHECK_TRUE(program->frozen);
    NAOS_CHECK_PTR_EQ(clopts_program_root(program), &program->root);

    clopts_result_free(result);
    clopts_program_free(program);
    NAOS_CHECK_INT_EQ(probe.live, 0);
}

NAOS_TEST(validate_result_allocation_failure_returns_sentinel)
{
    clopts_parser_probe_t probe;
    clopts_program_t *program;
    clopts_result_t *result;

    probe = make_probe(0);
    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    clopts_parser_set_probe(program, &probe);
    (void)clopts_command_add_flag(clopts_program_root(program), "force", 'f');
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);

    probe.fail_at = probe.calls + 1;
    result = clopts_program_validate(program);
    NAOS_CHECK_PTR_EQ(result, clopts_parser_result_oom());
    NAOS_CHECK_TRUE(program->frozen);
    clopts_result_free(result);

    clopts_program_free(program);
    NAOS_CHECK_INT_EQ(probe.live, 0);
}

/*
 * The immediate checks come with task 6.1; here a recorded declaration error
 * is put into the model directly to check what the entry copies.
 */
NAOS_TEST(validate_copies_a_recorded_declaration_error)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_option_t *output;
    clopts_positional_t *src;
    clopts_result_t *result;

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    build = clopts_program_add_command(program, "build");
    output = clopts_command_add_value(build, "output", 'o');
    src = clopts_command_add_positional(build, "src");
    NAOS_REQUIRE_NOT_NULL(src);

    program->error.status = CLOPTS_DECL_ERROR;
    program->error.kind = CLOPTS_DECL_DUPLICATE_OPTION;
    program->error.command = build;
    program->error.option = output;
    program->error.positional = NULL;

    result = clopts_program_validate(program);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_DECL_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), build);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), output);
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_NONE);
    NAOS_CHECK_INT_EQ(result->error.argv_index, -1);
    clopts_result_free(result);

    /* the root is reported as NULL */
    program->error.kind = CLOPTS_DECL_POSITIONAL_ORDER;
    program->error.command = root;
    program->error.option = NULL;
    program->error.positional = src;
    result = clopts_program_validate(program);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_DECL_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_POSITIONAL_ORDER);
    NAOS_CHECK_NULL(clopts_result_error_command(result));
    NAOS_CHECK_NULL(clopts_result_error_option(result));
    NAOS_CHECK_PTR_EQ(clopts_result_error_positional(result), src);
    clopts_result_free(result);

    clopts_program_free(program);
}

/* ---- freezing ---------------------------------------------------------------- */

NAOS_TEST(builder_after_validation_does_nothing)
{
#ifdef NDEBUG
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_option_t *flag;
    clopts_result_t *result;

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    root = clopts_program_root(program);
    flag = clopts_command_add_flag(root, "force", 'f');
    NAOS_REQUIRE_NOT_NULL(flag);

    result = clopts_program_validate(program);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    clopts_result_free(result);

    NAOS_CHECK_NULL(clopts_command_add_flag(root, "other", 'o'));
    NAOS_CHECK_NULL(clopts_program_add_command(program, "build"));
    NAOS_CHECK_NULL(clopts_command_add_positional(root, "file"));
    clopts_option_set_help(flag, "help");
    clopts_program_set_version(program, "1.0");
    NAOS_CHECK_UINT_EQ(root->options.len, 1);
    NAOS_CHECK_UINT_EQ(program->commands.len, 0);
    NAOS_CHECK_NULL(flag->help);
    NAOS_CHECK_NULL(program->version);
    NAOS_CHECK_INT_EQ(program->error.status, CLOPTS_OK);
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

/* Checks every model field of option against expected. */
static void check_option(const clopts_option_t *option, const expected_option_t *expected)
{
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_PTR_EQ(option->scope, expected->scope);
    NAOS_CHECK_PTR_EQ(option->program, expected->scope->program);
    NAOS_CHECK_UINT_EQ(option->index, expected->index);
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

/* Checks the model fields of positional. */
static void check_positional(const clopts_positional_t *positional, const clopts_command_t *scope, size_t index,
                             const char *name, const char *help, bool required, bool variadic)
{
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_PTR_EQ(positional->scope, scope);
    NAOS_CHECK_PTR_EQ(positional->program, scope->program);
    NAOS_CHECK_UINT_EQ(positional->index, index);
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

/* Validates program and checks that the status is OK. */
static void check_validates_ok(clopts_program_t *program)
{
    clopts_result_t *result;

    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);
    result = clopts_program_validate(program);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);
    clopts_result_free(result);
}

NAOS_TEST(fixtures_validate_ok)
{
    fixture_tool_t tool;
    fixture_filt_t filt;
    fixture_strict_t strict;

    fixture_tool_init(&tool, NULL);
    check_validates_ok(tool.program);
    fixture_tool_free(&tool);
    NAOS_CHECK_NULL(tool.program);

    fixture_filt_init(&filt, NULL);
    check_validates_ok(filt.program);
    fixture_filt_free(&filt);
    NAOS_CHECK_NULL(filt.program);

    fixture_strict_init(&strict, NULL);
    check_validates_ok(strict.program);
    fixture_strict_free(&strict);
    NAOS_CHECK_NULL(strict.program);
}

NAOS_TEST(fixture_tool_declaration)
{
    fixture_tool_t f;
    clopts_program_t *program;
    clopts_command_t *root;

    fixture_tool_init(&f, NULL);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(program->probe, NULL);

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

    fixture_filt_init(&f, NULL);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);

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

    fixture_strict_init(&f, NULL);
    program = f.program;
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_INT_EQ(program->error.status, CLOPTS_OK);

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
    clopts_parser_probe_t probe;
    fixture_tool_t tool;
    fixture_filt_t filt;
    fixture_strict_t strict;

    probe = make_probe(0);
    fixture_tool_init(&tool, &probe);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_CHECK_PTR_EQ(tool.program->probe, &probe);
    NAOS_CHECK_UINT_GT(probe.calls, 0);
    NAOS_CHECK_INT_GT(probe.live, 0);
    check_validates_ok(tool.program);
    fixture_tool_free(&tool);
    NAOS_CHECK_INT_EQ(probe.live, 0);

    probe = make_probe(0);
    fixture_filt_init(&filt, &probe);
    NAOS_REQUIRE_NOT_NULL(filt.program);
    NAOS_CHECK_PTR_EQ(filt.program->probe, &probe);
    NAOS_CHECK_INT_GT(probe.live, 0);
    check_validates_ok(filt.program);
    fixture_filt_free(&filt);
    NAOS_CHECK_INT_EQ(probe.live, 0);

    probe = make_probe(0);
    fixture_strict_init(&strict, &probe);
    NAOS_REQUIRE_NOT_NULL(strict.program);
    NAOS_CHECK_PTR_EQ(strict.program->probe, &probe);
    NAOS_CHECK_INT_GT(probe.live, 0);
    check_validates_ok(strict.program);
    fixture_strict_free(&strict);
    NAOS_CHECK_INT_EQ(probe.live, 0);
}

NAOS_TEST(fixture_out_of_memory_is_recorded_and_released)
{
    clopts_parser_probe_t probe;
    fixture_tool_t tool;
    size_t total;

    /* number of fault points of a full assembly */
    probe = make_probe(0);
    fixture_tool_init(&tool, &probe);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    total = probe.calls;
    fixture_tool_free(&tool);

    /* the first and the last fault point fail */
    probe = make_probe(1);
    fixture_tool_init(&tool, &probe);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_CHECK_INT_EQ(tool.program->error.status, CLOPTS_OUT_OF_MEMORY);
    fixture_tool_free(&tool);
    NAOS_CHECK_INT_EQ(probe.live, 0);

    probe = make_probe(total);
    fixture_tool_init(&tool, &probe);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_CHECK_INT_EQ(tool.program->error.status, CLOPTS_OUT_OF_MEMORY);
    fixture_tool_free(&tool);
    NAOS_CHECK_INT_EQ(probe.live, 0);
}

NAOS_TEST(fixture_handler_records_calls)
{
    fixture_handler_record_t record;
    int marker;

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
    NAOS_ADD_TEST(null_handles_do_nothing),
    NAOS_ADD_TEST(free_null_program_does_nothing),
    NAOS_ADD_TEST(program_new_embeds_root_and_copies_name),
    NAOS_ADD_TEST(adders_link_objects_in_order),
    NAOS_ADD_TEST(root_takes_positionals_and_handler),
    NAOS_ADD_TEST(strings_are_copied),
    NAOS_ADD_TEST(setting_again_keeps_the_last_value),
    NAOS_ADD_TEST(hundred_options_keep_the_first_handle),
    NAOS_ADD_TEST(first_out_of_memory_stops_the_builder),
    NAOS_ADD_TEST(setter_out_of_memory_keeps_the_old_copy),
    NAOS_ADD_TEST(every_failure_point_is_released),
    NAOS_ADD_TEST(validate_null_program_returns_sentinel),
    NAOS_ADD_TEST(validate_fresh_program_returns_ok),
    NAOS_ADD_TEST(validate_result_allocation_failure_returns_sentinel),
    NAOS_ADD_TEST(validate_copies_a_recorded_declaration_error),
    NAOS_ADD_TEST(builder_after_validation_does_nothing),
    NAOS_ADD_TEST(fixtures_validate_ok),
    NAOS_ADD_TEST(fixture_tool_declaration),
    NAOS_ADD_TEST(fixture_filt_declaration),
    NAOS_ADD_TEST(fixture_strict_declaration),
    NAOS_ADD_TEST(fixtures_release_every_block),
    NAOS_ADD_TEST(fixture_out_of_memory_is_recorded_and_released),
    NAOS_ADD_TEST(fixture_handler_records_calls),
    NAOS_ADD_TEST(fixture_env_lends_from_its_buffers),
    NAOS_ADD_TEST(fixture_env_fail_next_is_one_shot),
    NAOS_ADD_TEST(fixture_env_clobber_overwrites_lent_values)
)
