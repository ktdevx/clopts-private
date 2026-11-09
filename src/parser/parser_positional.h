#ifndef CLOPTS_PARSER_POSITIONAL_H
#define CLOPTS_PARSER_POSITIONAL_H

#include "clopts/positional.h"

#include <stdbool.h>

/*
 * Internal representation of a positional (parser_positional.c). Like an
 * option, it does not point to the command it is attached to.
 */

struct clopts_positional
{
    bool attached;              /* attached to a command: owned by it and read-only */
    char *name;                 /* not empty; owned copy */
    char *help;                 /* may be NULL; owned copy */
    bool required;
    bool variadic;              /* true for the last positional of a command only */
};

/* Releases positional and its strings, attached or not (used by clopts_program_free()). */
void clopts_parser_positional_release(clopts_positional_t *positional);

#endif /* CLOPTS_PARSER_POSITIONAL_H */
