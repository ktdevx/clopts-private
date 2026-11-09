#include <stdio.h>

#include "clopts/clopts.h"

int main(int argc, char **argv)
{
    clopts_program_t *program = clopts_program_new("hello");
    clopts_command_t *root;
    clopts_option_t *greeting;
    clopts_positional_t *name;
    clopts_argv_t *args;
    clopts_result_t *result;
    int code = 0;

    if (program == NULL)
    {
        fputs("hello: error: out of memory\n", stderr);
        return 1;
    }
    root = clopts_program_root(program);

    /* Declare: hello [-g TEXT] NAME */
    clopts_program_set_version(program, "1.0.0");
    clopts_command_set_description(root, "Greet someone.");
    greeting = clopts_command_add_value(root, "greeting", 'g');
    clopts_option_set_value_name(greeting, "TEXT");
    clopts_option_set_default(greeting, "Hello");
    clopts_option_set_help(greeting, "Word to greet with");
    name = clopts_command_add_positional(root, "NAME");
    clopts_positional_set_required(name, true);
    clopts_positional_set_help(name, "Who to greet");

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_get(argc, argv);
    if (clopts_argv_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("hello: error: cannot read the command line arguments\n", stderr);
        clopts_argv_free(args);
        clopts_program_free(program);
        return 1;
    }

    result = clopts_parse(program, clopts_argv_count(args), clopts_argv_values(args));
    switch (clopts_result_status(result))
    {
    case CLOPTS_OK:
        printf("greeting: %s (%s)\n", clopts_result_value(result, greeting),
               clopts_result_source(result, greeting) == CLOPTS_SOURCE_DEFAULT ? "default" : "explicit");
        printf("name: %s\n", clopts_result_positional_at(result, 0));
        break;
    case CLOPTS_HELP:
        clopts_write_help(program, clopts_result_command(result), clopts_file_write, stdout);
        break;
    case CLOPTS_VERSION:
        clopts_write_version(program, clopts_file_write, stdout);
        break;
    case CLOPTS_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_error_command(result), clopts_file_write, stderr);
        code = 2;
        break;
    case CLOPTS_NO_COMMAND:
        /* Only programs with subcommands get this; kept for the common pattern. */
        clopts_write_usage(program, NULL, clopts_file_write, stderr);
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        code = 2;
        break;
    default: /* CLOPTS_DECL_ERROR, CLOPTS_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        code = 1;
        break;
    }

    clopts_result_free(result);
    clopts_argv_free(args);
    clopts_program_free(program);
    return code;
}
