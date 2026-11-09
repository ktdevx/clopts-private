#ifndef CLOPTS_PARSER_MODEL_H
#define CLOPTS_PARSER_MODEL_H

#include "clopts/parse.h"
#include "core/core_internal.h"

/*
 * Internal declaration model: the only representation of a declaration, built
 * by the builder functions (parser_build.c). help-and-messages reads it
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

/* Kind of an option: the public kind. */
typedef clopts_option_kind_t clopts_parser_kind_t;

#define CLOPTS_PARSER_FLAG CLOPTS_OPTION_FLAG
#define CLOPTS_PARSER_COUNT CLOPTS_OPTION_COUNT
#define CLOPTS_PARSER_VALUE CLOPTS_OPTION_VALUE
#define CLOPTS_PARSER_LIST CLOPTS_OPTION_LIST

struct clopts_option
{
    bool attached;              /* attached to a command: owned by it and read-only */
    clopts_parser_kind_t kind;
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
};

struct clopts_positional
{
    bool attached;              /* attached to a command: owned by it and read-only */
    char *name;                 /* not empty; owned copy */
    char *help;                 /* may be NULL; owned copy */
    bool required;
    bool variadic;              /* true for the last positional of a command only */
};

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
