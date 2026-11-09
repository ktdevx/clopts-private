/*
 * Builder of the program (clopts/program.h): creation and release, the
 * version and the subcommands.
 *
 * Every builder function checks its arguments and the declaration rules that
 * belong to it, in a fixed order, and returns the first rule that fails.
 * Nothing is changed before every check has passed and every allocation has
 * succeeded, so a failing call leaves every handle as it was.
 */

#include "clopts/program.h"
#include "parser/parser_internal.h"

#include <string.h>

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

    clopts_parser_scope_init(&program->root, program);
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

        clopts_parser_scope_free(command);
        clopts_core_free((void *)command->name);
        clopts_core_free(command);
    }
    clopts_core_vec_free(&program->commands);

    clopts_parser_scope_free(&program->root);
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
    error = clopts_parser_check_frozen(program);
    if (error != CLOPTS_DECL_NONE)
    {
        return error;
    }
    /* The lookup range of the root covers every command. */
    if ((version != NULL) && (clopts_parser_find_conflict(&program->root, "version", '\0') != NULL))
    {
        return CLOPTS_DECL_RESERVED_NAME;
    }
    return (clopts_core_replace_string(&program->version, version) == 0) ? CLOPTS_DECL_NONE : CLOPTS_DECL_OUT_OF_MEMORY;
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
static clopts_decl_error_t program_check_command(const clopts_program_t *program, const char *name)
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
    error = clopts_parser_check_frozen(program);
    if (error == CLOPTS_DECL_NONE)
    {
        error = program_check_command(program, name);
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
    clopts_parser_scope_init(command, program);
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
