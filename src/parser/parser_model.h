#ifndef CLOPTS_PARSER_MODEL_H
#define CLOPTS_PARSER_MODEL_H

#include "clopts/command.h"
#include "clopts/parse.h"
#include "clopts/program.h"
#include "core/core_internal.h"
#include "parser/parser_option.h"
#include "parser/parser_positional.h"

/*
 * Internal declaration model: the only representation of a declaration, built
 * by the builder functions (parser_program.c, parser_command.c,
 * parser_option.c and parser_positional.c). This header holds the program and
 * the commands; options and positionals have their own headers. help-and-messages reads it
 * through const pointers and never writes to it.
 *
 * Invariants, guaranteed by the builder:
 *   - Everything attached to a program satisfies every declaration rule:
 *     a builder call that would break one fails and changes nothing.
 *   - The address of every handle stays the same until clopts_program_free():
 *     children are allocated one by one and the vecs hold pointers only.
 *   - An option or positional does not point to its command. Its index is its
 *     position in the vec of the command it is attached to.
 */

struct clopts_command
{
    clopts_program_t *program;
    const char *name;               /* root: the program name (in the program's block); otherwise an owned copy */
    char *summary;                  /* may be NULL; owned copy */
    char *description;              /* may be NULL; owned copy */
    clopts_core_vec_t options;      /* clopts_option_t * elements, in order of attachment */
    clopts_core_vec_t positionals;  /* clopts_positional_t * elements, in order of attachment */
    clopts_handler_fn handler;      /* may be NULL */
    void *userdata;
};

struct clopts_program
{
    clopts_command_t root;          /* the root scope; &program->root is its handle */
    char *version;                  /* may be NULL; owned copy */
    clopts_core_vec_t commands;     /* clopts_command_t * elements, in order of addition; empty: no subcommands */
    bool frozen;                    /* set by the first parse; never cleared */
};

#endif /* CLOPTS_PARSER_MODEL_H */
