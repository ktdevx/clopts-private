#include <stdbool.h>
#include <stdio.h>

#include "clopts/clopts.h"

/* Runs one declaration call; on an error, jumps to the cleanup of the function. */
#define CHECK(call)                 \
    do                              \
    {                               \
        e = (call);                 \
        if (e != CLOPTS_ERROR_NONE) \
        {                           \
            goto fail;              \
        }                           \
    } while (0)

/* Declares: hello [-g TEXT] [-h] [--version] NAME */
static clopts_error_t declare(clopts_program_t **out, clopts_option_t **greeting,
                              const clopts_option_t **version)
{
    clopts_error_t e;
    clopts_program_t *program = NULL;
    clopts_option_t *option = NULL;
    clopts_positional_t *name = NULL;

    CHECK(clopts_program_create("hello", &program));
    CHECK(clopts_program_set_description(program, "Greet someone."));

    CHECK(clopts_option_create(CLOPTS_OPTION_VALUE, "greeting", 'g', &option));
    CHECK(clopts_option_set_value_name(option, "TEXT"));
    CHECK(clopts_option_set_default(option, "Hello"));
    CHECK(clopts_option_set_help(option, "Word to greet with"));
    CHECK(clopts_program_add_option(program, option));
    *greeting = option; /* now owned by the program */
    option = NULL;

    CHECK(clopts_positional_create("NAME", &name));
    clopts_positional_set_required(name, true);
    CHECK(clopts_positional_set_help(name, "Who to greet"));
    CHECK(clopts_program_add_positional(program, name));
    name = NULL; /* now owned by the program */

    /* -h / --help and --version are terminal flags: they end the parse. */
    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, "help", 'h', &option));
    CHECK(clopts_option_set_terminal(option, true));
    CHECK(clopts_option_set_help(option, "Show this help"));
    CHECK(clopts_program_add_option(program, option));
    option = NULL;

    CHECK(clopts_option_create(CLOPTS_OPTION_FLAG, "version", '\0', &option));
    CHECK(clopts_option_set_terminal(option, true));
    CHECK(clopts_option_set_help(option, "Show version information"));
    CHECK(clopts_program_add_option(program, option));
    *version = option;
    option = NULL;

    *out = program;
    return CLOPTS_ERROR_NONE;

fail:
    /* Whatever is not attached is still ours; the program owns the rest. */
    clopts_option_destroy(option);
    clopts_positional_destroy(name);
    clopts_program_destroy(program);
    return e;
}

int main(int argc, char **argv)
{
    clopts_program_t *program = NULL;
    clopts_option_t *greeting = NULL;
    const clopts_option_t *version = NULL;
    clopts_argv_t *args;
    clopts_result_t *result;
    clopts_error_t error;
    int code = 0;

    error = declare(&program, &greeting, &version);
    if (error != CLOPTS_ERROR_NONE)
    {
        fprintf(stderr, "hello: error: invalid declaration: %s\n", clopts_error_get_message(error));
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
