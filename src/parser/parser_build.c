/*
 * Builder: the public functions of clopts/decl.h, their checks and the
 * release of a program.
 *
 * Every builder function checks its arguments and the declaration rules that
 * belong to it, in a fixed order, and returns the first rule that fails.
 * Nothing is changed before every check has passed and every allocation has
 * succeeded, so a failing call leaves every handle as it was. Constructors
 * and setters of options and positionals check what depends on the object
 * alone (its kind and attributes); the attach functions check what depends on
 * the command and the program (names, order, layout). Resetting an attribute
 * (NULL, false, '\0' or 0) is not checked. Name rules and lookup ranges live
 * in parser_decl.c.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/*
 * Common check of every call that changes program: changing a frozen program
 * is a contract violation caught by assert; without assert the call gives
 * CLOPTS_DECL_FROZEN.
 */
static clopts_decl_error_t build_check_frozen(const clopts_program_t *program)
{
    assert(!program->frozen);
    return program->frozen ? CLOPTS_DECL_FROZEN : CLOPTS_DECL_NONE;
}

/* True for the kinds that take no value (FLAG / COUNT). */
static bool build_takes_no_value(clopts_parser_kind_t kind)
{
    return ((kind == CLOPTS_PARSER_FLAG) || (kind == CLOPTS_PARSER_COUNT));
}

/*
 * Replaces the owned copy *field with a copy of value (NULL resets it). On
 * allocation failure keeps the old copy and returns OUT_OF_MEMORY.
 */
static clopts_decl_error_t build_replace_string(char **field, const char *value)
{
    char *copy;

    copy = clopts_core_strdup(value);
    if ((value != NULL) && (copy == NULL))
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    clopts_core_free(*field);
    *field = copy;
    return CLOPTS_DECL_NONE;
}

/* Releases an option and its strings. */
static void build_free_option(clopts_option_t *option)
{
    clopts_core_free(option->long_name);
    clopts_core_free(option->help);
    clopts_core_free(option->value_name);
    clopts_core_free(option->default_value);
    clopts_core_free(option->env);
    clopts_core_free(option);
}

/* Releases a positional and its strings. */
static void build_free_positional(clopts_positional_t *positional)
{
    clopts_core_free(positional->name);
    clopts_core_free(positional->help);
    clopts_core_free(positional);
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
static void build_free_scope(clopts_command_t *scope)
{
    size_t i;

    for (i = 0; i < scope->options.len; i++)
    {
        build_free_option(*(clopts_option_t **)clopts_core_vec_at(&scope->options, i));
    }
    clopts_core_vec_free(&scope->options);

    for (i = 0; i < scope->positionals.len; i++)
    {
        build_free_positional(*(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, i));
    }
    clopts_core_vec_free(&scope->positionals);

    clopts_core_free(scope->summary);
    clopts_core_free(scope->description);
}

/* ---- program ---- */

clopts_decl_error_t clopts_program_new(const char *name, clopts_program_t **out)
{
    clopts_program_t *program;
    size_t name_size;
    size_t bytes;
    char *name_copy;

    if (out == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    *out = NULL;
    if (!clopts_parser_valid_command_name(name))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
    if ((clopts_core_size_add(strlen(name), 1, &name_size) != 0)
        || (clopts_core_size_add(sizeof(*program), name_size, &bytes) != 0))
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }

    /* The struct and the copy of the name are one block, released with clopts_core_free(). */
    program = (clopts_program_t *)clopts_core_malloc(bytes);
    if (program == NULL)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    name_copy = (char *)(program + 1);
    memcpy(name_copy, name, name_size);

    build_init_scope(&program->root, program);
    program->root.name = name_copy;
    program->version = NULL;
    clopts_core_vec_init(&program->commands, sizeof(clopts_command_t *));
    program->frozen = false;
    *out = program;
    return CLOPTS_DECL_NONE;
}

void clopts_program_free(clopts_program_t *program)
{
    size_t i;

    if (program == NULL)
    {
        return;
    }

    for (i = 0; i < program->commands.len; i++)
    {
        clopts_command_t *command = *(clopts_command_t **)clopts_core_vec_at(&program->commands, i);

        build_free_scope(command);
        clopts_core_free((void *)command->name);
        clopts_core_free(command);
    }
    clopts_core_vec_free(&program->commands);

    build_free_scope(&program->root);
    clopts_core_free(program->version);
    clopts_core_free(program);
}

clopts_decl_error_t clopts_program_set_version(clopts_program_t *program, const char *version)
{
    clopts_decl_error_t error;

    if (program == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    error = build_check_frozen(program);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    /* The lookup range of the root covers every command. */
    if ((version != NULL) && (clopts_parser_find_conflict(&program->root, "version", '\0') != NULL))
    {
        return CLOPTS_DECL_RESERVED_NAME;
    }
    return build_replace_string(&program->version, version);
}

clopts_command_t *clopts_program_root(clopts_program_t *program)
{
    if (program == NULL)
    {
        return NULL;
    }
    return &program->root;
}

/* Checks of clopts_program_add_command(name), in order. */
static clopts_decl_error_t build_check_command(const clopts_program_t *program, const char *name)
{
    if (!clopts_parser_valid_command_name(name))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
    if (strcmp(name, "help") == 0)
    {
        return CLOPTS_DECL_RESERVED_NAME;
    }
    if (clopts_parser_find_command(program, name) != NULL)
    {
        return CLOPTS_DECL_DUPLICATE_COMMAND;
    }
    if ((program->root.positionals.len > 0) || (program->root.handler != NULL))
    {
        return CLOPTS_DECL_MIXED_LAYOUT;
    }
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_program_add_command(clopts_program_t *program, const char *name, clopts_command_t **out)
{
    clopts_command_t *command;
    char *name_copy;
    clopts_decl_error_t error;

    if (out != NULL)
    {
        *out = NULL;
    }
    if ((program == NULL) || (out == NULL))
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    error = build_check_frozen(program);
    if (error == CLOPTS_DECL_NONE)
    {
        error = build_check_command(program, name);
    }
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    command = (clopts_command_t *)clopts_core_calloc(1, sizeof(*command));
    name_copy = clopts_core_strdup(name);
    if ((command == NULL) || (name_copy == NULL))
    {
        clopts_core_free(command);
        clopts_core_free(name_copy);
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    build_init_scope(command, program);
    command->name = name_copy;
    if (clopts_core_vec_push(&program->commands, &command) != 0)
    {
        clopts_core_free(name_copy);
        clopts_core_free(command);
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    *out = command;
    return CLOPTS_DECL_NONE;
}

/* ---- command ---- */

/* Common checks of a call that changes command. */
static clopts_decl_error_t build_check_command_call(const clopts_command_t *command)
{
    if (command == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    return build_check_frozen(command->program);
}

clopts_decl_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary)
{
    clopts_decl_error_t error = build_check_command_call(command);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return build_replace_string(&command->summary, summary);
}

clopts_decl_error_t clopts_command_set_description(clopts_command_t *command, const char *description)
{
    clopts_decl_error_t error = build_check_command_call(command);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return build_replace_string(&command->description, description);
}

clopts_decl_error_t clopts_command_set_handler(clopts_command_t *command, clopts_handler_fn handler, void *userdata)
{
    clopts_decl_error_t error = build_check_command_call(command);

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
static bool build_has_negatable(const clopts_command_t *scope, const char *name, size_t name_len)
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
static int build_has_negation_name(const clopts_command_t *scope, const char *long_name)
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
static clopts_decl_error_t build_check_attach_option(const clopts_command_t *scope, const clopts_option_t *option)
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
    if ((l != NULL) && (strncmp(l, "no-", 3) == 0) && build_has_negatable(scope, l + 3, strlen(l) - 3))
    {
        return CLOPTS_DECL_NEGATION_CONFLICT;
    }
    if (option->negatable && (l != NULL))
    {
        conflict = build_has_negation_name(scope, l);
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
    error = build_check_frozen(command->program);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (option->attached)
    {
        return CLOPTS_DECL_ALREADY_ATTACHED;
    }
    error = build_check_attach_option(command, option);
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
static clopts_decl_error_t build_check_attach_positional(const clopts_command_t *scope,
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
    error = build_check_frozen(command->program);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (positional->attached)
    {
        return CLOPTS_DECL_ALREADY_ATTACHED;
    }
    error = build_check_attach_positional(command, positional);
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

/* ---- option ---- */

/* True for the values of clopts_option_kind_t. */
static bool build_valid_kind(clopts_option_kind_t kind)
{
    return ((kind == CLOPTS_OPTION_FLAG) || (kind == CLOPTS_OPTION_COUNT) || (kind == CLOPTS_OPTION_VALUE)
            || (kind == CLOPTS_OPTION_LIST));
}

/* Checks of clopts_option_new(kind, l, s), in order. */
static clopts_decl_error_t build_check_option(clopts_option_kind_t kind, const char *l, char s)
{
    if (!build_valid_kind(kind))
    {
        return CLOPTS_DECL_INVALID_KIND;
    }
    if ((l == NULL) && (s == '\0'))
    {
        return CLOPTS_DECL_NO_NAME;
    }
    if (((l != NULL) && (!clopts_parser_valid_long_name(l))) || ((s != '\0') && (!clopts_parser_valid_short_name(s))))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_new(clopts_option_kind_t kind, const char *long_name, char short_name,
                                      clopts_option_t **out)
{
    clopts_option_t *option;
    clopts_decl_error_t error;

    if (out == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    *out = NULL;
    error = build_check_option(kind, long_name, short_name);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }

    option = (clopts_option_t *)clopts_core_calloc(1, sizeof(*option));
    if (option == NULL)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    option->attached = false;
    option->kind = kind;
    option->long_name = clopts_core_strdup(long_name);
    if ((long_name != NULL) && (option->long_name == NULL))
    {
        clopts_core_free(option);
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    option->short_name = short_name;
    option->help = NULL;
    option->value_name = NULL;
    option->default_value = NULL;
    option->env = NULL;
    option->delimiter = '\0';
    option->count_max = 0;
    option->required = false;
    option->negatable = false;
    *out = option;
    return CLOPTS_DECL_NONE;
}

void clopts_option_free(clopts_option_t *option)
{
    if (option == NULL)
    {
        return;
    }
    assert(!option->attached && "an attached option is owned by its command");
    if (option->attached)
    {
        return;
    }
    build_free_option(option);
}

/* Common checks of an option setter. */
static clopts_decl_error_t build_check_option_call(const clopts_option_t *option)
{
    if (option == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    return option->attached ? CLOPTS_DECL_ALREADY_ATTACHED : CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_help(clopts_option_t *option, const char *help)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return build_replace_string(&option->help, help);
}

clopts_decl_error_t clopts_option_set_value_name(clopts_option_t *option, const char *value_name)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return build_replace_string(&option->value_name, value_name);
}

clopts_decl_error_t clopts_option_set_env(clopts_option_t *option, const char *env)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((env != NULL) && build_takes_no_value(option->kind))
    {
        return CLOPTS_DECL_ENV_NOT_ALLOWED;
    }
    if ((env != NULL) && (env[0] == '\0'))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
    return build_replace_string(&option->env, env);
}

clopts_decl_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((default_value != NULL) && build_takes_no_value(option->kind))
    {
        return CLOPTS_DECL_DEFAULT_NOT_ALLOWED;
    }
    if ((default_value != NULL) && option->required)
    {
        return CLOPTS_DECL_REQUIRED_WITH_DEFAULT;
    }
    return build_replace_string(&option->default_value, default_value);
}

clopts_decl_error_t clopts_option_set_required(clopts_option_t *option, bool required)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (required && build_takes_no_value(option->kind))
    {
        return CLOPTS_DECL_REQUIRED_NOT_ALLOWED;
    }
    if (required && (option->default_value != NULL))
    {
        return CLOPTS_DECL_REQUIRED_WITH_DEFAULT;
    }
    option->required = required;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if (negatable && (option->kind != CLOPTS_PARSER_FLAG))
    {
        return CLOPTS_DECL_NOT_NEGATABLE;
    }
    option->negatable = negatable;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((delimiter != '\0') && (option->kind != CLOPTS_PARSER_LIST))
    {
        return CLOPTS_DECL_ATTRIBUTE_KIND;
    }
    option->delimiter = delimiter;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_option_set_count_max(clopts_option_t *option, unsigned count_max)
{
    clopts_decl_error_t error = build_check_option_call(option);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    if ((count_max != 0) && (option->kind != CLOPTS_PARSER_COUNT))
    {
        return CLOPTS_DECL_ATTRIBUTE_KIND;
    }
    option->count_max = count_max;
    return CLOPTS_DECL_NONE;
}

/* ---- positional ---- */

clopts_decl_error_t clopts_positional_new(const char *name, clopts_positional_t **out)
{
    clopts_positional_t *positional;

    if (out == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    *out = NULL;
    if ((name == NULL) || (name[0] == '\0'))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }

    positional = (clopts_positional_t *)clopts_core_calloc(1, sizeof(*positional));
    if (positional == NULL)
    {
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    positional->attached = false;
    positional->name = clopts_core_strdup(name);
    if (positional->name == NULL)
    {
        clopts_core_free(positional);
        return CLOPTS_DECL_OUT_OF_MEMORY;
    }
    positional->help = NULL;
    positional->required = false;
    positional->variadic = false;
    *out = positional;
    return CLOPTS_DECL_NONE;
}

void clopts_positional_free(clopts_positional_t *positional)
{
    if (positional == NULL)
    {
        return;
    }
    assert(!positional->attached && "an attached positional is owned by its command");
    if (positional->attached)
    {
        return;
    }
    build_free_positional(positional);
}

/* Common checks of a positional setter. */
static clopts_decl_error_t build_check_positional_call(const clopts_positional_t *positional)
{
    if (positional == NULL)
    {
        return CLOPTS_DECL_INVALID_ARGUMENT;
    }
    return positional->attached ? CLOPTS_DECL_ALREADY_ATTACHED : CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_positional_set_help(clopts_positional_t *positional, const char *help)
{
    clopts_decl_error_t error = build_check_positional_call(positional);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    return build_replace_string(&positional->help, help);
}

clopts_decl_error_t clopts_positional_set_required(clopts_positional_t *positional, bool required)
{
    clopts_decl_error_t error = build_check_positional_call(positional);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    positional->required = required;
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic)
{
    clopts_decl_error_t error = build_check_positional_call(positional);

    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    positional->variadic = variadic;
    return CLOPTS_DECL_NONE;
}
