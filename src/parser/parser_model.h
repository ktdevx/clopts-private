#ifndef CLOPTS_PARSER_MODEL_H
#define CLOPTS_PARSER_MODEL_H

#include "clopts/parse.h"
#include "core/core_internal.h"

/*
 * Internal declaration model: the only representation of a declaration, built
 * by the builder functions (parser_build.c). help-and-messages reads it
 * through const pointers and never writes to it.
 *
 * Invariants, guaranteed by the builder for a program whose
 * error.status == CLOPTS_OK:
 *   - Names and attributes satisfy every rule of the immediate check.
 *   - The address of every handle stays the same until clopts_program_free():
 *     children are allocated one by one and the vecs hold pointers only.
 *   - index equals the position of the object in its scope's vec.
 * A program with a recorded error may break them: readers must accept NULL
 * names there.
 */

/* Kind of an option. */
typedef enum clopts_parser_kind
{
    CLOPTS_PARSER_FLAG,
    CLOPTS_PARSER_COUNT,
    CLOPTS_PARSER_VALUE,
    CLOPTS_PARSER_LIST
} clopts_parser_kind_t;

struct clopts_option
{
    clopts_program_t *program;  /* owner; checked by the value accessors */
    clopts_command_t *scope;    /* scope it belongs to (the root included) */
    size_t index;               /* index in scope->options = index of its slot */
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
    clopts_program_t *program;
    clopts_command_t *scope;
    size_t index;               /* index in scope->positionals */
    char *name;                 /* not empty; owned copy */
    char *help;                 /* may be NULL; owned copy */
    bool required;
    bool variadic;              /* true for the last positional of scope only */
};

struct clopts_command
{
    clopts_program_t *program;
    const char *name;               /* root: the program name (in the program's block); otherwise an owned copy */
    char *summary;                  /* may be NULL; owned copy */
    char *description;              /* may be NULL; owned copy */
    clopts_core_vec_t options;      /* clopts_option_t * elements, in order of addition */
    clopts_core_vec_t positionals;  /* clopts_positional_t * elements, in order of addition */
    clopts_handler_fn handler;      /* may be NULL */
    void *userdata;
};

struct clopts_program
{
    clopts_command_t root;          /* the root scope; &program->root is its handle */
    char *version;                  /* may be NULL; owned copy */
    clopts_core_vec_t commands;     /* clopts_command_t * elements, in order of addition; empty: no subcommands */
    bool frozen;                    /* set by the first parse / validation; never cleared */
    struct
    {
        clopts_status_t status;     /* OK / DECL_ERROR / OUT_OF_MEMORY; changes at most once */
        clopts_decl_error_t kind;
        clopts_command_t *command;  /* scope of the offending handle; reported as NULL for the root */
        clopts_option_t *option;
        clopts_positional_t *positional;
    } error;
    struct clopts_parser_probe *probe;  /* tests only; normally NULL */
};

#endif /* CLOPTS_PARSER_MODEL_H */
