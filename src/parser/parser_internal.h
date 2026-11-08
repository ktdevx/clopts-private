#ifndef CLOPTS_PARSER_INTERNAL_H
#define CLOPTS_PARSER_INTERNAL_H

#include "clopts/decl.h"
#include "clopts/parse.h"
#include "core/core_internal.h"

/*
 * Internal contracts of the parser module, shared by the declaration check
 * (parser_decl.c), the result store (parser_store.c), the resolver
 * (parser_resolve.c) and the scanner (parser_scan.c).
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
 * Declaration check and lookup (parser_decl.c).
 *
 * Lookups compare names byte by byte, exactly and case-sensitively; there is
 * no prefix matching.
 */

/* Where a looked-up option was found. */
typedef struct clopts_parser_ref
{
    const clopts_option_t *option;  /* NULL when not found */
    bool global;                    /* true: program->options, false: command->options */
    size_t index;                   /* index in that array */
    bool negated;                   /* found through --no-<name> */
} clopts_parser_ref_t;

/*
 * Checks the declaration in table order and stops at the first violation.
 * Returns 0 when it is valid; otherwise returns -1 and, when error is not
 * NULL, stores the kind and the offending command / option / positional in
 * *error.
 */
int clopts_parser_decl_check(const clopts_program_t *program, clopts_error_t *error);

/* Returns the command named name, or NULL. */
const clopts_command_t *clopts_parser_find_command(const clopts_program_t *program,
                                                   const char *name);

/*
 * Looks up the long name given by the name_len bytes at name, first in the
 * options of command (which may be NULL) and then in program->options.
 */
clopts_parser_ref_t clopts_parser_find_long(const clopts_program_t *program,
                                            const clopts_command_t *command,
                                            const char *name, size_t name_len);

/*
 * When name (name_len bytes) starts with "no-", looks up the rest as a
 * negatable FLAG in the same order as clopts_parser_find_long. Used when there
 * is no exact match.
 */
clopts_parser_ref_t clopts_parser_find_negation(const clopts_program_t *program,
                                                const clopts_command_t *command,
                                                const char *name, size_t name_len);

/* Looks up the short name c in the same order as clopts_parser_find_long. */
clopts_parser_ref_t clopts_parser_find_short(const clopts_program_t *program,
                                             const clopts_command_t *command, char c);

/*
 * Result store (parser_store.c).
 *
 * The store owns everything reachable from result->impl. A slot has the same
 * index as its option. The value of a slot and its source are always updated
 * together, and value == owned whenever owned is not NULL.
 */

/* Recorded state of one option. */
typedef struct clopts_parser_slot
{
    clopts_source_t source;
    bool flag;
    unsigned count;
    const char *value;        /* VALUE: points into argv / the default / owned; NULL if none */
    char *owned;              /* copy of an environment value owned by the slot, or NULL */
    clopts_core_vec_t items;  /* LIST: char * elements, each owned by the slot */
} clopts_parser_slot_t;

/* Storage behind clopts_result_t.impl. */
typedef struct clopts_parser_store
{
    const clopts_program_t *program;
    const clopts_command_t *command;      /* selected command, or NULL */
    clopts_parser_slot_t *global_slots;   /* program->option_count elements */
    clopts_parser_slot_t *command_slots;  /* command->option_count elements; NULL without command */
    clopts_core_vec_t positionals;        /* const char * elements pointing into argv */
} clopts_parser_store_t;

/*
 * Allocates a store for program and sets it as result->impl, releasing the
 * previous impl first. Returns 0 on success and -1 on failure.
 */
int clopts_parser_store_begin(clopts_result_t *result, const clopts_program_t *program);

/*
 * Selects command and allocates its slots. Returns 0 on success and -1 on
 * failure, leaving the store unchanged.
 */
int clopts_parser_store_select(clopts_result_t *result, const clopts_command_t *command);

/* Returns the slot of the option referred to by ref. */
clopts_parser_slot_t *clopts_parser_store_slot(clopts_parser_store_t *store,
                                               const clopts_parser_ref_t *ref);

/* FLAG: records value (false when negated) with source. */
void clopts_parser_store_set_flag(clopts_parser_slot_t *slot, bool value, clopts_source_t source);

/*
 * COUNT: adds 1, clamped to count_max (0 means CLOPTS_COUNT_MAX_DEFAULT). The
 * source becomes CLOPTS_SOURCE_EXPLICIT.
 */
void clopts_parser_store_add_count(clopts_parser_slot_t *slot, unsigned count_max);

/* VALUE: replaces the value and records source. */
void clopts_parser_store_set_value(clopts_parser_slot_t *slot, const char *value,
                                   clopts_source_t source);

/*
 * LIST: splits value at delimiter and appends a copy of every item, empty
 * items included; with delimiter '\0' value is a single item. Records source.
 * Returns 0 on success and -1 on failure, keeping the items appended so far.
 */
int clopts_parser_store_append_list(clopts_parser_slot_t *slot, const char *value,
                                    char delimiter, clopts_source_t source);

/*
 * VALUE: moves ownership of owned (an environment value copy) to slot,
 * releasing the previous owned copy.
 */
void clopts_parser_store_own(clopts_parser_slot_t *slot, char *owned);

/* Appends token (an argv element) as a positional. Returns 0 or -1. */
int clopts_parser_store_add_positional(clopts_parser_store_t *store, const char *token);

/*
 * Resolver (parser_resolve.c).
 */

/*
 * Default environment lookup: copies the value returned by getenv(name) right
 * away. Returns CLOPTS_ENV_FOUND with the copy in *out, CLOPTS_ENV_UNSET when
 * the variable is not set and CLOPTS_ENV_FAILED when the copy cannot be
 * allocated. ctx is not used.
 */
clopts_env_status_t clopts_parser_getenv_default(void *ctx, const char *name, char **out);

/*
 * Fills VALUE / LIST slots that got nothing from argv, from the environment
 * and then from default_value, in declaration order. Only global slots are
 * resolved when include_command is false. Returns 0 on success and -1 when a
 * lookup or an allocation fails.
 */
int clopts_parser_resolve(clopts_result_t *result, clopts_getenv_fn getenv_fn, void *env_ctx,
                          bool include_command);

/*
 * Checks required options and required positionals in declaration order
 * (global, command, positionals). Returns 0 when nothing is missing;
 * otherwise returns -1 and stores the first missing one in result->error.
 * Only global options are checked when include_command is false.
 */
int clopts_parser_check_required(clopts_result_t *result, bool include_command);

#endif /* CLOPTS_PARSER_INTERNAL_H */
