/*
 * textcount: a fictional filter-style tool showing a CLOpts program without
 * subcommands.
 *
 *   textcount [-l] [-w] [--total | --no-total] [-q...] [-e NAME] [-x PATTERN,...] FILE...
 *
 * It does not read any file: it reports which files it would count, and with
 * which settings, as "<key>: <value> (<source>)" lines on stdout.
 */
#include <stdio.h>

#include "clopts/clopts.h"

/* Option handles read by the handler (passed as userdata). */
struct textcount_options
{
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

static int run(const clopts_result_t *result, void *userdata)
{
    const struct textcount_options *opts = userdata;
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

/* Declares the program. Errors are recorded in it and reported by the parse. */
static void declare(clopts_program_t *program, struct textcount_options *opts)
{
    clopts_command_t *root = clopts_program_root(program);
    clopts_positional_t *positional;

    clopts_program_set_version(program, "0.4.0");
    clopts_command_set_description(root, "Count the lines and words of text files.");
    clopts_command_set_handler(root, run, opts);

    opts->lines = clopts_command_add_flag(root, "lines", 'l');
    clopts_option_set_help(opts->lines, "Count lines");
    opts->words = clopts_command_add_flag(root, "words", 'w');
    clopts_option_set_help(opts->words, "Count words");
    opts->total = clopts_command_add_flag(root, "total", '\0');
    clopts_option_set_negatable(opts->total, true);
    clopts_option_set_help(opts->total, "Print a total line");
    opts->quiet = clopts_command_add_count(root, "quiet", 'q');
    clopts_option_set_help(opts->quiet, "Print less (repeat for even less)");
    opts->encoding = clopts_command_add_value(root, "encoding", 'e');
    clopts_option_set_value_name(opts->encoding, "NAME");
    clopts_option_set_env(opts->encoding, "TEXTCOUNT_ENCODING");
    clopts_option_set_default(opts->encoding, "utf-8");
    clopts_option_set_help(opts->encoding, "Text encoding of the files");
    opts->exclude = clopts_command_add_list(root, "exclude", 'x');
    clopts_option_set_value_name(opts->exclude, "PATTERN");
    clopts_option_set_delimiter(opts->exclude, ',');
    clopts_option_set_help(opts->exclude, "Skip files matching the patterns");

    positional = clopts_command_add_positional(root, "FILE");
    clopts_positional_set_required(positional, true);
    clopts_positional_set_variadic(positional, true);
    clopts_positional_set_help(positional, "Files to count (- for standard input)");
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
        /* Only programs with subcommands get this; kept for the common pattern. */
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
    struct textcount_options opts = {NULL, NULL, NULL, NULL, NULL, NULL};
    clopts_program_t *program;
    clopts_argv_t *args;
    clopts_env_t *env;
    clopts_result_t *result;
    int code;

    program = clopts_program_new("textcount");
    if (program == NULL)
    {
        fputs("textcount: error: out of memory\n", stderr);
        return 1;
    }
    declare(program, &opts);

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_get(argc, argv);
    if (clopts_argv_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("textcount: error: cannot read the command line arguments\n", stderr);
        clopts_argv_free(args);
        clopts_program_free(program);
        return 1;
    }

    /* If clopts_env_new failed, looking up TEXTCOUNT_ENCODING fails and the
       parse reports out of memory. */
    env = clopts_env_new();
    result = clopts_parse_with_env(program, clopts_argv_count(args), clopts_argv_values(args), clopts_env_lookup, env);
    code = finish(program, result);

    clopts_result_free(result);
    clopts_env_free(env);
    clopts_argv_free(args);
    clopts_program_free(program);
    return code;
}
