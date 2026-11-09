/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * textcount: a fictional filter-style tool showing a CLOpts program without
 * subcommands.
 *
 *   textcount [-l] [-w] [--total | --no-total] [-q...] [-e NAME] [-x PATTERN,...] FILE...
 *
 * It does not read any file: it reports which files it would count, and with
 * which settings, as "<key>: <value> (<source>)" lines on stdout.
 */
#include <stdbool.h>
#include <stdio.h>

#include "clopts/clopts.h"

/* Option handles read after the parse. */
struct textcount_options
{
    const clopts_option_t *help;
    const clopts_option_t *version;
    clopts_option_t *lines;
    clopts_option_t *words;
    clopts_option_t *total;
    clopts_option_t *quiet;
    clopts_option_t *encoding;
    clopts_option_t *exclude;
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

static void print_flag(const clopts_result_t *result, const char *key, const clopts_option_t *option)
{
    printf("%s: %s", key, clopts_result_flag(result, option) ? "true" : "false");
    print_source(result, option);
}

static int run(const clopts_result_t *result, const struct textcount_options *opts)
{
    size_t count = clopts_result_positional_count(result);
    size_t i;

    printf("files:");
    for (i = 0; i < count; i++)
    {
        printf("%s %s", (i > 0) ? "," : "", clopts_result_positional_at(result, i));
    }
    fputc('\n', stdout);
    print_flag(result, "lines", opts->lines);
    print_flag(result, "words", opts->words);
    print_flag(result, "total", opts->total);
    printf("quiet: %u", clopts_result_count(result, opts->quiet));
    print_source(result, opts->quiet);
    printf("encoding: %s", clopts_result_value(result, opts->encoding));
    print_source(result, opts->encoding);
    count = clopts_result_list_count(result, opts->exclude);
    printf("exclude:");
    for (i = 0; i < count; i++)
    {
        printf("%s %s", (i > 0) ? "," : "", clopts_result_list_at(result, opts->exclude, i));
    }
    print_source(result, opts->exclude);
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
 * Declares the program into *out: the root first, then the program. Returns
 * the declaration error that stopped it, or CLOPTS_ERROR_NONE.
 */
static clopts_error_t declare(clopts_program_t **out, struct textcount_options *opts)
{
    clopts_error_t e;
    clopts_command_t *root = NULL;
    clopts_option_t *option = NULL;
    clopts_positional_t *positional = NULL;

    CHECK(clopts_command_create("textcount", &root));
    CHECK(clopts_command_set_description(root, "Count the lines and words of text files."));

    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, "lines", 'l', &option));
    CHECK(clopts_option_set_help(option, "Count lines"));
    CHECK(clopts_command_add_option(root, option));
    opts->lines = option;
    option = NULL; /* now owned by root */
    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, "words", 'w', &option));
    CHECK(clopts_option_set_help(option, "Count words"));
    CHECK(clopts_command_add_option(root, option));
    opts->words = option;
    option = NULL; /* now owned by root */
    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, "total", '\0', &option));
    CHECK(clopts_option_set_negatable(option, true));
    CHECK(clopts_option_set_help(option, "Print a total line"));
    CHECK(clopts_command_add_option(root, option));
    opts->total = option;
    option = NULL; /* now owned by root */
    CHECK(clopts_option_create(CLOPTS_OPTION_COUNT, "quiet", 'q', &option));
    CHECK(clopts_option_set_help(option, "Print less (repeat for even less)"));
    CHECK(clopts_command_add_option(root, option));
    opts->quiet = option;
    option = NULL; /* now owned by root */
    CHECK(clopts_option_create(CLOPTS_OPTION_VALUE, "encoding", 'e', &option));
    CHECK(clopts_option_set_value_name(option, "NAME"));
    CHECK(clopts_option_set_env(option, "TEXTCOUNT_ENCODING"));
    CHECK(clopts_option_set_default(option, "utf-8"));
    CHECK(clopts_option_set_help(option, "Text encoding of the files"));
    CHECK(clopts_command_add_option(root, option));
    opts->encoding = option;
    option = NULL; /* now owned by root */
    CHECK(clopts_option_create(CLOPTS_OPTION_LIST, "exclude", 'x', &option));
    CHECK(clopts_option_set_value_name(option, "PATTERN"));
    CHECK(clopts_option_set_delimiter(option, ','));
    CHECK(clopts_option_set_help(option, "Skip files matching the patterns"));
    CHECK(clopts_command_add_option(root, option));
    opts->exclude = option;
    option = NULL; /* now owned by root */
    CHECK(clopts_help_add_help_option(root, &opts->help));
    CHECK(clopts_help_add_version_option(root, &opts->version));

    CHECK(clopts_positional_create("FILE", &positional));
    CHECK(clopts_positional_set_required(positional, true));
    CHECK(clopts_positional_set_variadic(positional, true));
    CHECK(clopts_positional_set_help(positional, "Files to count (- for standard input)"));
    CHECK(clopts_command_add_positional(root, positional));
    positional = NULL; /* now owned by root */

    CHECK(clopts_program_create(root, NULL, 0, out));
    return CLOPTS_ERROR_NONE;

fail:
    /* Nothing was handed to the program: whatever is not attached is still ours. */
    clopts_option_destroy(option);
    clopts_positional_destroy(positional);
    clopts_command_destroy(root);
    return e;
}

/* Writes the text for result and returns the exit code (0, 1 or 2). */
static int finish(const clopts_program_t *program, const clopts_result_t *result,
                  const struct textcount_options *opts)
{
    switch (clopts_result_status(result))
    {
    case CLOPTS_OK:
        return run(result, opts);
    case CLOPTS_TERMINATED:
        if (clopts_result_terminal(result) == opts->version)
        {
            clopts_write_version(program, "0.4.0", clopts_file_write, stdout);
        }
        else
        {
            clopts_write_help(program, NULL, clopts_file_write, stdout);
        }
        return 0;
    case CLOPTS_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_error_command(result), clopts_file_write, stderr);
        return 2;
    case CLOPTS_NO_COMMAND:
        /* Only programs with subcommands get this; kept for the common pattern. */
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
    struct textcount_options opts = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    clopts_program_t *program = NULL;
    clopts_argv_t *args;
    clopts_env_t *env;
    clopts_result_t *result;
    clopts_error_t error;
    int code;

    error = declare(&program, &opts);
    if (error != CLOPTS_ERROR_NONE)
    {
        fprintf(stderr, "textcount: error: invalid declaration: %s\n", clopts_decl_error_message(error));
        return 1;
    }

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_get(argc, argv);
    if (clopts_argv_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("textcount: error: cannot read the command line arguments\n", stderr);
        clopts_argv_free(args);
        clopts_program_destroy(program);
        return 1;
    }

    /* If clopts_env_new failed, looking up TEXTCOUNT_ENCODING fails and the
       parse reports out of memory. */
    env = clopts_env_new();
    result = clopts_parse_with_env(program, clopts_argv_count(args), clopts_argv_values(args), clopts_env_lookup, env);
    code = finish(program, result, &opts);

    clopts_result_free(result);
    clopts_env_free(env);
    clopts_argv_free(args);
    clopts_program_destroy(program);
    return code;
}
