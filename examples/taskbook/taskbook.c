/*
 * taskbook: a fictional task manager showing a CLOpts program with
 * subcommands.
 *
 *   taskbook [-v] [--color | --no-color] add TITLE [-t TAG,...] [-p LEVEL]
 *   taskbook [-v] [--color | --no-color] list [-a] [--format FORMAT]
 *   taskbook [-v] [--color | --no-color] done ID...
 *
 * It does not store any task: each command reports the values it got as
 * "<key>: <value> (<source>)" lines on stdout.
 */
#include <stdio.h>

#include "clopts/clopts.h"

/* Option handles shared by every handler (passed as userdata). */
struct taskbook_options
{
    clopts_option_t *verbose;
    clopts_option_t *color;
    clopts_option_t *tag;
    clopts_option_t *priority;
    clopts_option_t *all;
    clopts_option_t *format;
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

static int run_add(const clopts_result_t *result, void *userdata)
{
    const struct taskbook_options *opts = userdata;
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

static int run_list(const clopts_result_t *result, void *userdata)
{
    const struct taskbook_options *opts = userdata;

    printf("command: list\n");
    printf("all: %s", clopts_result_flag(result, opts->all) ? "true" : "false");
    print_source(result, opts->all);
    printf("format: %s", clopts_result_value(result, opts->format));
    print_source(result, opts->format);
    print_globals(result, opts);
    return 0;
}

static int run_done(const clopts_result_t *result, void *userdata)
{
    const struct taskbook_options *opts = userdata;
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

/* Declares the program. Errors are recorded in it and reported by the parse. */
static void declare(clopts_program_t *program, struct taskbook_options *opts)
{
    clopts_command_t *root = clopts_program_root(program);
    clopts_command_t *add;
    clopts_command_t *list;
    clopts_command_t *done;
    clopts_positional_t *positional;

    clopts_program_set_version(program, "1.2.3");
    clopts_command_set_description(root, "Keep a small list of tasks.");

    opts->verbose = clopts_command_add_count(root, "verbose", 'v');
    clopts_option_set_count_max(opts->verbose, 3);
    clopts_option_set_help(opts->verbose, "Print more details (up to 3 times)");
    opts->color = clopts_command_add_flag(root, "color", '\0');
    clopts_option_set_negatable(opts->color, true);
    clopts_option_set_help(opts->color, "Colorize the output");

    add = clopts_program_add_command(program, "add");
    clopts_command_set_summary(add, "Add a task");
    clopts_command_set_description(add, "Add a task with the given title.");
    clopts_command_set_handler(add, run_add, opts);
    opts->tag = clopts_command_add_list(add, "tag", 't');
    clopts_option_set_value_name(opts->tag, "TAG");
    clopts_option_set_delimiter(opts->tag, ',');
    clopts_option_set_help(opts->tag, "Tags of the task");
    opts->priority = clopts_command_add_value(add, "priority", 'p');
    clopts_option_set_value_name(opts->priority, "LEVEL");
    clopts_option_set_default(opts->priority, "normal");
    clopts_option_set_help(opts->priority, "Priority: low, normal or high");
    positional = clopts_command_add_positional(add, "TITLE");
    clopts_positional_set_required(positional, true);
    clopts_positional_set_help(positional, "Title of the task");

    list = clopts_program_add_command(program, "list");
    clopts_command_set_summary(list, "List tasks");
    clopts_command_set_description(list, "List the open tasks.");
    clopts_command_set_handler(list, run_list, opts);
    opts->all = clopts_command_add_flag(list, "all", 'a');
    clopts_option_set_help(opts->all, "Include finished tasks");
    opts->format = clopts_command_add_value(list, "format", '\0');
    clopts_option_set_value_name(opts->format, "FORMAT");
    clopts_option_set_env(opts->format, "TASKBOOK_FORMAT");
    clopts_option_set_default(opts->format, "text");
    clopts_option_set_help(opts->format, "Output format: text or json");

    done = clopts_program_add_command(program, "done");
    clopts_command_set_summary(done, "Mark tasks as finished");
    clopts_command_set_description(done, "Mark the given tasks as finished.");
    clopts_command_set_handler(done, run_done, opts);
    positional = clopts_command_add_positional(done, "ID");
    clopts_positional_set_required(positional, true);
    clopts_positional_set_variadic(positional, true);
    clopts_positional_set_help(positional, "Numbers of the tasks");
}

/* Writes the text for result and returns the exit code (0, 1 or 2). */
static int finish(const clopts_program_t *program, const clopts_result_t *result)
{
    switch (clopts_result_status(result))
    {
    case CLOPTS_OK:
        return clopts_result_run(result);
    case CLOPTS_HELP:
        clopts_write_help(program, clopts_result_command(result), clopts_file_write, stdout);
        return 0;
    case CLOPTS_VERSION:
        clopts_write_version(program, clopts_file_write, stdout);
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
        /* CLOPTS_DECL_ERROR and CLOPTS_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        return 1;
    }
}

int main(int argc, char **argv)
{
    struct taskbook_options opts = {NULL, NULL, NULL, NULL, NULL, NULL};
    clopts_program_t *program;
    clopts_argv_t *args;
    clopts_env_t *env;
    clopts_result_t *result;
    int code;

    program = clopts_program_new("taskbook");
    if (program == NULL)
    {
        fputs("taskbook: error: out of memory\n", stderr);
        return 1;
    }
    declare(program, &opts);

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_get(argc, argv);
    if (clopts_argv_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("taskbook: error: cannot read the command line arguments\n", stderr);
        clopts_argv_free(args);
        clopts_program_free(program);
        return 1;
    }

    /* If clopts_env_new failed, looking up TASKBOOK_FORMAT fails and the parse
       reports out of memory. */
    env = clopts_env_new();
    result = clopts_parse_with_env(program, clopts_argv_count(args), clopts_argv_values(args), clopts_env_lookup, env);
    code = finish(program, result);

    clopts_result_free(result);
    clopts_env_free(env);
    clopts_argv_free(args);
    clopts_program_free(program);
    return code;
}
