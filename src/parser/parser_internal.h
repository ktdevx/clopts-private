/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSER_INTERNAL_H
#define CLOPTS_PARSER_INTERNAL_H

#include "clopts/parse.h"
#include "core/core_internal.h"
#include "decl/decl_internal.h"

/*
 * Internal contracts of the parser module, shared by the entry of parsing
 * and its required checks (parse.c), the scanner and its lookups (scan.c),
 * the resolver (resolve.c) and the result handle (result.c). The parser reads a declaration through the public read
 * functions and the scope functions of the decl module
 * (decl/decl_internal.h), and never writes to it. None of these symbols is
 * exported from the shared library. Every allocation goes through the
 * allocation gateway of core (clopts_core_malloc and the others,
 * core/core_internal.h).
 */

/*
 * Result handle (result.c).
 *
 * A slot records one option and has the same index as the option in its
 * scope; a slot is found from its option by its address. The value of a slot
 * and its source are always updated together, and value == owned whenever
 * owned is not NULL.
 */

/*
 * Where a looked-up option was found: the scanner (scan.c) looks an option
 * up by name and finds its slot through this.
 */
typedef struct clopts_parser_ref
{
    const clopts_option_t *option;  /* NULL when not found */
    bool global;                    /* true: the program, false: the selected command */
    size_t index;                   /* index in that scope */
    bool negated;                   /* found through --no-<name> */
} clopts_parser_ref_t;

/* Recorded state of one option. */
typedef struct clopts_parser_slot
{
    const clopts_option_t *option;  /* the option of the slot */
    clopts_source_t source;
    bool flag;
    unsigned count;
    const char *value;        /* VALUE: points into argv / the model's default / owned; NULL if none */
    char *owned;              /* copy of an environment value owned by the slot, or NULL */
    clopts_core_vec_t items;  /* LIST: char * elements, each owned by the slot */
} clopts_parser_slot_t;

/* The fields of a usage error (the error of a result). */
typedef struct clopts_parser_error
{
    clopts_usage_error_t usage;
    const clopts_command_t *command;
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const char *token;   /* an argv element, or NULL */
    int argv_index;      /* -1 when there is none */
    size_t name_len;
    char short_name;
} clopts_parser_error_t;

/*
 * The layout of a result (struct clopts_result) is private to result.c:
 * everything else reads and records into a result through the functions
 * below.
 */

/*
 * Allocates a result for program with one slot per global option. Returns NULL
 * on failure (the caller returns the sentinel instead).
 */
clopts_result_t *clopts_parser_result_new(const clopts_program_t *program);

/*
 * Returns the out-of-memory sentinel: a static const result with status
 * CLOPTS_PARSE_OUT_OF_MEMORY, no error (NONE / argv_index -1) and no slots. No
 * function writes to it and clopts_result_destroy() ignores it.
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
void clopts_parser_slot_set_value(clopts_parser_slot_t *slot, const char *value,
                                  clopts_source_t source);

/*
 * LIST: splits value at delimiter and appends a copy of every item, empty
 * items included; with delimiter '\0' value is a single item. Records source.
 * Returns 0, or -1 keeping the items appended so far.
 */
int clopts_parser_slot_append_list(clopts_parser_slot_t *slot, const char *value,
                                   char delimiter, clopts_source_t source);

/*
 * VALUE: records owned (an environment value copy) as the value with source,
 * and moves its ownership to slot, releasing the previous owned copy.
 */
void clopts_parser_slot_set_owned_value(clopts_parser_slot_t *slot, char *owned, clopts_source_t source);

/* Appends token (an argv element) as a positional. Returns 0 or -1. */
int clopts_parser_result_add_positional(clopts_result_t *result, const char *token);

/* Records error as the usage error of result. The status is left unchanged. */
void clopts_parser_result_set_error(clopts_result_t *result, const clopts_parser_error_t *error);

/* Records option as the terminal option of result. The status is left unchanged. */
void clopts_parser_result_set_terminal(clopts_result_t *result, const clopts_option_t *option);

/* Writes the final status of result. */
void clopts_parser_result_finish(clopts_result_t *result, clopts_parse_status_t status);

/*
 * Readers of a result. Unlike the public accessors (clopts/parse.h), they do
 * not look at the status, so the parse can read what it has recorded so far.
 * They accept the sentinel.
 */

/*
 * Returns the slots of the global options (global true) or of the selected
 * command (NULL without one) and sets *count to their number. The slot of an
 * option has the index of the option in its scope.
 */
clopts_parser_slot_t *clopts_parser_result_get_slots(clopts_result_t *result, bool global, size_t *count);

/* Returns the slot of option (a global option or one of the selected command), found by its address, or NULL. */
clopts_parser_slot_t *clopts_parser_result_find_slot(clopts_result_t *result, const clopts_option_t *option);

/* Returns the number of positionals recorded so far. */
size_t clopts_parser_result_get_positional_count(const clopts_result_t *result);

/* Returns the usage error of result (usage NONE when none was recorded). */
const clopts_parser_error_t *clopts_parser_result_get_error(const clopts_result_t *result);

/* Returns the terminal option of result, or NULL. */
const clopts_option_t *clopts_parser_result_get_terminal(const clopts_result_t *result);

/*
 * Scanner (scan.c).
 */

/* Outcome of clopts_parser_scan(). */
typedef enum clopts_parser_scan_status
{
    CLOPTS_PARSER_SCAN_DONE,           /* every token was read, or a terminal option ended the scan */
    CLOPTS_PARSER_SCAN_USAGE_ERROR,    /* recorded as the error of result */
    CLOPTS_PARSER_SCAN_OUT_OF_MEMORY
} clopts_parser_scan_status_t;

/*
 * Steps 2 and 3 of parsing (parse.c): scans argv into result, selecting a
 * command when program has subcommands, then runs the recovery scan after a
 * usage error. Sets *terminal to the terminal option that ended the scan, or
 * NULL when the outcome is not DONE. Does not write the status of result.
 */
clopts_parser_scan_status_t clopts_parser_scan(clopts_result_t *result, const clopts_program_t *program,
                                               int argc, char *const *argv, const clopts_option_t **terminal);

/*
 * Resolver (resolve.c).
 */

/* Outcome of clopts_parser_resolve(). */
typedef enum clopts_parser_resolve_status
{
    CLOPTS_PARSER_RESOLVE_DONE,
    CLOPTS_PARSER_RESOLVE_INVALID,        /* an INVALID_ENV_VALUE error is recorded in result */
    CLOPTS_PARSER_RESOLVE_OUT_OF_MEMORY   /* a lookup reported FAILED, or a copy could not be allocated */
} clopts_parser_resolve_status_t;

/*
 * Fills the VALUE / LIST slots that got nothing from argv, from the
 * environment and then from the default value, in declaration order. A found
 * environment value is copied before the callback is called again. getenv_fn
 * must not be NULL: parse.c chooses the source. Only the global options
 * are resolved when include_command is false. A value the lookup reports as
 * INVALID leaves its slot unset; the first one is recorded as the error of
 * result (its command: the selected one when include_command, else NULL),
 * and the resolution goes on. Stops at the first FAILED lookup or
 * allocation failure.
 */
clopts_parser_resolve_status_t clopts_parser_resolve(clopts_result_t *result, const clopts_program_t *program,
                                                     clopts_getenv_fn getenv_fn, void *env_ctx,
                                                     bool include_command);

/*
 * Required checks (parse.c): step 6 of parsing.
 */

/*
 * Checks required options and positionals in declaration order (global
 * options, selected command options, positionals); an option counts as given
 * when its source is EXPLICIT or ENV. Returns 0 when nothing is missing;
 * otherwise records the first missing one as the error of result and
 * returns -1.
 */
int clopts_parser_check_required(clopts_result_t *result, const clopts_program_t *program, bool include_command);

#endif /* CLOPTS_PARSER_INTERNAL_H */
