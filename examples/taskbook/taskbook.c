/*
 * taskbook: a fictional task manager showing a CLOpts program with
 * subcommands.
 *
 *   taskbook [-v] [--color | --no-color] add TITLE [-t TAG,...] [-p LEVEL]
 *   taskbook [-v] [--color | --no-color] list [-a] [--format FORMAT]
 *   taskbook [-v] [--color | --no-color] done ID...
 *   taskbook help [COMMAND]
 *
 * It does not store any task: each command reports the values it got as
 * "<key>: <value> (<source>)" lines on stdout.
 */
#include <stdbool.h>
#include <stdio.h>

#include "clopts/clopts.h"

/* Handles of the declaration, read after the parse. */
struct taskbook_options
{
    const clopts_option_t *help;
    const clopts_option_t *version;
    clopts_option_t *verbose;
    clopts_option_t *color;
    clopts_option_t *tag;
    clopts_option_t *priority;
    clopts_option_t *all;
    clopts_option_t *format;
    clopts_command_t *add;
    clopts_command_t *list;
    clopts_command_t *done;
    clopts_command_t *help_command;
};

/* Ends the line, adding " (<source>)" when the option got a value. */
static void print_source(const clopts_result_t *result, const clopts_option_t *option)
{
    switch (clopts_result_source(result, option))
    {
    case CLOPTS_SOURCE_EXPLICIT:
        fputs(" (explicit)", stdout);
        break;
    case CLOPTS_SOURCE_ENV:
        fputs(" (env)", stdout);
        break;
    case CLOPTS_SOURCE_DEFAULT:
        fputs(" (default)", stdout);
        break;
    default:
        break;
    }
    fputc('\n', stdout);
}

static void print_globals(const clopts_result_t *result, const struct taskbook_options *opts)
{
    printf("verbose: %u", clopts_result_count(result, opts->verbose));
    print_source(result, opts->verbose);
    printf("color: %s", clopts_result_flag(result, opts->color) ? "true" : "false");
    print_source(result, opts->color);
}

static int run_add(const clopts_result_t *result, const struct taskbook_options *opts)
{
    size_t count = clopts_result_list_count(result, opts->tag);
    size_t i;

    printf("command: add\n");
    printf("title: %s\n", clopts_result_positional_at(result, 0));
    printf("tags:");
    for (i = 0; i < count; i++)
    {
        printf("%s %s", (i > 0) ? "," : "", clopts_result_list_at(result, opts->tag, i));
    }
    print_source(result, opts->tag);
    printf("priority: %s", clopts_result_value(result, opts->priority));
    print_source(result, opts->priority);
    print_globals(result, opts);
    return 0;
}

static int run_list(const clopts_result_t *result, const struct taskbook_options *opts)
{

    printf("command: list\n");
    printf("all: %s", clopts_result_flag(result, opts->all) ? "true" : "false");
    print_source(result, opts->all);
    printf("format: %s", clopts_result_value(result, opts->format));
    print_source(result, opts->format);
    print_globals(result, opts);
    return 0;
}

static int run_done(const clopts_result_t *result, const struct taskbook_options *opts)
{
    size_t count = clopts_result_positional_count(result);
    size_t i;

    printf("command: done\n");
    printf("ids:");
    for (i = 0; i < count; i++)
    {
        printf("%s %s", (i > 0) ? "," : "", clopts_result_positional_at(result, i));
    }
    fputc('\n', stdout);
    print_globals(result, opts);
    return 0;
}

/*
 * Keeps the first declaration error. Once one is kept, the rest of the
 * declaration is skipped: every later call fails or is ignored.
 */
static bool check(clopts_decl_error_t *first, clopts_decl_error_t error)
{
    if (*first == CLOPTS_DECL_NONE)
    {
        *first = error;
    }
    return *first == CLOPTS_DECL_NONE;
}

/* Attaches option to command and returns it, or releases it and returns NULL. */
static clopts_option_t *attach_option(clopts_decl_error_t *first, clopts_command_t *command, clopts_option_t *option)
{
    if ((*first != CLOPTS_DECL_NONE) || !check(first, clopts_command_add_option(command, option)))
    {
        clopts_option_destroy(option);
        return NULL;
    }
    return option;
}

/* Attaches positional to command, or releases it. */
static void attach_positional(clopts_decl_error_t *first, clopts_command_t *command, clopts_positional_t *positional)
{
    if ((*first != CLOPTS_DECL_NONE) || !check(first, clopts_command_add_positional(command, positional)))
    {
        clopts_positional_destroy(positional);
    }
}

/*
 * Declares the program into *out, from the bottom up: the root and the
 * commands first, then the program. Returns the first declaration error, or
 * CLOPTS_DECL_NONE.
 */
static clopts_decl_error_t declare(clopts_program_t **out, struct taskbook_options *opts)
{
    clopts_decl_error_t e = CLOPTS_DECL_NONE;
    clopts_command_t *root = NULL;
    clopts_command_t *add = NULL;
    clopts_command_t *list = NULL;
    clopts_command_t *done = NULL;
    clopts_command_t *help = NULL;
    clopts_option_t *option = NULL;
    clopts_positional_t *positional = NULL;

    check(&e, clopts_command_create("taskbook", &root));
    check(&e, clopts_command_set_description(root, "Keep a small list of tasks."));
    check(&e, clopts_option_create(CLOPTS_OPTION_COUNT, "verbose", 'v', &option));
    check(&e, clopts_option_set_count_max(option, 3));
    check(&e, clopts_option_set_help(option, "Print more details (up to 3 times)"));
    opts->verbose = attach_option(&e, root, option);
    check(&e, clopts_option_create(CLOPTS_OPTION_FLAG, "color", '\0', &option));
    check(&e, clopts_option_set_negatable(option, true));
    check(&e, clopts_option_set_help(option, "Colorize the output"));
    opts->color = attach_option(&e, root, option);
    check(&e, clopts_help_add_help_option(root, &opts->help));
    check(&e, clopts_help_add_version_option(root, &opts->version));

    check(&e, clopts_command_create("add", &add));
    check(&e, clopts_command_set_summary(add, "Add a task"));
    check(&e, clopts_command_set_description(add, "Add a task with the given title."));
    check(&e, clopts_option_create(CLOPTS_OPTION_LIST, "tag", 't', &option));
    check(&e, clopts_option_set_value_name(option, "TAG"));
    check(&e, clopts_option_set_delimiter(option, ','));
    check(&e, clopts_option_set_help(option, "Tags of the task"));
    opts->tag = attach_option(&e, add, option);
    check(&e, clopts_option_create(CLOPTS_OPTION_VALUE, "priority", 'p', &option));
    check(&e, clopts_option_set_value_name(option, "LEVEL"));
    check(&e, clopts_option_set_default(option, "normal"));
    check(&e, clopts_option_set_help(option, "Priority: low, normal or high"));
    opts->priority = attach_option(&e, add, option);
    check(&e, clopts_positional_create("TITLE", &positional));
    check(&e, clopts_positional_set_required(positional, true));
    check(&e, clopts_positional_set_help(positional, "Title of the task"));
    attach_positional(&e, add, positional);

    check(&e, clopts_command_create("list", &list));
    check(&e, clopts_command_set_summary(list, "List tasks"));
    check(&e, clopts_command_set_description(list, "List the open tasks."));
    check(&e, clopts_option_create(CLOPTS_OPTION_FLAG, "all", 'a', &option));
    check(&e, clopts_option_set_help(option, "Include finished tasks"));
    opts->all = attach_option(&e, list, option);
    check(&e, clopts_option_create(CLOPTS_OPTION_VALUE, "format", '\0', &option));
    check(&e, clopts_option_set_value_name(option, "FORMAT"));
    check(&e, clopts_option_set_env(option, "TASKBOOK_FORMAT"));
    check(&e, clopts_option_set_default(option, "text"));
    check(&e, clopts_option_set_help(option, "Output format: text or json"));
    opts->format = attach_option(&e, list, option);

    check(&e, clopts_command_create("done", &done));
    check(&e, clopts_command_set_summary(done, "Mark tasks as finished"));
    check(&e, clopts_command_set_description(done, "Mark the given tasks as finished."));
    check(&e, clopts_positional_create("ID", &positional));
    check(&e, clopts_positional_set_required(positional, true));
    check(&e, clopts_positional_set_variadic(positional, true));
    check(&e, clopts_positional_set_help(positional, "Numbers of the tasks"));
    attach_positional(&e, done, positional);

    check(&e, clopts_help_command_create(&help));

    if (e == CLOPTS_DECL_NONE)
    {
        clopts_command_t *commands[4];

        commands[0] = add;
        commands[1] = list;
        commands[2] = done;
        commands[3] = help;
        check(&e, clopts_program_create(root, commands, 4, out));
    }
    if (e != CLOPTS_DECL_NONE)
    {
        /* Nothing was handed to a program: every command is still ours. */
        clopts_command_destroy(root);
        clopts_command_destroy(add);
        clopts_command_destroy(list);
        clopts_command_destroy(done);
        clopts_command_destroy(help);
        return e;
    }
    opts->add = add;
    opts->list = list;
    opts->done = done;
    opts->help_command = help;
    return e;
}

/* Runs the selected command. */
static int run(const clopts_program_t *program, const clopts_result_t *result, const struct taskbook_options *opts)
{
    const clopts_command_t *command = clopts_result_command(result);
    const clopts_command_t *target;

    if (command == opts->add)
    {
        return run_add(result, opts);
    }
    if (command == opts->list)
    {
        return run_list(result, opts);
    }
    if (command == opts->done)
    {
        return run_done(result, opts);
    }
    /* help [COMMAND] */
    if (!clopts_help_target(program, result, &target))
    {
        fprintf(stderr, "taskbook: error: unknown command '%s'\n", clopts_result_positional_at(result, 0));
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        return 2;
    }
    clopts_write_help(program, target, clopts_file_write, stdout);
    return 0;
}

/* Writes the text for result and returns the exit code (0, 1 or 2). */
static int finish(const clopts_program_t *program, const clopts_result_t *result,
                  const struct taskbook_options *opts)
{
    switch (clopts_result_status(result))
    {
    case CLOPTS_OK:
        return run(program, result, opts);
    case CLOPTS_TERMINATED:
        if (clopts_result_terminal(result) == opts->version)
        {
            clopts_write_version(program, "1.2.3", clopts_file_write, stdout);
        }
        else
        {
            clopts_write_help(program, clopts_result_command(result), clopts_file_write, stdout);
        }
        return 0;
    case CLOPTS_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_error_command(result), clopts_file_write, stderr);
        return 2;
    case CLOPTS_NO_COMMAND:
        clopts_write_usage(program, NULL, clopts_file_write, stderr);
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        return 2;
    default:
        /* CLOPTS_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        return 1;
    }
}

int main(int argc, char **argv)
{
    struct taskbook_options opts = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    clopts_program_t *program = NULL;
    clopts_argv_t *args;
    clopts_env_t *env;
    clopts_result_t *result;
    clopts_decl_error_t error;
    int code;

    error = declare(&program, &opts);
    if (error != CLOPTS_DECL_NONE)
    {
        fprintf(stderr, "taskbook: error: invalid declaration: %s\n", clopts_decl_error_message(error));
        return 1;
    }

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_get(argc, argv);
    if (clopts_argv_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("taskbook: error: cannot read the command line arguments\n", stderr);
        clopts_argv_free(args);
        clopts_program_destroy(program);
        return 1;
    }

    /* If clopts_env_new failed, looking up TASKBOOK_FORMAT fails and the parse
       reports out of memory. */
    env = clopts_env_new();
    result = clopts_parse_with_env(program, clopts_argv_count(args), clopts_argv_values(args), clopts_env_lookup, env);
    code = finish(program, result, &opts);

    clopts_result_free(result);
    clopts_env_free(env);
    clopts_argv_free(args);
    clopts_program_destroy(program);
    return code;
}
