/*
 * Builder: the public functions of clopts/decl.h, the immediate checks, the
 * recording of the first error, freezing and the release of a program.
 *
 * Implemented: the common prologue of every builder call, the program block,
 * the adders, the setters, the recording of OUT_OF_MEMORY and the release.
 * Task 6.1 adds the immediate checks (the rule table of the design) to each
 * builder function.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 * Common prologue of every builder call. Returns true when the call may go
 * on: program is not NULL, not frozen (a contract violation caught by
 * assert; without assert the call does nothing) and has no recorded error.
 */
static bool build_begin(const clopts_program_t *program)
{
    if (program == NULL)
    {
        return false;
    }
    assert(!program->frozen);
    if (program->frozen)
    {
        return false;
    }
    return (program->error.status == CLOPTS_OK);
}

/* Records an allocation failure as the first error of program. */
static void build_record_oom(clopts_program_t *program)
{
    if (program->error.status == CLOPTS_OK)
    {
        program->error.status = CLOPTS_OUT_OF_MEMORY;
    }
}

/*
 * Replaces the owned copy *field with a copy of value (NULL resets it). On
 * allocation failure records OUT_OF_MEMORY and keeps the old copy.
 */
static void build_set_string(clopts_program_t *program, char **field, const char *value)
{
    char *copy;

    if (!build_begin(program))
    {
        return;
    }

    copy = clopts_parser_strdup(program->probe, value);
    if ((value != NULL) && (copy == NULL))
    {
        build_record_oom(program);
        return;
    }
    clopts_parser_free(program->probe, *field);
    *field = copy;
}

/* Sets the empty state of a scope's member vecs. */
static void build_init_scope(clopts_command_t *scope, clopts_program_t *program)
{
    scope->program = program;
    scope->summary = NULL;
    scope->description = NULL;
    clopts_core_vec_init(&scope->options, sizeof(clopts_option_t *));
    clopts_core_vec_init(&scope->positionals, sizeof(clopts_positional_t *));
    scope->handler = NULL;
    scope->userdata = NULL;
}

/* Releases the options, positionals and strings of scope (not its name). */
static void build_free_scope(clopts_parser_probe_t *probe, clopts_command_t *scope)
{
    size_t i;

    for (i = 0; i < scope->options.len; i++)
    {
        clopts_option_t *option = *(clopts_option_t **)clopts_core_vec_at(&scope->options, i);

        clopts_parser_free(probe, option->long_name);
        clopts_parser_free(probe, option->help);
        clopts_parser_free(probe, option->value_name);
        clopts_parser_free(probe, option->default_value);
        clopts_parser_free(probe, option->env);
        clopts_parser_free(probe, option);
    }
    clopts_parser_vec_free(probe, &scope->options);

    for (i = 0; i < scope->positionals.len; i++)
    {
        clopts_positional_t *positional = *(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, i);

        clopts_parser_free(probe, positional->name);
        clopts_parser_free(probe, positional->help);
        clopts_parser_free(probe, positional);
    }
    clopts_parser_vec_free(probe, &scope->positionals);

    clopts_parser_free(probe, scope->summary);
    clopts_parser_free(probe, scope->description);
}

/*
 * Adds an option of kind to scope: allocates it, links it to scope->options
 * and then copies the long name, so that everything allocated stays
 * reachable from the program on failure.
 */
static clopts_option_t *build_add_option(clopts_command_t *scope, clopts_parser_kind_t kind, const char *long_name,
                                         char short_name)
{
    clopts_program_t *program;
    clopts_option_t *option;

    if (scope == NULL)
    {
        return NULL;
    }
    program = scope->program;
    if (!build_begin(program))
    {
        return NULL;
    }

    option = (clopts_option_t *)clopts_parser_calloc(program->probe, 1, sizeof(*option));
    if (option == NULL)
    {
        build_record_oom(program);
        return NULL;
    }
    option->program = program;
    option->scope = scope;
    option->index = scope->options.len;
    option->kind = kind;
    option->long_name = NULL;
    option->short_name = short_name;
    option->help = NULL;
    option->value_name = NULL;
    option->default_value = NULL;
    option->env = NULL;
    option->delimiter = '\0';
    option->count_max = 0;
    option->required = false;
    option->negatable = false;

    if (clopts_parser_vec_push(program->probe, &scope->options, &option) != 0)
    {
        clopts_parser_free(program->probe, option);
        build_record_oom(program);
        return NULL;
    }

    option->long_name = clopts_parser_strdup(program->probe, long_name);
    if ((long_name != NULL) && (option->long_name == NULL))
    {
        build_record_oom(program);
        return NULL;
    }
    return option;
}

/* Returns true when option may be changed by a setter. */
static bool build_begin_option(const clopts_option_t *option)
{
    return ((option != NULL) && build_begin(option->program));
}

/* Returns true when positional may be changed by a setter. */
static bool build_begin_positional(const clopts_positional_t *positional)
{
    return ((positional != NULL) && build_begin(positional->program));
}

void clopts_parser_set_probe(clopts_program_t *program, clopts_parser_probe_t *probe)
{
    assert(program != NULL);

    program->probe = probe;
}

/* ---- program ---- */

clopts_program_t *clopts_program_new(const char *name)
{
    clopts_program_t *program;
    size_t name_size = 0;
    size_t bytes;
    char *name_copy = NULL;

    if (name != NULL)
    {
        if (clopts_core_size_add(strlen(name), 1, &name_size) != 0)
        {
            return NULL;
        }
    }
    if (clopts_core_size_add(sizeof(*program), name_size, &bytes) != 0)
    {
        return NULL;
    }

    /* The struct and the copy of the name are one block, released with free(). */
    program = (clopts_program_t *)malloc(bytes);
    if (program == NULL)
    {
        return NULL;
    }
    if (name != NULL)
    {
        name_copy = (char *)(program + 1);
        memcpy(name_copy, name, name_size);
    }

    build_init_scope(&program->root, program);
    program->root.name = name_copy;
    program->version = NULL;
    clopts_core_vec_init(&program->commands, sizeof(clopts_command_t *));
    program->frozen = false;
    program->error.status = CLOPTS_OK;
    program->error.kind = CLOPTS_DECL_NONE;
    program->error.command = NULL;
    program->error.option = NULL;
    program->error.positional = NULL;
    program->probe = NULL;
    return program;
}

void clopts_program_free(clopts_program_t *program)
{
    clopts_parser_probe_t *probe;
    size_t i;

    if (program == NULL)
    {
        return;
    }
    probe = program->probe;

    for (i = 0; i < program->commands.len; i++)
    {
        clopts_command_t *command = *(clopts_command_t **)clopts_core_vec_at(&program->commands, i);

        build_free_scope(probe, command);
        clopts_parser_free(probe, (void *)command->name);
        clopts_parser_free(probe, command);
    }
    clopts_parser_vec_free(probe, &program->commands);

    build_free_scope(probe, &program->root);
    clopts_parser_free(probe, program->version);
    free(program);
}

void clopts_program_set_version(clopts_program_t *program, const char *version)
{
    if (program == NULL)
    {
        return;
    }
    build_set_string(program, &program->version, version);
}

clopts_command_t *clopts_program_root(clopts_program_t *program)
{
    if (program == NULL)
    {
        return NULL;
    }
    return &program->root;
}

clopts_command_t *clopts_program_add_command(clopts_program_t *program, const char *name)
{
    clopts_command_t *command;
    char *name_copy;

    if (!build_begin(program))
    {
        return NULL;
    }

    command = (clopts_command_t *)clopts_parser_calloc(program->probe, 1, sizeof(*command));
    if (command == NULL)
    {
        build_record_oom(program);
        return NULL;
    }
    build_init_scope(command, program);
    command->name = NULL;

    if (clopts_parser_vec_push(program->probe, &program->commands, &command) != 0)
    {
        clopts_parser_free(program->probe, command);
        build_record_oom(program);
        return NULL;
    }

    name_copy = clopts_parser_strdup(program->probe, name);
    if ((name != NULL) && (name_copy == NULL))
    {
        build_record_oom(program);
        return NULL;
    }
    command->name = name_copy;
    return command;
}

/* ---- command ---- */

void clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    if (command == NULL)
    {
        return;
    }
    build_set_string(command->program, &command->summary, summary);
}

void clopts_command_set_description(clopts_command_t *command, const char *description)
{
    if (command == NULL)
    {
        return;
    }
    build_set_string(command->program, &command->description, description);
}

void clopts_command_set_handler(clopts_command_t *command, clopts_handler_fn handler, void *userdata)
{
    if ((command == NULL) || (!build_begin(command->program)))
    {
        return;
    }
    command->handler = handler;
    command->userdata = userdata;
}

clopts_option_t *clopts_command_add_flag(clopts_command_t *scope, const char *long_name, char short_name)
{
    return build_add_option(scope, CLOPTS_PARSER_FLAG, long_name, short_name);
}

clopts_option_t *clopts_command_add_count(clopts_command_t *scope, const char *long_name, char short_name)
{
    return build_add_option(scope, CLOPTS_PARSER_COUNT, long_name, short_name);
}

clopts_option_t *clopts_command_add_value(clopts_command_t *scope, const char *long_name, char short_name)
{
    return build_add_option(scope, CLOPTS_PARSER_VALUE, long_name, short_name);
}

clopts_option_t *clopts_command_add_list(clopts_command_t *scope, const char *long_name, char short_name)
{
    return build_add_option(scope, CLOPTS_PARSER_LIST, long_name, short_name);
}

clopts_positional_t *clopts_command_add_positional(clopts_command_t *scope, const char *name)
{
    clopts_program_t *program;
    clopts_positional_t *positional;

    if (scope == NULL)
    {
        return NULL;
    }
    program = scope->program;
    if (!build_begin(program))
    {
        return NULL;
    }

    positional = (clopts_positional_t *)clopts_parser_calloc(program->probe, 1, sizeof(*positional));
    if (positional == NULL)
    {
        build_record_oom(program);
        return NULL;
    }
    positional->program = program;
    positional->scope = scope;
    positional->index = scope->positionals.len;
    positional->name = NULL;
    positional->help = NULL;
    positional->required = false;
    positional->variadic = false;

    if (clopts_parser_vec_push(program->probe, &scope->positionals, &positional) != 0)
    {
        clopts_parser_free(program->probe, positional);
        build_record_oom(program);
        return NULL;
    }

    positional->name = clopts_parser_strdup(program->probe, name);
    if ((name != NULL) && (positional->name == NULL))
    {
        build_record_oom(program);
        return NULL;
    }
    return positional;
}

/* ---- option ---- */

void clopts_option_set_help(clopts_option_t *option, const char *help)
{
    if (option == NULL)
    {
        return;
    }
    build_set_string(option->program, &option->help, help);
}

void clopts_option_set_value_name(clopts_option_t *option, const char *value_name)
{
    if (option == NULL)
    {
        return;
    }
    build_set_string(option->program, &option->value_name, value_name);
}

void clopts_option_set_env(clopts_option_t *option, const char *env)
{
    if (option == NULL)
    {
        return;
    }
    build_set_string(option->program, &option->env, env);
}

void clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    if (option == NULL)
    {
        return;
    }
    build_set_string(option->program, &option->default_value, default_value);
}

void clopts_option_set_required(clopts_option_t *option, bool required)
{
    if (!build_begin_option(option))
    {
        return;
    }
    option->required = required;
}

void clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    if (!build_begin_option(option))
    {
        return;
    }
    option->negatable = negatable;
}

void clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    if (!build_begin_option(option))
    {
        return;
    }
    option->delimiter = delimiter;
}

void clopts_option_set_count_max(clopts_option_t *option, unsigned count_max)
{
    if (!build_begin_option(option))
    {
        return;
    }
    option->count_max = count_max;
}

/* ---- positional ---- */

void clopts_positional_set_help(clopts_positional_t *positional, const char *help)
{
    if (positional == NULL)
    {
        return;
    }
    build_set_string(positional->program, &positional->help, help);
}

void clopts_positional_set_required(clopts_positional_t *positional, bool required)
{
    if (!build_begin_positional(positional))
    {
        return;
    }
    positional->required = required;
}

void clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic)
{
    if (!build_begin_positional(positional))
    {
        return;
    }
    positional->variadic = variadic;
}
