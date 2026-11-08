/*
 * DeclCheck: declaration validation and lookup of commands and options.
 *
 * Skeleton: every function returns a fixed value until the declaration check
 * is implemented.
 */

#include "parser/parser_internal.h"

int clopts_parser_decl_check(const clopts_program_t *program, clopts_error_t *error)
{
    (void)program;
    (void)error;
    return 0;
}

const clopts_command_t *clopts_parser_find_command(const clopts_program_t *program,
                                                   const char *name)
{
    (void)program;
    (void)name;
    return NULL;
}

clopts_parser_ref_t clopts_parser_find_long(const clopts_program_t *program,
                                            const clopts_command_t *command,
                                            const char *name, size_t name_len)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    (void)program;
    (void)command;
    (void)name;
    (void)name_len;
    return ref;
}

clopts_parser_ref_t clopts_parser_find_negation(const clopts_program_t *program,
                                                const clopts_command_t *command,
                                                const char *name, size_t name_len)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    (void)program;
    (void)command;
    (void)name;
    (void)name_len;
    return ref;
}

clopts_parser_ref_t clopts_parser_find_short(const clopts_program_t *program,
                                             const clopts_command_t *command, char c)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    (void)program;
    (void)command;
    (void)c;
    return ref;
}

clopts_status_t clopts_decl_validate(const clopts_program_t *program, clopts_error_t *error)
{
    (void)program;
    (void)error;
    return CLOPTS_OK;
}
