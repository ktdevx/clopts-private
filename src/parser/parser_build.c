/*
 * Builder: the public functions of clopts/decl.h, the immediate checks, the
 * recording of the first error, freezing and the release of a program.
 *
 * Every builder function runs the common prologue, then the rows of the
 * immediate check table of the design that belong to it, in table order. The
 * first rule that holds is recorded as the declaration error of the program
 * (the root is reported as NULL). An adder still links the new object and
 * returns it; a setter leaves the attribute unchanged. Resetting an attribute
 * (NULL, false, '\0' or 0) is not checked, except that making a positional
 * optional is checked against the required positionals after it. Name rules
 * and lookup ranges live in parser_decl.c.
 */

#include "parser/parser_internal.h"

#include <assert.h>
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
 * Records a declaration error as the first error of program. command is the
 * scope of the offending handle; the root is stored as NULL.
 */
static void build_record_decl(clopts_program_t *program, clopts_decl_error_t kind, const clopts_command_t *command,
                              const clopts_option_t *option, const clopts_positional_t *positional)
{
    if (program->error.status != CLOPTS_OK)
    {
        return;
    }
    program->error.status = CLOPTS_DECL_ERROR;
    program->error.kind = kind;
    program->error.command = (command == &program->root) ? NULL : (clopts_command_t *)command;
    program->error.option = (clopts_option_t *)option;
    program->error.positional = (clopts_positional_t *)positional;
}

/* True for the kinds that take no value (FLAG / COUNT). */
static bool build_takes_no_value(clopts_parser_kind_t kind)
{
    return ((kind == CLOPTS_PARSER_FLAG) || (kind == CLOPTS_PARSER_COUNT));
}

/*
 * Replaces the owned copy *field with a copy of value (NULL resets it),
 * without the prologue. On allocation failure records OUT_OF_MEMORY and keeps
 * the old copy.
 */
static void build_replace_string(clopts_program_t *program, char **field, const char *value)
{
    char *copy;

    copy = clopts_core_strdup(value);
    if ((value != NULL) && (copy == NULL))
    {
        build_record_oom(program);
        return;
    }
    clopts_core_free(*field);
    *field = copy;
}

/* Replaces the owned copy *field with a copy of value after the prologue. */
static void build_set_string(clopts_program_t *program, char **field, const char *value)
{
    if (!build_begin(program))
    {
        return;
    }
    build_replace_string(program, field, value);
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
        clopts_option_t *option = *(clopts_option_t **)clopts_core_vec_at(&scope->options, i);

        clopts_core_free(option->long_name);
        clopts_core_free(option->help);
        clopts_core_free(option->value_name);
        clopts_core_free(option->default_value);
        clopts_core_free(option->env);
        clopts_core_free(option);
    }
    clopts_core_vec_free(&scope->options);

    for (i = 0; i < scope->positionals.len; i++)
    {
        clopts_positional_t *positional = *(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, i);

        clopts_core_free(positional->name);
        clopts_core_free(positional->help);
        clopts_core_free(positional);
    }
    clopts_core_vec_free(&scope->positionals);

    clopts_core_free(scope->summary);
    clopts_core_free(scope->description);
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

/* Rows of clopts_command_add_<kind>(scope, l, s), in table order. */
static clopts_decl_error_t build_check_option(const clopts_command_t *scope, const char *l, char s)
{
    if ((l == NULL) && (s == '\0'))
    {
        return CLOPTS_DECL_NO_NAME;
    }
    if (((l != NULL) && (!clopts_parser_valid_long_name(l))) || ((s != '\0') && (!clopts_parser_valid_short_name(s))))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
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
    return CLOPTS_DECL_NONE;
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
    clopts_decl_error_t error;

    if (scope == NULL)
    {
        return NULL;
    }
    program = scope->program;
    if (!build_begin(program))
    {
        return NULL;
    }
    /* Checked before linking, so that the new option does not match itself. */
    error = build_check_option(scope, long_name, short_name);

    option = (clopts_option_t *)clopts_core_calloc(1, sizeof(*option));
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

    if (clopts_core_vec_push(&scope->options, &option) != 0)
    {
        clopts_core_free(option);
        build_record_oom(program);
        return NULL;
    }

    option->long_name = clopts_core_strdup(long_name);
    if ((long_name != NULL) && (option->long_name == NULL))
    {
        build_record_oom(program);
        return NULL;
    }
    if (error != CLOPTS_DECL_NONE)
    {
        build_record_decl(program, error, scope, option, NULL);
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

    /* The struct and the copy of the name are one block, released with clopts_core_free(). */
    program = (clopts_program_t *)clopts_core_malloc(bytes);
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
    if (!clopts_parser_valid_command_name(name))
    {
        build_record_decl(program, CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
    }
    return program;
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

void clopts_program_set_version(clopts_program_t *program, const char *version)
{
    const clopts_option_t *taken;

    if (!build_begin(program))
    {
        return;
    }
    if (version != NULL)
    {
        taken = clopts_parser_find_conflict(&program->root, "version", '\0');
        if (taken != NULL)
        {
            build_record_decl(program, CLOPTS_DECL_RESERVED_NAME, taken->scope, taken, NULL);
            return;
        }
    }
    build_replace_string(program, &program->version, version);
}

clopts_command_t *clopts_program_root(clopts_program_t *program)
{
    if (program == NULL)
    {
        return NULL;
    }
    return &program->root;
}

/* Rows of clopts_program_add_command(name), in table order. */
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

clopts_command_t *clopts_program_add_command(clopts_program_t *program, const char *name)
{
    clopts_command_t *command;
    char *name_copy;
    clopts_decl_error_t error;

    if (!build_begin(program))
    {
        return NULL;
    }
    error = build_check_command(program, name);

    command = (clopts_command_t *)clopts_core_calloc(1, sizeof(*command));
    if (command == NULL)
    {
        build_record_oom(program);
        return NULL;
    }
    build_init_scope(command, program);
    command->name = NULL;

    if (clopts_core_vec_push(&program->commands, &command) != 0)
    {
        clopts_core_free(command);
        build_record_oom(program);
        return NULL;
    }

    name_copy = clopts_core_strdup(name);
    if ((name != NULL) && (name_copy == NULL))
    {
        build_record_oom(program);
        return NULL;
    }
    command->name = name_copy;
    if (error != CLOPTS_DECL_NONE)
    {
        build_record_decl(program, error, command, NULL, NULL);
    }
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
    if ((handler != NULL) && (command == &command->program->root) && (command->program->commands.len > 0))
    {
        build_record_decl(command->program, CLOPTS_DECL_MIXED_LAYOUT, NULL, NULL, NULL);
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

/* Returns positional index of scope. */
static clopts_positional_t *build_positional_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_positional_t **)clopts_core_vec_at(&scope->positionals, index);
}

/* Rows of clopts_command_add_positional(scope, n), in table order. */
static clopts_decl_error_t build_check_positional(const clopts_command_t *scope, const char *name)
{
    const clopts_program_t *program = scope->program;

    if ((name == NULL) || (name[0] == '\0'))
    {
        return CLOPTS_DECL_INVALID_NAME;
    }
    if ((scope == &program->root) && (program->commands.len > 0))
    {
        return CLOPTS_DECL_MIXED_LAYOUT;
    }
    if ((scope->positionals.len > 0) && build_positional_at(scope, scope->positionals.len - 1)->variadic)
    {
        return CLOPTS_DECL_VARIADIC_POSITION;
    }
    return CLOPTS_DECL_NONE;
}

clopts_positional_t *clopts_command_add_positional(clopts_command_t *scope, const char *name)
{
    clopts_program_t *program;
    clopts_positional_t *positional;
    clopts_decl_error_t error;

    if (scope == NULL)
    {
        return NULL;
    }
    program = scope->program;
    if (!build_begin(program))
    {
        return NULL;
    }
    error = build_check_positional(scope, name);

    positional = (clopts_positional_t *)clopts_core_calloc(1, sizeof(*positional));
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

    if (clopts_core_vec_push(&scope->positionals, &positional) != 0)
    {
        clopts_core_free(positional);
        build_record_oom(program);
        return NULL;
    }

    positional->name = clopts_core_strdup(name);
    if ((name != NULL) && (positional->name == NULL))
    {
        build_record_oom(program);
        return NULL;
    }
    if (error != CLOPTS_DECL_NONE)
    {
        build_record_decl(program, error, scope, NULL, positional);
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

/* Records kind at option (reported with its scope). */
static void build_reject_option(clopts_option_t *option, clopts_decl_error_t kind)
{
    build_record_decl(option->program, kind, option->scope, option, NULL);
}

void clopts_option_set_env(clopts_option_t *option, const char *env)
{
    if (!build_begin_option(option))
    {
        return;
    }
    if ((env != NULL) && build_takes_no_value(option->kind))
    {
        build_reject_option(option, CLOPTS_DECL_ENV_NOT_ALLOWED);
        return;
    }
    if ((env != NULL) && (env[0] == '\0'))
    {
        build_reject_option(option, CLOPTS_DECL_INVALID_NAME);
        return;
    }
    build_replace_string(option->program, &option->env, env);
}

void clopts_option_set_default(clopts_option_t *option, const char *default_value)
{
    if (!build_begin_option(option))
    {
        return;
    }
    if ((default_value != NULL) && build_takes_no_value(option->kind))
    {
        build_reject_option(option, CLOPTS_DECL_DEFAULT_NOT_ALLOWED);
        return;
    }
    if ((default_value != NULL) && option->required)
    {
        build_reject_option(option, CLOPTS_DECL_REQUIRED_WITH_DEFAULT);
        return;
    }
    build_replace_string(option->program, &option->default_value, default_value);
}

void clopts_option_set_required(clopts_option_t *option, bool required)
{
    if (!build_begin_option(option))
    {
        return;
    }
    if (required && build_takes_no_value(option->kind))
    {
        build_reject_option(option, CLOPTS_DECL_REQUIRED_NOT_ALLOWED);
        return;
    }
    if (required && (option->default_value != NULL))
    {
        build_reject_option(option, CLOPTS_DECL_REQUIRED_WITH_DEFAULT);
        return;
    }
    option->required = required;
}

/*
 * True when the lookup range of option's scope has an option whose long name
 * is "no-" followed by option's long name. Returns -1 (and records
 * OUT_OF_MEMORY) when the name cannot be built.
 */
static int build_has_negation_name(clopts_option_t *option)
{
    clopts_program_t *program = option->program;
    size_t len = strlen(option->long_name);
    size_t size;
    char *name;
    int found;

    if (clopts_core_size_add(len, 4, &size) != 0)
    {
        build_record_oom(program);
        return -1;
    }
    name = (char *)clopts_core_malloc(size);
    if (name == NULL)
    {
        build_record_oom(program);
        return -1;
    }
    memcpy(name, "no-", 3);
    memcpy(name + 3, option->long_name, len + 1);
    found = (clopts_parser_find_named(option->scope, name, len + 3) != NULL) ? 1 : 0;
    clopts_core_free(name);
    return found;
}

void clopts_option_set_negatable(clopts_option_t *option, bool negatable)
{
    int conflict = 0;

    if (!build_begin_option(option))
    {
        return;
    }
    if (negatable && (option->kind != CLOPTS_PARSER_FLAG))
    {
        build_reject_option(option, CLOPTS_DECL_NOT_NEGATABLE);
        return;
    }
    if (negatable && (option->long_name != NULL))
    {
        conflict = build_has_negation_name(option);
        if (conflict < 0)
        {
            return;
        }
        if (conflict > 0)
        {
            build_reject_option(option, CLOPTS_DECL_NEGATION_CONFLICT);
            return;
        }
    }
    option->negatable = negatable;
}

void clopts_option_set_delimiter(clopts_option_t *option, char delimiter)
{
    if (!build_begin_option(option))
    {
        return;
    }
    if ((delimiter != '\0') && (option->kind != CLOPTS_PARSER_LIST))
    {
        build_reject_option(option, CLOPTS_DECL_ATTRIBUTE_KIND);
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
    if ((count_max != 0) && (option->kind != CLOPTS_PARSER_COUNT))
    {
        build_reject_option(option, CLOPTS_DECL_ATTRIBUTE_KIND);
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

/* Records kind at positional (reported with its scope). */
static void build_reject_positional(clopts_positional_t *positional, clopts_decl_error_t kind)
{
    build_record_decl(positional->program, kind, positional->scope, NULL, positional);
}

/*
 * True when the order of required positionals breaks if positional becomes
 * required (an optional one before it) or optional (a required one after it).
 */
static bool build_breaks_order(const clopts_positional_t *positional, bool required)
{
    const clopts_command_t *scope = positional->scope;
    size_t i;

    if (required)
    {
        for (i = 0; i < positional->index; i++)
        {
            if (!build_positional_at(scope, i)->required)
            {
                return true;
            }
        }
        return false;
    }
    for (i = positional->index + 1; i < scope->positionals.len; i++)
    {
        if (build_positional_at(scope, i)->required)
        {
            return true;
        }
    }
    return false;
}

void clopts_positional_set_required(clopts_positional_t *positional, bool required)
{
    if (!build_begin_positional(positional))
    {
        return;
    }
    if (build_breaks_order(positional, required))
    {
        build_reject_positional(positional, CLOPTS_DECL_POSITIONAL_ORDER);
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
    if (variadic && ((positional->index + 1) != positional->scope->positionals.len))
    {
        build_reject_positional(positional, CLOPTS_DECL_VARIADIC_POSITION);
        return;
    }
    positional->variadic = variadic;
}
