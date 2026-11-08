/*
 * Name rules and lookups, shared by the builder and the scanner.
 *
 * Names are compared byte by byte, exactly and case-sensitively, with no
 * prefix matching. The lookup range of a scope is the scope itself followed,
 * for the root, by every command in declaration order and, for a command, by
 * the root. The find_* functions used by the scanner search the selected
 * command (when there is one) and then the root.
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <string.h>

/* ---- name rules ---------------------------------------------------------- */

/* True for the white space characters of the C locale. */
static bool decl_is_space(char c)
{
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\v') || (c == '\f') || (c == '\r'));
}

/* True when s contains a white space character. */
static bool decl_has_space(const char *s)
{
    for (; *s != '\0'; s++)
    {
        if (decl_is_space(*s))
        {
            return true;
        }
    }
    return false;
}

bool clopts_parser_valid_command_name(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (!decl_has_space(name)));
}

bool clopts_parser_valid_long_name(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (strchr(name, '=') == NULL)
            && (!decl_has_space(name)));
}

bool clopts_parser_valid_short_name(char c)
{
    /* '\0' means "no short name" and is not a name. */
    return ((c != '\0') && (c != '-') && (c != '=') && (!decl_is_space(c)));
}

/* ---- scopes ---------------------------------------------------------------- */

/* Returns option index of scope. */
static const clopts_option_t *decl_option_at(const clopts_command_t *scope, size_t index)
{
    return *(clopts_option_t *const *)clopts_core_vec_at(&scope->options, index);
}

/* Returns command index of program. */
static const clopts_command_t *decl_command_at(const clopts_program_t *program, size_t index)
{
    return *(clopts_command_t *const *)clopts_core_vec_at(&program->commands, index);
}

/* True when long_name is exactly the name_len bytes at name. */
static bool decl_long_equals(const char *long_name, const char *name, size_t name_len)
{
    return ((long_name != NULL) && (strlen(long_name) == name_len) && (memcmp(long_name, name, name_len) == 0));
}

/* What an option search in one scope looks for. */
typedef struct decl_query
{
    const char *long_name;  /* NULL: do not compare long names */
    size_t long_len;
    char short_name;        /* '\0': do not compare short names */
} decl_query_t;

/* True when option matches query. */
static bool decl_matches(const clopts_option_t *option, const decl_query_t *query)
{
    if ((query->long_name != NULL) && decl_long_equals(option->long_name, query->long_name, query->long_len))
    {
        return true;
    }
    return ((query->short_name != '\0') && (option->short_name == query->short_name));
}

/* Returns the index in scope of the first option matching query, or scope->options.len. */
static size_t decl_index_in(const clopts_command_t *scope, const decl_query_t *query)
{
    size_t i;

    for (i = 0; i < scope->options.len; i++)
    {
        if (decl_matches(decl_option_at(scope, i), query))
        {
            return i;
        }
    }
    return scope->options.len;
}

/* Returns the first option matching query in scope, or NULL. */
static const clopts_option_t *decl_find_in(const clopts_command_t *scope, const decl_query_t *query)
{
    size_t index = decl_index_in(scope, query);

    return (index < scope->options.len) ? decl_option_at(scope, index) : NULL;
}

/* Returns the first option matching query in the lookup range of scope, or NULL. */
static const clopts_option_t *decl_find_in_range(const clopts_command_t *scope, const decl_query_t *query)
{
    const clopts_program_t *program = scope->program;
    const clopts_option_t *found;
    size_t i;

    found = decl_find_in(scope, query);
    if (found != NULL)
    {
        return found;
    }
    if (scope != &program->root)
    {
        return decl_find_in(&program->root, query);
    }
    for (i = 0; i < program->commands.len; i++)
    {
        found = decl_find_in(decl_command_at(program, i), query);
        if (found != NULL)
        {
            return found;
        }
    }
    return NULL;
}

const clopts_option_t *clopts_parser_find_conflict(const clopts_command_t *scope, const char *l, char s)
{
    decl_query_t query;

    assert(scope != NULL);
    query.long_name = l;
    query.long_len = (l != NULL) ? strlen(l) : 0;
    query.short_name = s;
    return decl_find_in_range(scope, &query);
}

const clopts_option_t *clopts_parser_find_named(const clopts_command_t *scope, const char *name, size_t name_len)
{
    decl_query_t query;

    assert((scope != NULL) && (name != NULL));
    query.long_name = name;
    query.long_len = name_len;
    query.short_name = '\0';
    return decl_find_in_range(scope, &query);
}

/* ---- lookups for the scanner --------------------------------------------- */

static clopts_parser_ref_t decl_no_ref(void)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    return ref;
}

/*
 * Searches the selected command (NULL or the root means none) and then the
 * root for the first option matching query.
 */
static clopts_parser_ref_t decl_lookup(const clopts_program_t *program, const clopts_command_t *command,
                                       const decl_query_t *query)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };
    size_t index;

    if ((command != NULL) && (command != &program->root))
    {
        index = decl_index_in(command, query);
        if (index < command->options.len)
        {
            ref.option = decl_option_at(command, index);
            ref.global = false;
            ref.index = index;
            return ref;
        }
    }
    index = decl_index_in(&program->root, query);
    if (index < program->root.options.len)
    {
        ref.option = decl_option_at(&program->root, index);
        ref.global = true;
        ref.index = index;
    }
    return ref;
}

const clopts_command_t *clopts_parser_find_command(const clopts_program_t *program, const char *name)
{
    size_t i;

    assert((program != NULL) && (name != NULL));
    for (i = 0; i < program->commands.len; i++)
    {
        const clopts_command_t *command = decl_command_at(program, i);

        if ((command->name != NULL) && (strcmp(command->name, name) == 0))
        {
            return command;
        }
    }
    return NULL;
}

clopts_parser_ref_t clopts_parser_find_long(const clopts_program_t *program, const clopts_command_t *command,
                                            const char *name, size_t name_len)
{
    decl_query_t query;

    assert((program != NULL) && (name != NULL));
    query.long_name = name;
    query.long_len = name_len;
    query.short_name = '\0';
    return decl_lookup(program, command, &query);
}

clopts_parser_ref_t clopts_parser_find_negation(const clopts_program_t *program, const clopts_command_t *command,
                                                const char *name, size_t name_len)
{
    clopts_parser_ref_t ref;

    assert((program != NULL) && (name != NULL));
    if ((name_len <= 3) || (memcmp(name, "no-", 3) != 0))
    {
        return decl_no_ref();
    }
    ref = clopts_parser_find_long(program, command, name + 3, name_len - 3);
    if ((ref.option == NULL) || (ref.option->kind != CLOPTS_PARSER_FLAG) || (!ref.option->negatable))
    {
        return decl_no_ref();
    }
    ref.negated = true;
    return ref;
}

clopts_parser_ref_t clopts_parser_find_short(const clopts_program_t *program, const clopts_command_t *command, char c)
{
    decl_query_t query;

    assert(program != NULL);
    if (c == '\0')
    {
        return decl_no_ref();
    }
    query.long_name = NULL;
    query.long_len = 0;
    query.short_name = c;
    return decl_lookup(program, command, &query);
}
