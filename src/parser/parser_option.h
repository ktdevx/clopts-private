#ifndef CLOPTS_PARSER_OPTION_H
#define CLOPTS_PARSER_OPTION_H

#include "clopts/option.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Internal representation of an option (parser_option.c). An option holds
 * its own attributes only: it does not point to the command it is attached
 * to, it only knows its position there. Attached, it is owned by that
 * command and read-only.
 */

struct clopts_option
{
    bool attached;              /* attached to a command: owned by it and read-only */
    clopts_option_kind_t kind;
    char *long_name;            /* may be NULL */
    char short_name;            /* '\0' for none */
    char *help;                 /* may be NULL; owned copy */
    char *value_name;           /* may be NULL; owned copy */
    char *default_value;        /* may be NULL; owned copy */
    char *env;                  /* may be NULL; owned copy */
    char delimiter;             /* non-'\0' for LIST only */
    unsigned count_max;         /* non-0 for COUNT only; 0 means the default (UINT_MAX) */
    bool required;              /* true for VALUE / LIST only */
    bool negatable;             /* true for FLAG only */
    bool terminal;              /* true for FLAG only */
    size_t index;               /* attached: position in the options of its command */
};

/* Releases option and its strings, attached or not (used when its command is released). */
void clopts_parser_option_release(clopts_option_t *option);

#endif /* CLOPTS_PARSER_OPTION_H */
