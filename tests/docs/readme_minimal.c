#include <stdbool.h>
#include <stdio.h>

#include "clopts/clopts.h"

/* Keeps the first declaration error; once one is kept, later calls are ignored. */
static bool check(clopts_decl_error_t *first, clopts_decl_error_t error)
{
    if (*first == CLOPTS_DECL_NONE)
    {
        *first = error;
    }
    return *first == CLOPTS_DECL_NONE;
}

/* Declares: hello [-g TEXT] NAME */
static clopts_decl_error_t declare(clopts_program_t *program, clopts_option_t **greeting)
{
    clopts_decl_error_t e = CLOPTS_DECL_NONE;
    clopts_command_t *root = clopts_program_root(program);
    clopts_option_t *option = NULL;
    clopts_positional_t *name = NULL;

    check(&e, clopts_program_set_version(program, "1.0.0"));
    check(&e, clopts_command_set_description(root, "Greet someone."));

    check(&e, clopts_option_new(CLOPTS_OPTION_VALUE, "greeting", 'g', &option));
    check(&e, clopts_option_set_value_name(option, "TEXT"));
    check(&e, clopts_option_set_default(option, "Hello"));
    check(&e, clopts_option_set_help(option, "Word to greet with"));
    if (check(&e, clopts_command_add_option(root, option)))
    {
        *greeting = option; /* now owned by root */
    }
    else
    {
        clopts_option_free(option); /* not attached: still ours */
    }

    check(&e, clopts_positional_new("NAME", &name));
    check(&e, clopts_positional_set_required(name, true));
    check(&e, clopts_positional_set_help(name, "Who to greet"));
    if (!check(&e, clopts_command_add_positional(root, name)))
    {
        clopts_positional_free(name);
    }
    return e;
}

int main(int argc, char **argv)
{
    clopts_program_t *program;
    clopts_option_t *greeting = NULL;
    clopts_argv_t *args;
    clopts_result_t *result;
    clopts_decl_error_t error;
    int code = 0;

    error = clopts_program_new("hello", &program);
    if (error == CLOPTS_DECL_NONE)
    {
        error = declare(program, &greeting);
    }
    if (error != CLOPTS_DECL_NONE)
    {
        fprintf(stderr, "hello: error: invalid declaration: %s\n", clopts_decl_error_message(error));
        clopts_program_free(program);
        return 1;
    }

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
    default: /* CLOPTS_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        code = 1;
        break;
    }

    clopts_result_free(result);
    clopts_argv_free(args);
    clopts_program_free(program);
    return code;
}
