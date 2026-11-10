#include <stdbool.h>
#include <stdio.h>

#include "clopts/clopts.h"

int main(int argc, char **argv)
{
    clopts_program_t *program;
    clopts_option_t *greeting;
    clopts_option_t *version;
    clopts_positional_t *name;
    clopts_argv_t *args;
    clopts_result_t *result;
    clopts_error_t error;
    int code = 0;

    /* Declares: hello [-g TEXT] [-h] [--version] NAME */
    program = clopts_program_create("hello");
    clopts_program_set_description(program, "Greet someone.");

    greeting = clopts_program_add_option(program, CLOPTS_OPTION_VALUE, "greeting", 'g');
    clopts_option_set_value_name(greeting, "TEXT");
    clopts_option_set_default(greeting, "Hello");
    clopts_option_set_help(greeting, "Word to greet with");

    name = clopts_program_add_positional(program, "NAME");
    clopts_positional_set_required(name, true);
    clopts_positional_set_help(name, "Who to greet");

    /* -h / --help and --version are terminal flags: they end the parse. */
    clopts_program_add_help_option(program);
    version = clopts_program_add_version_option(program);

    /* The program keeps the first declaration error; one check covers every call above. */
    error = clopts_program_get_error(program);
    if (error != CLOPTS_ERROR_NONE)
    {
        fprintf(stderr, "hello: error: invalid declaration: %s\n", clopts_error_get_message(error));
        clopts_program_destroy(program);
        return 1;
    }

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_create(argc, argv);
    if (clopts_argv_get_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("hello: error: cannot read the command line arguments\n", stderr);
        clopts_argv_destroy(args);
        clopts_program_destroy(program);
        return 1;
    }

    result = clopts_parse(program, clopts_argv_get_count(args), clopts_argv_get_values(args));
    switch (clopts_result_get_status(result))
    {
    case CLOPTS_PARSE_OK:
        printf("greeting: %s (%s)\n", clopts_result_get_value(result, greeting),
               clopts_result_get_source(result, greeting) == CLOPTS_SOURCE_DEFAULT ? "default" : "explicit");
        printf("name: %s\n", clopts_result_get_positional_at(result, 0));
        break;
    case CLOPTS_PARSE_TERMINATED: /* -h / --help or --version */
        if (clopts_result_get_terminal_option(result) == version)
        {
            clopts_write_version(program, "1.0.0", clopts_file_write, stdout);
        }
        else
        {
            clopts_write_help(program, NULL, clopts_file_write, stdout);
        }
        break;
    case CLOPTS_PARSE_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_get_command(result), clopts_file_write, stderr);
        code = 2;
        break;
    case CLOPTS_PARSE_NO_COMMAND:
        /* Only programs with subcommands get this; kept for the common pattern. */
        clopts_write_usage(program, NULL, clopts_file_write, stderr);
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        code = 2;
        break;
    default: /* CLOPTS_PARSE_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        code = 1;
        break;
    }

    clopts_result_destroy(result);
    clopts_argv_destroy(args);
    clopts_program_destroy(program);
    return code;
}
