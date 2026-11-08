/*
 * Name rules and lookups, shared by the builder and the scanner.
 *
 * Skeleton: every function is a placeholder that returns a fixed value
 * (false, NULL or a reference that found nothing). Task 6.1 implements them.
 */

#include "parser/parser_internal.h"

bool clopts_parser_valid_command_name(const char *name)
{
    (void)name;
    return false;
}

bool clopts_parser_valid_long_name(const char *name)
{
    (void)name;
    return false;
}

bool clopts_parser_valid_short_name(char c)
{
    (void)c;
    return false;
}

const clopts_option_t *clopts_parser_find_conflict(const clopts_command_t *scope, const char *l, char s)
{
    (void)scope;
    (void)l;
    (void)s;
    return NULL;
}

const clopts_option_t *clopts_parser_find_named(const clopts_command_t *scope, const char *name, size_t name_len)
{
    (void)scope;
    (void)name;
    (void)name_len;
    return NULL;
}

const clopts_command_t *clopts_parser_find_command(const clopts_program_t *program, const char *name)
{
    (void)program;
    (void)name;
    return NULL;
}

clopts_parser_ref_t clopts_parser_find_long(const clopts_program_t *program, const clopts_command_t *command,
                                            const char *name, size_t name_len)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    (void)program;
    (void)command;
    (void)name;
    (void)name_len;
    return ref;
}

clopts_parser_ref_t clopts_parser_find_negation(const clopts_program_t *program, const clopts_command_t *command,
                                                const char *name, size_t name_len)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    (void)program;
    (void)command;
    (void)name;
    (void)name_len;
    return ref;
}

clopts_parser_ref_t clopts_parser_find_short(const clopts_program_t *program, const clopts_command_t *command, char c)
{
    clopts_parser_ref_t ref = { NULL, false, 0, false };

    (void)program;
    (void)command;
    (void)c;
    return ref;
}
