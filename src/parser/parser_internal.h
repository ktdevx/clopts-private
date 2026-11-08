#ifndef CLOPTS_PARSER_INTERNAL_H
#define CLOPTS_PARSER_INTERNAL_H

#include "parser/parser_model.h"

/*
 * Internal contracts of the parser module, shared by the allocation gateway
 * (parser_alloc.c), the builder (parser_build.c), the name rules and lookups
 * (parser_decl.c), the scanner (parser_scan.c), the resolver
 * (parser_resolve.c) and the result handle (parser_result.c). None of these
 * symbols is exported from the shared library.
 */

/*
 * Allocation gateway and allocation probe (parser_alloc.c).
 *
 * Every allocation of the parser goes through these functions. When probe is
 * NULL they only call malloc / calloc / free / clopts_core_vec_push /
 * clopts_core_vec_free. When probe is not NULL, each call to malloc, calloc,
 * strdup, strndup or vec_push is one fault point: it increments probe->calls
 * and, when fail_at != 0 and calls >= fail_at, fails without allocating.
 * probe->live counts the live blocks: +1 for each successful object or
 * string allocation and -1 for each free of a non-NULL pointer; a vec counts
 * +1 when its buffer goes from NULL to non-NULL and -1 when vec_free releases
 * a non-NULL buffer.
 */
typedef struct clopts_parser_probe
{
    size_t fail_at;  /* 0: never fail. n: the n-th and every later fault point fail */
    size_t calls;    /* number of fault points passed */
    long live;       /* live blocks (objects, strings and vecs that own a buffer) */
} clopts_parser_probe_t;

/* Returns a block of size bytes, or NULL. */
void *clopts_parser_malloc(clopts_parser_probe_t *probe, size_t size);

/*
 * Returns a zero-filled block of count * size bytes, or NULL. The product is
 * checked with clopts_core_size_mul; on overflow nothing is allocated.
 */
void *clopts_parser_calloc(clopts_parser_probe_t *probe, size_t count, size_t size);

/*
 * Returns a copy of s, or NULL on failure. When s is NULL it returns NULL
 * without counting a fault point.
 */
char *clopts_parser_strdup(clopts_parser_probe_t *probe, const char *s);

/*
 * Returns a NUL-terminated copy of the len bytes at s, or NULL. s must not be
 * NULL and must point to at least len bytes. len + 1 is checked with
 * clopts_core_size_add; on overflow nothing is allocated.
 */
char *clopts_parser_strndup(clopts_parser_probe_t *probe, const char *s, size_t len);

/* Appends elem to vec as clopts_core_vec_push does. Returns 0 or -1. */
int clopts_parser_vec_push(clopts_parser_probe_t *probe, clopts_core_vec_t *vec, const void *elem);

/* Releases the buffer of vec as clopts_core_vec_free does. */
void clopts_parser_vec_free(clopts_parser_probe_t *probe, clopts_core_vec_t *vec);

/* Releases ptr (which may be NULL), allocated by the functions above. */
void clopts_parser_free(clopts_parser_probe_t *probe, void *ptr);

/*
 * Attaches probe to program (parser_build.c). Call it right after
 * clopts_program_new(): allocations before that are not counted. A result
 * inherits the probe of the program it was parsed from.
 */
void clopts_parser_set_probe(clopts_program_t *program, clopts_parser_probe_t *probe);

/*
 * Name rules and lookups (parser_decl.c).
 *
 * Names are compared byte by byte, exactly and case-sensitively; there is no
 * prefix matching. The lookup range of a scope is the scope itself plus, for
 * the root, every command and, for a command, the root. Duplicates between
 * different commands are allowed.
 */

/* Where a looked-up option was found. */
typedef struct clopts_parser_ref
{
    const clopts_option_t *option;  /* NULL when not found */
    bool global;                    /* true: the root, false: the selected command */
    size_t index;                   /* index in that scope */
    bool negated;                   /* found through --no-<name> */
} clopts_parser_ref_t;

/* Program and command name rule: not NULL, not empty, no leading '-', no white space. */
bool clopts_parser_valid_command_name(const char *name);

/* Long name rule: not empty, no leading '-', no '=' and no white space. */
bool clopts_parser_valid_long_name(const char *name);

/* Short name rule: not '-', '=' or white space. */
bool clopts_parser_valid_short_name(char c);

/*
 * Returns the first option in the lookup range of scope whose long name
 * equals l (which may be NULL) or whose short name equals s (when s is not
 * '\0'), or NULL.
 */
const clopts_option_t *clopts_parser_find_conflict(const clopts_command_t *scope, const char *l, char s);

/*
 * Returns the first option in the lookup range of scope whose long name is
 * the name_len bytes at name, or NULL (used for the no-x conflict checks).
 */
const clopts_option_t *clopts_parser_find_named(const clopts_command_t *scope, const char *name, size_t name_len);

/* Returns the command of program named name, or NULL. */
const clopts_command_t *clopts_parser_find_command(const clopts_program_t *program, const char *name);

/*
 * Looks up the long name given by the name_len bytes at name, first in
 * command (which may be NULL) and then in the root.
 */
clopts_parser_ref_t clopts_parser_find_long(const clopts_program_t *program, const clopts_command_t *command,
                                            const char *name, size_t name_len);

/*
 * When name (name_len bytes) starts with "no-", looks up the rest as a
 * negatable FLAG in the same order as clopts_parser_find_long. Used when there
 * is no exact match.
 */
clopts_parser_ref_t clopts_parser_find_negation(const clopts_program_t *program, const clopts_command_t *command,
                                                const char *name, size_t name_len);

/* Looks up the short name c in the same order as clopts_parser_find_long. */
clopts_parser_ref_t clopts_parser_find_short(const clopts_program_t *program, const clopts_command_t *command, char c);

/*
 * Result handle (parser_result.c).
 *
 * A slot records one option and has the same index as the option in its
 * scope. The value of a slot and its source are always updated together, and
 * value == owned whenever owned is not NULL.
 */

/* Recorded state of one option. */
typedef struct clopts_parser_slot
{
    clopts_source_t source;
    bool flag;
    unsigned count;
    const char *value;        /* VALUE: points into argv / the model's default / owned; NULL if none */
    char *owned;              /* copy of an environment value owned by the slot, or NULL */
    clopts_core_vec_t items;  /* LIST: char * elements, each owned by the slot */
} clopts_parser_slot_t;

struct clopts_result
{
    clopts_status_t status;
    const clopts_program_t *program;  /* owner check and run; never read by free */
    const clopts_command_t *command;  /* selected command / help target, or NULL */
    struct
    {
        clopts_usage_error_t usage;
        clopts_decl_error_t decl;
        const clopts_command_t *command;
        const clopts_option_t *option;
        const clopts_positional_t *positional;
        const char *token;            /* an argv element, or NULL */
        int argv_index;               /* -1 when there is none */
        size_t name_len;
        char short_name;
    } error;
    bool parsed;                      /* produced by clopts_parse*() (false for validation) */
    clopts_parser_slot_t *global_slots;   /* one per root option */
    size_t global_count;
    clopts_parser_slot_t *command_slots;  /* one per option of the selected command; NULL without one */
    size_t command_count;
    clopts_core_vec_t positionals;    /* const char * elements pointing into argv */
    clopts_parser_probe_t *probe;     /* inherited from the program */
};

/*
 * Allocates a result for program with one slot per root option. Returns NULL
 * on failure (the caller returns the sentinel instead).
 */
clopts_result_t *clopts_parser_result_new(const clopts_program_t *program);

/*
 * Returns the out-of-memory sentinel: a static const result with status
 * CLOPTS_OUT_OF_MEMORY, no error (NONE / argv_index -1) and no slots. No
 * function writes to it and clopts_result_free() ignores it.
 */
clopts_result_t *clopts_parser_result_oom(void);

/* Selects command and allocates its slots. Returns 0, or -1 leaving result unchanged. */
int clopts_parser_result_select(clopts_result_t *result, const clopts_command_t *command);

/* Returns the slot of the option referred to by ref. */
clopts_parser_slot_t *clopts_parser_result_slot(clopts_result_t *result, const clopts_parser_ref_t *ref);

/* FLAG: records value (false when negated) with source; the last one wins. */
void clopts_parser_slot_set_flag(clopts_parser_slot_t *slot, bool value, clopts_source_t source);

/* COUNT: adds 1, clamped to count_max (0 means UINT_MAX). The source becomes CLOPTS_SOURCE_EXPLICIT. */
void clopts_parser_slot_add_count(clopts_parser_slot_t *slot, unsigned count_max);

/* VALUE: replaces the value (releasing a previous owned copy) and records source; the last one wins. */
void clopts_parser_slot_set_value(clopts_parser_probe_t *probe, clopts_parser_slot_t *slot, const char *value,
                                  clopts_source_t source);

/*
 * LIST: splits value at delimiter and appends a copy of every item, empty
 * items included; with delimiter '\0' value is a single item. Records source.
 * Returns 0, or -1 keeping the items appended so far.
 */
int clopts_parser_slot_append_list(clopts_parser_probe_t *probe, clopts_parser_slot_t *slot, const char *value,
                                   char delimiter, clopts_source_t source);

/* VALUE: moves ownership of owned (an environment value copy) to slot, releasing the previous owned copy. */
void clopts_parser_slot_own(clopts_parser_probe_t *probe, clopts_parser_slot_t *slot, char *owned);

/* Appends token (an argv element) as a positional. Returns 0 or -1. */
int clopts_parser_result_add_positional(clopts_result_t *result, const char *token);

/*
 * Resolver (parser_resolve.c).
 */

/*
 * Default environment lookup: returns the value of getenv(name) as a borrowed
 * string. CLOPTS_ENV_FOUND with *value set, or CLOPTS_ENV_UNSET. ctx is not
 * used.
 */
clopts_env_status_t clopts_parser_getenv_default(void *ctx, const char *name, const char **value);

/*
 * Fills the VALUE / LIST slots that got nothing from argv, from the
 * environment and then from the default value, in declaration order. A found
 * environment value is copied before the callback is called again. Only the
 * root is resolved when include_command is false. Returns 0, or -1 (out of
 * memory) when the lookup fails or a copy cannot be allocated.
 */
int clopts_parser_resolve(clopts_result_t *result, const clopts_program_t *program,
                          clopts_getenv_fn getenv_fn, void *env_ctx, bool include_command);

/*
 * Checks required options and positionals in declaration order (root
 * options, selected command options, positionals); an option counts as given
 * when its source is EXPLICIT or ENV. Returns 0 when nothing is missing;
 * otherwise records the first missing one in result->error and returns -1.
 */
int clopts_parser_check_required(clopts_result_t *result, const clopts_program_t *program, bool include_command);

#endif /* CLOPTS_PARSER_INTERNAL_H */
