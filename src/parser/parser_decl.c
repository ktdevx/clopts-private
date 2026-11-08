/*
 * DeclCheck: declaration validation and lookup of commands and options.
 *
 * The check walks the declaration in table order (program name, global
 * options, program positionals without subcommands, then each command's
 * name, options and positionals) and, for each element, applies the rule
 * table of the design in row order, stopping at the first violation.
 * Lookups compare bytes exactly and case-sensitively, with no prefix match.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/* ---- declaration check ------------------------------------------------- */

/* True for the C locale whitespace characters. */
static bool is_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\v') || (c == '\f') || (c == '\r');
}

static bool has_space(const char *s)
{
    for (; *s != '\0'; s++)
    {
        if (is_space(*s))
        {
            return true;
        }
    }
    return false;
}

/* True for the kinds that take a value (VALUE / LIST). */
static bool takes_value(clopts_kind_t kind)
{
    return (kind == CLOPTS_VALUE) || (kind == CLOPTS_LIST);
}

/*
 * Stores a declaration error in *error (when not NULL) and returns -1. The
 * fields that only describe usage errors are reset; usage is left as is.
 */
static int fail(clopts_error_t *error, clopts_decl_error_t kind, const clopts_command_t *command,
                const clopts_option_t *option, const clopts_positional_t *positional)
{
    if (error != NULL)
    {
        error->decl = kind;
        error->command = command;
        error->option = option;
        error->positional = positional;
        error->token = NULL;
        error->argv_index = -1;
        error->name_len = 0;
        error->short_name = '\0';
    }
    return -1;
}

/* A program / command name: not NULL, not empty. */
static bool valid_name(const char *name)
{
    return (name != NULL) && (name[0] != '\0');
}

/* A command name: a valid name that does not start with '-' and has no whitespace. */
static bool valid_command_name(const char *name)
{
    return valid_name(name) && (name[0] != '-') && !has_space(name);
}

/* Row 3 of the rule table: long name, short name and env are well formed. */
static bool valid_option_names(const clopts_option_t *option)
{
    const char *long_name = option->long_name;
    char short_name = option->short_name;

    if ((long_name != NULL)
        && ((long_name[0] == '\0') || (long_name[0] == '-') || (strchr(long_name, '=') != NULL)
            || has_space(long_name)))
    {
        return false;
    }
    if ((short_name == '-') || (short_name == '=') || is_space(short_name))
    {
        return false;
    }
    if ((option->env != NULL) && (option->env[0] == '\0'))
    {
        return false;
    }
    return true;
}

/* True when a and b share a long name or a short name. */
static bool names_collide(const clopts_option_t *a, const clopts_option_t *b)
{
    if ((a->long_name != NULL) && (b->long_name != NULL) && (strcmp(a->long_name, b->long_name) == 0))
    {
        return true;
    }
    return (a->short_name != '\0') && (a->short_name == b->short_name);
}

/* True when flag is negatable and other is named "no-<flag's long name>". */
static bool negation_of(const clopts_option_t *flag, const clopts_option_t *other)
{
    return flag->negatable && (flag->long_name != NULL) && (other->long_name != NULL)
           && (strncmp(other->long_name, "no-", 3) == 0)
           && (strcmp(other->long_name + 3, flag->long_name) == 0);
}

/* True when option collides with one of the count options at others. */
static bool any_duplicate(const clopts_option_t *option, const clopts_option_t *others, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (names_collide(option, &others[i]))
        {
            return true;
        }
    }
    return false;
}

/* True when option and one of the count options at others negate each other. */
static bool any_negation_conflict(const clopts_option_t *option, const clopts_option_t *others,
                                  size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (negation_of(option, &others[i]) || negation_of(&others[i], option))
        {
            return true;
        }
    }
    return false;
}

/*
 * Checks options[index] against the rule table. The options checked before it
 * in the same lookup range are options[0..index) and, for a command option
 * (command != NULL), all of program->options.
 */
static int check_option(const clopts_program_t *program, const clopts_command_t *command,
                        const clopts_option_t *options, size_t index, clopts_error_t *error)
{
    const clopts_option_t *option = &options[index];
    const clopts_option_t *globals = (command != NULL) ? program->options : NULL;
    size_t global_count = (command != NULL) ? program->option_count : 0;
    bool value_kind = takes_value(option->kind);

    if ((option->long_name == NULL) && (option->short_name == '\0'))
    {
        return fail(error, CLOPTS_DECL_NO_NAME, command, option, NULL);
    }
    if (!valid_option_names(option))
    {
        return fail(error, CLOPTS_DECL_INVALID_NAME, command, option, NULL);
    }
    if (any_duplicate(option, globals, global_count) || any_duplicate(option, options, index))
    {
        return fail(error, CLOPTS_DECL_DUPLICATE_OPTION, command, option, NULL);
    }
    if (((option->long_name != NULL)
         && ((strcmp(option->long_name, "help") == 0)
             || ((program->version != NULL) && (strcmp(option->long_name, "version") == 0))))
        || (option->short_name == 'h'))
    {
        return fail(error, CLOPTS_DECL_RESERVED_NAME, command, option, NULL);
    }
    if (option->negatable && (option->kind != CLOPTS_FLAG))
    {
        return fail(error, CLOPTS_DECL_NOT_NEGATABLE, command, option, NULL);
    }
    if (any_negation_conflict(option, globals, global_count)
        || any_negation_conflict(option, options, index))
    {
        return fail(error, CLOPTS_DECL_NEGATION_CONFLICT, command, option, NULL);
    }
    if ((option->default_value != NULL) && !value_kind)
    {
        return fail(error, CLOPTS_DECL_DEFAULT_NOT_ALLOWED, command, option, NULL);
    }
    if ((option->env != NULL) && !value_kind)
    {
        return fail(error, CLOPTS_DECL_ENV_NOT_ALLOWED, command, option, NULL);
    }
    if (option->required && (option->default_value != NULL))
    {
        return fail(error, CLOPTS_DECL_REQUIRED_WITH_DEFAULT, command, option, NULL);
    }
    if (option->required && !value_kind)
    {
        return fail(error, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, command, option, NULL);
    }
    return 0;
}

/* Checks every option of one scope (the globals when command is NULL). */
static int check_options(const clopts_program_t *program, const clopts_command_t *command,
                         const clopts_option_t *options, size_t count, clopts_error_t *error)
{
    size_t i;

    assert((count == 0) || (options != NULL));
    for (i = 0; i < count; i++)
    {
        if (check_option(program, command, options, i, error) != 0)
        {
            return -1;
        }
    }
    return 0;
}

/* Checks the order of required and variadic positionals. */
static int check_positionals(const clopts_command_t *command,
                             const clopts_positional_t *positionals, size_t count,
                             clopts_error_t *error)
{
    bool seen_optional = false;
    size_t i;

    assert((count == 0) || (positionals != NULL));
    for (i = 0; i < count; i++)
    {
        const clopts_positional_t *positional = &positionals[i];

        if (positional->required && seen_optional)
        {
            return fail(error, CLOPTS_DECL_POSITIONAL_ORDER, command, NULL, positional);
        }
        if (positional->variadic && ((i + 1) != count))
        {
            return fail(error, CLOPTS_DECL_VARIADIC_POSITION, command, NULL, positional);
        }
        if (!positional->required)
        {
            seen_optional = true;
        }
    }
    return 0;
}

/* Checks commands[index]: its name, then its options, then its positionals. */
static int check_command(const clopts_program_t *program, size_t index, clopts_error_t *error)
{
    const clopts_command_t *command = &program->commands[index];
    size_t i;

    if (!valid_command_name(command->name))
    {
        return fail(error, CLOPTS_DECL_INVALID_NAME, command, NULL, NULL);
    }
    for (i = 0; i < index; i++)
    {
        if (strcmp(program->commands[i].name, command->name) == 0)
        {
            return fail(error, CLOPTS_DECL_DUPLICATE_COMMAND, command, NULL, NULL);
        }
    }
    if (strcmp(command->name, "help") == 0)
    {
        return fail(error, CLOPTS_DECL_RESERVED_NAME, command, NULL, NULL);
    }
    if (check_options(program, command, command->options, command->option_count, error) != 0)
    {
        return -1;
    }
    return check_positionals(command, command->positionals, command->positional_count, error);
}

int clopts_parser_decl_check(const clopts_program_t *program, clopts_error_t *error)
{
    size_t i;

    assert(program != NULL);
    if (!valid_name(program->name))
    {
        return fail(error, CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
    }
    if (check_options(program, NULL, program->options, program->option_count, error) != 0)
    {
        return -1;
    }
    if (program->command_count == 0)
    {
        /* Without subcommands the program's own positionals are parsed (1.4). */
        return check_positionals(NULL, program->positionals, program->positional_count, error);
    }
    assert(program->commands != NULL);
    for (i = 0; i < program->command_count; i++)
    {
        if (check_command(program, i, error) != 0)
        {
            return -1;
        }
    }
    return 0;
}

clopts_status_t clopts_decl_validate(const clopts_program_t *program, clopts_error_t *error)
{
    assert(program != NULL);
    return (clopts_parser_decl_check(program, error) == 0) ? CLOPTS_OK : CLOPTS_DECL_ERROR;
}

/* ---- lookup ------------------------------------------------------------ */

static clopts_parser_ref_t no_ref(void)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    return ref;
}

static clopts_parser_ref_t make_ref(const clopts_option_t *options, bool global, size_t index)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    ref.option = &options[index];
    ref.global = global;
    ref.index = index;
    return ref;
}

/* True when long_name is exactly the name_len bytes at name. */
static bool long_equals(const char *long_name, const char *name, size_t name_len)
{
    return (long_name != NULL) && (strlen(long_name) == name_len)
           && (memcmp(long_name, name, name_len) == 0);
}

/* Returns the index of the option named name (name_len bytes), or count. */
static size_t index_of_long(const clopts_option_t *options, size_t count, const char *name,
                            size_t name_len)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (long_equals(options[i].long_name, name, name_len))
        {
            return i;
        }
    }
    return count;
}

/* Returns the index of the option with short name c, or count. */
static size_t index_of_short(const clopts_option_t *options, size_t count, char c)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (options[i].short_name == c)
        {
            return i;
        }
    }
    return count;
}

const clopts_command_t *clopts_parser_find_command(const clopts_program_t *program,
                                                   const char *name)
{
    size_t i;

    assert((program != NULL) && (name != NULL));
    for (i = 0; i < program->command_count; i++)
    {
        if (strcmp(program->commands[i].name, name) == 0)
        {
            return &program->commands[i];
        }
    }
    return NULL;
}

clopts_parser_ref_t clopts_parser_find_long(const clopts_program_t *program,
                                            const clopts_command_t *command,
                                            const char *name, size_t name_len)
{
    size_t index;

    assert((program != NULL) && (name != NULL));
    if (command != NULL)
    {
        index = index_of_long(command->options, command->option_count, name, name_len);
        if (index < command->option_count)
        {
            return make_ref(command->options, false, index);
        }
    }
    index = index_of_long(program->options, program->option_count, name, name_len);
    if (index < program->option_count)
    {
        return make_ref(program->options, true, index);
    }
    return no_ref();
}

clopts_parser_ref_t clopts_parser_find_negation(const clopts_program_t *program,
                                                const clopts_command_t *command,
                                                const char *name, size_t name_len)
{
    clopts_parser_ref_t ref;

    assert((program != NULL) && (name != NULL));
    if ((name_len <= 3) || (memcmp(name, "no-", 3) != 0))
    {
        return no_ref();
    }
    ref = clopts_parser_find_long(program, command, name + 3, name_len - 3);
    if ((ref.option == NULL) || (ref.option->kind != CLOPTS_FLAG) || !ref.option->negatable)
    {
        return no_ref();
    }
    ref.negated = true;
    return ref;
}

clopts_parser_ref_t clopts_parser_find_short(const clopts_program_t *program,
                                             const clopts_command_t *command, char c)
{
    size_t index;

    assert(program != NULL);
    if (c == '\0')
    {
        return no_ref();
    }
    if (command != NULL)
    {
        index = index_of_short(command->options, command->option_count, c);
        if (index < command->option_count)
        {
            return make_ref(command->options, false, index);
        }
    }
    index = index_of_short(program->options, program->option_count, c);
    if (index < program->option_count)
    {
        return make_ref(program->options, true, index);
    }
    return no_ref();
}
