/* Declaration helpers for the unit tests (see test_decl.h). */

#include "test_decl.h"

#include <stddef.h>

clopts_decl_error_t test_decl_last = CLOPTS_DECL_NONE;
unsigned test_decl_failures = 0;

void test_decl_reset(void)
{
    test_decl_last = CLOPTS_DECL_NONE;
    test_decl_failures = 0;
}

clopts_decl_error_t test_decl_check(clopts_decl_error_t error)
{
    test_decl_last = error;
    if (error != CLOPTS_DECL_NONE)
    {
        test_decl_failures++;
    }
    return error;
}

clopts_program_t *test_decl_program(const char *name)
{
    clopts_program_t *program = NULL;

    test_decl_check(clopts_program_new(name, &program));
    return program;
}

clopts_command_t *test_decl_command(clopts_program_t *program, const char *name)
{
    clopts_command_t *command = NULL;

    test_decl_check(clopts_program_add_command(program, name, &command));
    return command;
}

clopts_option_t *test_decl_option(clopts_option_kind_t kind, const char *long_name, char short_name)
{
    clopts_option_t *option = NULL;

    test_decl_check(clopts_option_new(kind, long_name, short_name, &option));
    return option;
}

clopts_positional_t *test_decl_positional(const char *name)
{
    clopts_positional_t *positional = NULL;

    test_decl_check(clopts_positional_new(name, &positional));
    return positional;
}

clopts_option_t *test_decl_attach(clopts_command_t *command, clopts_option_t *option)
{
    test_decl_check(clopts_command_add_option(command, option));
    if (test_decl_last != CLOPTS_DECL_NONE)
    {
        clopts_option_free(option);
        return NULL;
    }
    return option;
}

clopts_positional_t *test_decl_attach_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    test_decl_check(clopts_command_add_positional(command, positional));
    if (test_decl_last != CLOPTS_DECL_NONE)
    {
        clopts_positional_free(positional);
        return NULL;
    }
    return positional;
}

clopts_option_t *test_decl_add(clopts_command_t *command, clopts_option_kind_t kind, const char *long_name,
                               char short_name)
{
    return test_decl_attach(command, test_decl_option(kind, long_name, short_name));
}

clopts_positional_t *test_decl_add_positional(clopts_command_t *command, const char *name)
{
    return test_decl_attach_positional(command, test_decl_positional(name));
}
