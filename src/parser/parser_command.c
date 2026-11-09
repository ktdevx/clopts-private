/*
 * Builder of commands, the root included (clopts/command.h): texts and
 * handler, the attach of options and positionals with the checks against the
 * other options, positionals and commands, and the scope helpers shared with
 * parser_program.c.
 *
 * Every builder function checks its arguments and the declaration rules that
 * belong to it, in a fixed order, and returns the first rule that fails.
 * Nothing is changed before every check has passed and every allocation has
 * succeeded, so a failing call leaves every handle as it was. Name rules and lookup ranges live in
 * parser_decl.c.
 */

#include "clopts/command.h"
#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

clopts_decl_error_t clopts_parser_check_frozen(const clopts_program_t *program)
{
    assert(!program->frozen);
    return program->frozen ? CLOPTS_DECL_FROZEN : CLOPTS_DECL_NONE;
}

void clopts_parser_scope_init(clopts_command_t *scope, clopts_program_t *program)
{
    scope->program = program;
    scope->summary = NULL;
    scope->description = NULL;
    clopts_core_vec_init(&scope->options, sizeof(clopts_option_t *));
    clopts_core_vec_init(&scope->positionals, sizeof(clopts_positional_t *));
    scope->handler = NULL;
    scope->userdata = NULL;
}

void clopts_parser_scope_free(clopts_command_t *scope)
{
    size_t i;

    for (i = 0; i < scope->options.len; i++)
    {
        clopts_parser_option_release(*(clopts_option_t **)clopts_core_vec_at(&scope->options, i));
    }
    clopts_core_vec_free(&scope->options);

    for (i = 0; i < scope->positionals.len; i++)
    {
        clopts_parser_positional_release(*(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, i));
    }
    clopts_core_vec_free(&scope->positionals);

    clopts_core_free(scope->summary);
    clopts_core_free(scope->description);
}

/* Common checks of a call that changes command. */
static clopts_decl_error_t command_check_call(const clopts_command_t *command)
{
    if (command == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    return clopts_parser_check_frozen(command->program);
}

clopts_decl_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    clopts_decl_error_t error = command_check_call(command);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return (clopts_core_replace_string(&command->summary, summary) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_command_set_description(clopts_command_t *command, const char *description)
{
    clopts_decl_error_t error = command_check_call(command);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return (clopts_core_replace_string(&command->description, description) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
}

clopts_decl_error_t clopts_command_set_handler(clopts_command_t *command, clopts_handler_fn handler, void *userdata)
{
    clopts_decl_error_t error = command_check_call(command);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((handler != NULL) && (command == &command->program->root) && (command->program->commands.len > 0))
    {
        return CLOPTS_DECL_MIXED_LAYOUT;
    }
    command->handler = handler;
    command->userdata = userdata;
    return CLOPTS_DECL_NONE;
}

/*
 * True when the lookup range of scope has a negatable FLAG whose long name is
 * the name_len bytes at name. For the root, each command is asked with its
 * own range (the command and the root): a name is unique within such a
 * range, while different commands may share one.
 */
static bool command_has_negatable(const clopts_command_t *scope, const char *name, size_t name_len)
{
    const clopts_program_t *program = scope->program;
    const clopts_option_t *found;
    size_t i;

    if ((scope != &program->root) || (program->commands.len == 0))
    {
        found = clopts_parser_find_named(scope, name, name_len);
        return ((found != NULL) && (found->kind == CLOPTS_PARSER_FLAG) && found->negatable);
    }
    for (i = 0; i < program->commands.len; i++)
    {
        const clopts_command_t *command = *(clopts_command_t *const *)clopts_core_vec_at(&program->commands, i);

        found = clopts_parser_find_named(command, name, name_len);
        if ((found != NULL) && (found->kind == CLOPTS_PARSER_FLAG) && found->negatable)
        {
            return true;
        }
    }
    return false;
}

/*
 * True when the lookup range of scope has an option whose long name is "no-"
 * followed by long_name. Returns -1 when the name cannot be built.
 */
static int command_has_negation_name(const clopts_command_t *scope, const char *long_name)
{
    size_t len = strlen(long_name);
    size_t size;
    char *name;
    int found;

    if (clopts_core_size_add(len, 4, &size) != 0)
    {
        return -1;
    }
    name = (char *)clopts_core_malloc(size);
    if (name == NULL)
    {
        return -1;
    }
    memcpy(name, "no-", 3);
    memcpy(name + 3, long_name, len + 1);
    found = (clopts_parser_find_named(scope, name, len + 3) != NULL) ? 1 : 0;
    clopts_core_free(name);
    return found;
}

/* Checks of clopts_command_add_option(scope, option) against scope, in order. */
static clopts_decl_error_t command_check_attach_option(const clopts_command_t *scope, const clopts_option_t *option)
{
    const char *l = option->long_name;
    const char s = option->short_name;
    int conflict;

    if (((l != NULL) && (strcmp(l, "help") == 0))
        || ((l != NULL) && (scope->program->version != NULL) && (strcmp(l, "version") == 0)) || (s == 'h'))
    {
        return CLOPTS_DECL_RESERVED_NAME;
    }
    if (clopts_parser_find_conflict(scope, l, s) != NULL)
    {
        return CLOPTS_DECL_DUPLICATE_OPTION;
    }
    if ((l != NULL) && (strncmp(l, "no-", 3) == 0) && command_has_negatable(scope, l + 3, strlen(l) - 3))
    {
        return CLOPTS_DECL_NEGATION_CONFLICT;
    }
    if (option->negatable && (l != NULL))
    {
        conflict = command_has_negation_name(scope, l);
        if (conflict < 0)
        {
            return CLOPTS_DECL_OUT_OF_MEMORY;
        }
        if (conflict > 0)
        {
            return CLOPTS_DECL_NEGATION_CONFLICT;
        }
    }
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option)
{
    clopts_decl_error_t error;

    if ((command == NULL) || (option == NULL))
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    error = clopts_parser_check_frozen(command->program);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (option->attached)
    {
        return CLOPTS_DECL_ALREADY_ATTACHED;
    }
    error = command_check_attach_option(command, option);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (clopts_core_vec_push(&command->options, &option) != 0)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    option->attached = true;
    return CLOPTS_DECL_NONE;
}

/* Checks of clopts_command_add_positional(scope, positional) against scope, in order. */
static clopts_decl_error_t command_check_attach_positional(const clopts_command_t *scope,
                                                         const clopts_positional_t *positional)
{
    const clopts_program_t *program = scope->program;
    const clopts_positional_t *last;

    if ((scope == &program->root) && (program->commands.len > 0))
    {
        return CLOPTS_DECL_MIXED_LAYOUT;
    }
    if (scope->positionals.len == 0)
    {
        return CLOPTS_DECL_NONE;
    }
    last = *(clopts_positional_t *const *)clopts_core_vec_at(&scope->positionals, scope->positionals.len - 1);
    if (last->variadic)
    {
        return CLOPTS_DECL_VARIADIC_POSITION;
    }
    /* The attached ones are in order, so checking the last one is enough. */
    if (positional->required && !last->required)
    {
        return CLOPTS_DECL_POSITIONAL_ORDER;
    }
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_command_add_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    clopts_decl_error_t error;

    if ((command == NULL) || (positional == NULL))
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    error = clopts_parser_check_frozen(command->program);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (positional->attached)
    {
        return CLOPTS_DECL_ALREADY_ATTACHED;
    }
    error = command_check_attach_positional(command, positional);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (clopts_core_vec_push(&command->positionals, &positional) != 0)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    positional->attached = true;
    return CLOPTS_DECL_NONE;
}
