/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
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
};

/* Ends the line, adding " (<source>)" when the option got a value. */
static void print_source(const clopts_result_t *result, const clopts_option_t *option)
{
    switch (clopts_result_get_source(result, option))
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
    printf("verbose: %u", clopts_result_get_count(result, opts->verbose));
    print_source(result, opts->verbose);
    printf("color: %s", clopts_result_get_flag(result, opts->color) ? "true" : "false");
    print_source(result, opts->color);
}

static int run_add(const clopts_result_t *result, const struct taskbook_options *opts)
{
    size_t count = clopts_result_get_list_count(result, opts->tag);
    size_t i;

    printf("command: add\n");
    printf("title: %s\n", clopts_result_get_positional_at(result, 0));
    printf("tags:");
    for (i = 0; i < count; i++)
    {
        printf("%s %s", (i > 0) ? "," : "", clopts_result_get_list_at(result, opts->tag, i));
    }
    print_source(result, opts->tag);
    printf("priority: %s", clopts_result_get_value(result, opts->priority));
    print_source(result, opts->priority);
    print_globals(result, opts);
    return 0;
}

static int run_list(const clopts_result_t *result, const struct taskbook_options *opts)
{

    printf("command: list\n");
    printf("all: %s", clopts_result_get_flag(result, opts->all) ? "true" : "false");
    print_source(result, opts->all);
    printf("format: %s", clopts_result_get_value(result, opts->format));
    print_source(result, opts->format);
    print_globals(result, opts);
    return 0;
}

static int run_done(const clopts_result_t *result, const struct taskbook_options *opts)
{
    size_t count = clopts_result_get_positional_count(result);
    size_t i;

    printf("command: done\n");
    printf("ids:");
    for (i = 0; i < count; i++)
    {
        printf("%s %s", (i > 0) ? "," : "", clopts_result_get_positional_at(result, i));
    }
    fputc('\n', stdout);
    print_globals(result, opts);
    return 0;
}

/*
 * Declares the program: its global options first, then each command with its
 * options and positionals. Nothing is checked here: the program keeps the
 * first declaration error, and main() checks it once.
 */
static clopts_program_t *declare(struct taskbook_options *opts)
{
    clopts_program_t *program = clopts_program_create("taskbook");
    clopts_command_t *help;
    clopts_positional_t *positional;

    clopts_program_set_description(program, "Keep a small list of tasks.");
    opts->verbose = clopts_program_add_option(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_count_max(opts->verbose, 3);
    clopts_option_set_help(opts->verbose, "Print more details (up to 3 times)");
    opts->color = clopts_program_add_option(program, CLOPTS_OPTION_FLAG, "color", '\0');
    clopts_option_set_negatable(opts->color, true);
    clopts_option_set_help(opts->color, "Colorize the output");
    clopts_program_add_help_option(program);
    opts->version = clopts_program_add_version_option(program);

    opts->add = clopts_program_add_command(program, "add");
    clopts_command_set_summary(opts->add, "Add a task");
    clopts_command_set_description(opts->add, "Add a task with the given title.");
    opts->tag = clopts_command_add_option(opts->add, CLOPTS_OPTION_LIST, "tag", 't');
    clopts_option_set_value_name(opts->tag, "TAG");
    clopts_option_set_delimiter(opts->tag, ',');
    clopts_option_set_help(opts->tag, "Tags of the task");
    opts->priority = clopts_command_add_option(opts->add, CLOPTS_OPTION_VALUE, "priority", 'p');
    clopts_option_set_value_name(opts->priority, "LEVEL");
    clopts_option_set_default(opts->priority, "normal");
    clopts_option_set_help(opts->priority, "Priority: low, normal or high");
    positional = clopts_command_add_positional(opts->add, "TITLE");
    clopts_positional_set_required(positional, true);
    clopts_positional_set_help(positional, "Title of the task");

    opts->list = clopts_program_add_command(program, "list");
    clopts_command_set_summary(opts->list, "List tasks");
    clopts_command_set_description(opts->list, "List the open tasks.");
    opts->all = clopts_command_add_option(opts->list, CLOPTS_OPTION_FLAG, "all", 'a');
    clopts_option_set_help(opts->all, "Include finished tasks");
    opts->format = clopts_command_add_option(opts->list, CLOPTS_OPTION_VALUE, "format", '\0');
    clopts_option_set_value_name(opts->format, "FORMAT");
    clopts_option_set_env(opts->format, "TASKBOOK_FORMAT");
    clopts_option_set_default(opts->format, "text");
    clopts_option_set_help(opts->format, "Output format: text or json");

    opts->done = clopts_program_add_command(program, "done");
    clopts_command_set_summary(opts->done, "Mark tasks as finished");
    clopts_command_set_description(opts->done, "Mark the given tasks as finished.");
    positional = clopts_command_add_positional(opts->done, "ID");
    clopts_positional_set_required(positional, true);
    clopts_positional_set_variadic(positional, true);
    clopts_positional_set_help(positional, "Numbers of the tasks");

    /* help [COMMAND]: an ordinary subcommand; run() resolves COMMAND itself. */
    help = clopts_program_add_command(program, "help");
    clopts_command_set_summary(help, "Show help for a command");
    positional = clopts_command_add_positional(help, "COMMAND");
    clopts_positional_set_help(positional, "Command to show help for");
    return program;
}

/* Runs the selected command. */
static int run(const clopts_program_t *program, const clopts_result_t *result, const struct taskbook_options *opts)
{
    const clopts_command_t *command = clopts_result_get_command(result);
    const clopts_command_t *target = NULL;

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
    /* help [COMMAND]: without COMMAND, the help of the whole program */
    if (clopts_result_get_positional_count(result) > 0)
    {
        const char *name = clopts_result_get_positional_at(result, 0);

        target = clopts_program_find_command(program, name);
        if (target == NULL)
        {
            fprintf(stderr, "taskbook: error: unknown command '%s'\n", name);
            clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
            return 2;
        }
    }
    clopts_write_help(program, target, clopts_file_write, stdout);
    return 0;
}

/* Writes the text for result and returns the exit code (0, 1 or 2). */
static int finish(const clopts_program_t *program, const clopts_result_t *result,
                  const struct taskbook_options *opts)
{
    switch (clopts_result_get_status(result))
    {
    case CLOPTS_PARSE_OK:
        return run(program, result, opts);
    case CLOPTS_PARSE_TERMINATED:
        if (clopts_result_get_terminal_option(result) == opts->version)
        {
            clopts_write_version(program, "1.2.3", clopts_file_write, stdout);
        }
        else
        {
            clopts_write_help(program, clopts_result_get_command(result), clopts_file_write, stdout);
        }
        return 0;
    case CLOPTS_PARSE_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_get_command(result), clopts_file_write, stderr);
        return 2;
    case CLOPTS_PARSE_NO_COMMAND:
        clopts_write_usage(program, NULL, clopts_file_write, stderr);
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        return 2;
    default:
        /* CLOPTS_PARSE_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        return 1;
    }
}

int main(int argc, char **argv)
{
    struct taskbook_options opts = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    clopts_program_t *program = declare(&opts);
    clopts_error_t error = clopts_program_get_error(program);
    clopts_argv_t *args;
    clopts_result_t *result;
    int code;

    /* A declaration mistake, or out of memory (program may be NULL). */
    if (error != CLOPTS_ERROR_NONE)
    {
        fprintf(stderr, "taskbook: error: invalid declaration: %s\n", clopts_error_get_message(error));
        clopts_program_destroy(program);
        return 1;
    }

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_create(argc, argv);
    if (clopts_argv_get_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("taskbook: error: cannot read the command line arguments\n", stderr);
        clopts_argv_destroy(args);
        clopts_program_destroy(program);
        return 1;
    }

    /* TASKBOOK_FORMAT is read from the process environment in UTF-8 too. */
    result = clopts_parse(program, clopts_argv_get_count(args), clopts_argv_get_values(args));
    code = finish(program, result, &opts);

    clopts_result_destroy(result);
    clopts_argv_destroy(args);
    clopts_program_destroy(program);
    return code;
}
