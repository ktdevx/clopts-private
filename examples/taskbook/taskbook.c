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

/* Runs one declaration call; on an error, jumps to the cleanup of the function. */
#define CHECK(call)                  \
    do                               \
    {                                \
        e = (call);                  \
        if (e != CLOPTS_ERROR_NONE)   \
        {                            \
            goto fail;               \
        }                            \
    } while (0)

/*
 * Creates the terminal FLAG named long_name / short_name with help into
 * *out. Returns the declaration error, or CLOPTS_ERROR_NONE; on an error *out
 * is NULL and nothing is left to release.
 */
static clopts_error_t create_terminal(const char *long_name, char short_name, const char *help,
                                      clopts_option_t **out)
{
    clopts_error_t e;
    clopts_option_t *option = NULL;

    *out = NULL;
    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, long_name, short_name, &option));
    CHECK(clopts_option_set_terminal(option, true));
    CHECK(clopts_option_set_help(option, help));
    *out = option;
    return CLOPTS_ERROR_NONE;

fail:
    clopts_option_destroy(option);
    return e;
}

/*
 * Declares the program into *out: the program and its global options first,
 * then each command, filled and then added. Returns the declaration error
 * that stopped it, or CLOPTS_ERROR_NONE.
 */
static clopts_error_t declare(clopts_program_t **out, struct taskbook_options *opts)
{
    clopts_error_t e;
    clopts_program_t *program = NULL;
    clopts_command_t *command = NULL;
    clopts_option_t *option = NULL;
    clopts_positional_t *positional = NULL;

    CHECK(clopts_program_create("taskbook", &program));
    CHECK(clopts_program_set_description(program, "Keep a small list of tasks."));
    CHECK(clopts_option_create(CLOPTS_OPTION_COUNT, "verbose", 'v', &option));
    CHECK(clopts_option_set_count_max(option, 3));
    CHECK(clopts_option_set_help(option, "Print more details (up to 3 times)"));
    CHECK(clopts_program_add_option(program, option));
    opts->verbose = option;
    option = NULL; /* now owned by the program */
    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, "color", '\0', &option));
    CHECK(clopts_option_set_negatable(option, true));
    CHECK(clopts_option_set_help(option, "Colorize the output"));
    CHECK(clopts_program_add_option(program, option));
    opts->color = option;
    option = NULL; /* now owned by the program */
    CHECK(create_terminal("help", 'h', "Show this help", &option));
    CHECK(clopts_program_add_option(program, option));
    opts->help = option;
    option = NULL; /* now owned by the program */
    CHECK(create_terminal("version", '\0', "Show version information", &option));
    CHECK(clopts_program_add_option(program, option));
    opts->version = option;
    option = NULL; /* now owned by the program */

    CHECK(clopts_command_create("add", &command));
    CHECK(clopts_command_set_summary(command, "Add a task"));
    CHECK(clopts_command_set_description(command, "Add a task with the given title."));
    CHECK(clopts_option_create(CLOPTS_OPTION_LIST, "tag", 't', &option));
    CHECK(clopts_option_set_value_name(option, "TAG"));
    CHECK(clopts_option_set_delimiter(option, ','));
    CHECK(clopts_option_set_help(option, "Tags of the task"));
    CHECK(clopts_command_add_option(command, option));
    opts->tag = option;
    option = NULL; /* now owned by add */
    CHECK(clopts_option_create(CLOPTS_OPTION_VALUE, "priority", 'p', &option));
    CHECK(clopts_option_set_value_name(option, "LEVEL"));
    CHECK(clopts_option_set_default(option, "normal"));
    CHECK(clopts_option_set_help(option, "Priority: low, normal or high"));
    CHECK(clopts_command_add_option(command, option));
    opts->priority = option;
    option = NULL; /* now owned by add */
    CHECK(clopts_positional_create("TITLE", &positional));
    clopts_positional_set_required(positional, true);
    CHECK(clopts_positional_set_help(positional, "Title of the task"));
    CHECK(clopts_command_add_positional(command, positional));
    positional = NULL; /* now owned by add */
    CHECK(clopts_program_add_command(program, command));
    opts->add = command;
    command = NULL; /* now owned by the program */

    CHECK(clopts_command_create("list", &command));
    CHECK(clopts_command_set_summary(command, "List tasks"));
    CHECK(clopts_command_set_description(command, "List the open tasks."));
    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, "all", 'a', &option));
    CHECK(clopts_option_set_help(option, "Include finished tasks"));
    CHECK(clopts_command_add_option(command, option));
    opts->all = option;
    option = NULL; /* now owned by list */
    CHECK(clopts_option_create(CLOPTS_OPTION_VALUE, "format", '\0', &option));
    CHECK(clopts_option_set_value_name(option, "FORMAT"));
    CHECK(clopts_option_set_env(option, "TASKBOOK_FORMAT"));
    CHECK(clopts_option_set_default(option, "text"));
    CHECK(clopts_option_set_help(option, "Output format: text or json"));
    CHECK(clopts_command_add_option(command, option));
    opts->format = option;
    option = NULL; /* now owned by list */
    CHECK(clopts_program_add_command(program, command));
    opts->list = command;
    command = NULL; /* now owned by the program */

    CHECK(clopts_command_create("done", &command));
    CHECK(clopts_command_set_summary(command, "Mark tasks as finished"));
    CHECK(clopts_command_set_description(command, "Mark the given tasks as finished."));
    CHECK(clopts_positional_create("ID", &positional));
    clopts_positional_set_required(positional, true);
    clopts_positional_set_variadic(positional, true);
    CHECK(clopts_positional_set_help(positional, "Numbers of the tasks"));
    CHECK(clopts_command_add_positional(command, positional));
    positional = NULL; /* now owned by done */
    CHECK(clopts_program_add_command(program, command));
    opts->done = command;
    command = NULL; /* now owned by the program */

    CHECK(clopts_help_command_create(&command));
    CHECK(clopts_program_add_command(program, command));
    opts->help_command = command;
    command = NULL; /* now owned by the program */

    *out = program;
    return CLOPTS_ERROR_NONE;

fail:
    /* Whatever is not attached is still ours; the program owns the rest. */
    clopts_option_destroy(option);
    clopts_positional_destroy(positional);
    clopts_command_destroy(command);
    clopts_program_destroy(program);
    return e;
}

/* Runs the selected command. */
static int run(const clopts_program_t *program, const clopts_result_t *result, const struct taskbook_options *opts)
{
    const clopts_command_t *command = clopts_result_get_command(result);
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
    if (!clopts_help_find_target(program, result, &target))
    {
        fprintf(stderr, "taskbook: error: unknown command '%s'\n", clopts_result_get_positional_at(result, 0));
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
    switch (clopts_result_get_status(result))
    {
    case CLOPTS_OK:
        return run(program, result, opts);
    case CLOPTS_TERMINATED:
        if (clopts_result_get_terminal_option(result) == opts->version)
        {
            clopts_write_version(program, "1.2.3", clopts_file_write, stdout);
        }
        else
        {
            clopts_write_help(program, clopts_result_get_command(result), clopts_file_write, stdout);
        }
        return 0;
    case CLOPTS_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_get_error_command(result), clopts_file_write, stderr);
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
    clopts_error_t error;
    int code;

    error = declare(&program, &opts);
    if (error != CLOPTS_ERROR_NONE)
    {
        fprintf(stderr, "taskbook: error: invalid declaration: %s\n", clopts_error_get_message(error));
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

    /* If clopts_env_create failed, looking up TASKBOOK_FORMAT fails and the parse
       reports out of memory. */
    env = clopts_env_create();
    result = clopts_parse_with_env(program, clopts_argv_get_count(args), clopts_argv_get_values(args),
                                   clopts_env_lookup, env);
    code = finish(program, result, &opts);

    clopts_result_destroy(result);
    clopts_env_destroy(env);
    clopts_argv_destroy(args);
    clopts_program_destroy(program);
    return code;
}
