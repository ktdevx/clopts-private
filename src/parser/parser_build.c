/*
 * Builder: the public functions of clopts/decl.h, the immediate checks, the
 * recording of the first error, freezing and the release of a program.
 *
 * Skeleton: every builder function is a placeholder that returns NULL or
 * does nothing, and clopts_program_root() returns NULL. Task 5.3 implements
 * the builder and 6.1 the immediate checks.
 */

#include "parser/parser_internal.h"

void clopts_parser_set_probe(clopts_program_t *program, clopts_parser_probe_t *probe)
{
    (void)program;
    (void)probe;
}

/* ---- program ---- */

clopts_program_t *clopts_program_new(const char *name)
{
    (void)name;
    return NULL;
}

void clopts_program_free(clopts_program_t *program)
{
    (void)program;
}

void clopts_program_set_version(clopts_program_t *program, const char *version)
{
    (void)program;
    (void)version;
}

clopts_command_t *clopts_program_root(clopts_program_t *program)
{
    (void)program;
    return NULL;
}

clopts_command_t *clopts_program_add_command(clopts_program_t *program, const char *name)
{
    (void)program;
    (void)name;
    return NULL;
}

/* ---- command ---- */

void clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    (void)command;
    (void)summary;
}

void clopts_command_set_description(clopts_command_t *command, const char *description)
{
    (void)command;
    (void)description;
}

void clopts_command_set_handler(clopts_command_t *command, clopts_handler_fn handler, void *userdata)
{
    (void)command;
    (void)handler;
    (void)userdata;
}

clopts_option_t *clopts_command_add_flag(clopts_command_t *scope, const char *long_name, char short_name)
{
    (void)scope;
    (void)long_name;
    (void)short_name;
    return NULL;
}

clopts_option_t *clopts_command_add_count(clopts_command_t *scope, const char *long_name, char short_name)
{
    (void)scope;
    (void)long_name;
    (void)short_name;
    return NULL;
}

clopts_option_t *clopts_command_add_value(clopts_command_t *scope, const char *long_name, char short_name)
{
    (void)scope;
    (void)long_name;
    (void)short_name;
    return NULL;
}

clopts_option_t *clopts_command_add_list(clopts_command_t *scope, const char *long_name, char short_name)
{
    (void)scope;
    (void)long_name;
    (void)short_name;
    return NULL;
}

clopts_positional_t *clopts_command_add_positional(clopts_command_t *scope, const char *name)
{
    (void)scope;
    (void)name;
    return NULL;
}

/* ---- option ---- */

void clopts_option_set_help(clopts_option_t *option, const char *help)
{
    (void)option;
    (void)help;
}

void clopts_option_set_value_name(clopts_option_t *option, const char *value_name)
{
    (void)option;
    (void)value_name;
}

void clopts_option_set_env(clopts_option_t *option, const char *env)
{
    (void)option;
    (void)env;
}

void clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    (void)option;
    (void)default_value;
}

void clopts_option_set_required(clopts_option_t *option, bool required)
{
    (void)option;
    (void)required;
}

void clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    (void)option;
    (void)negatable;
}

void clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    (void)option;
    (void)delimiter;
}

void clopts_option_set_count_max(clopts_option_t *option, unsigned count_max)
{
    (void)option;
    (void)count_max;
}

/* ---- positional ---- */

void clopts_positional_set_help(clopts_positional_t *positional, const char *help)
{
    (void)positional;
    (void)help;
}

void clopts_positional_set_required(clopts_positional_t *positional, bool required)
{
    (void)positional;
    (void)required;
}

void clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic)
{
    (void)positional;
    (void)variadic;
}
