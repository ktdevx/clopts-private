/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSER_INTERNAL_H
#define CLOPTS_PARSER_INTERNAL_H

#include "clopts/command.h"
#include "clopts/option.h"
#include "clopts/parse.h"
#include "clopts/positional.h"
#include "clopts/program.h"
#include "core/core_internal.h"

/*
 * Internal declaration model: the only representation of a declaration, built
 * by the builder functions (option.c, positional.c, scope.c, command.c and
 * program.c). The layouts of a program, a command, an option and a
 * positional are private to program.c, command.c, option.c and positional.c.
 * The program and every command embed a scope (below) that holds
 * their options and positionals. Everything outside those files reads them
 * through the read functions only: the public ones, and the scope functions
 * inside the parser (help-and-messages reads the public ones only).
 *
 * Invariants, guaranteed by the builder:
 *   - Everything attached to a program satisfies every declaration rule:
 *     a builder call that would break one fails and changes nothing. This
 *     holds as long as the caller keeps the ownership contract (no setter or
 *     attach after the attach, no handle attached twice), which is not
 *     checked.
 *   - The address of every handle stays the same until it is released:
 *     children are allocated one by one and the arrays hold pointers only.
 *   - Nothing points to its owner: an option or positional does not point to
 *     its program or command, and a command does not point to its program.
 *   - A program does not change once it is first parsed or written: this is
 *     a contract of the caller (clopts/program.h), not checked.
 */

/*
 * Internal contracts of the parser module, shared by the builder
 * (program.c, command.c, option.c, positional.c, scope.c), the name rules
 * and lookups (names.c), the entry of parsing (parse.c), the scanner
 * (scan.c), the resolver (resolve.c) and the result handle (result.c). None
 * of these symbols is exported from the shared library. Every allocation
 * goes through the allocation gateway of core (clopts_core_malloc and the
 * others, core/core_internal.h).
 */

/*
 * Scope (scope.c): the options and positionals of the program or of a
 * command, in order of attachment. The program and every command embed one;
 * everything else reads it through the functions below. An attach checks the
 * rules against the other options and positionals of the same scope, in a
 * fixed order, and changes nothing on failure.
 */
typedef struct clopts_scope
{
    clopts_core_vec_t options;      /* clopts_option_t * elements, in order of attachment */
    clopts_core_vec_t positionals;  /* clopts_positional_t * elements, in order of attachment */
} clopts_scope_t;

/* Makes scope empty. */
void clopts_scope_init(clopts_scope_t *scope);

/* Releases the options and positionals of scope and its arrays, leaving it empty. */
void clopts_scope_release(clopts_scope_t *scope);

/* DUPLICATE_OPTION, NEGATION_CONFLICT (in that order) or OUT_OF_MEMORY; on success scope owns option. */
clopts_error_t clopts_scope_add_option(clopts_scope_t *scope, clopts_option_t *option);

/* VARIADIC_POSITION, POSITIONAL_ORDER (in that order) or OUT_OF_MEMORY; on success scope owns positional. */
clopts_error_t clopts_scope_add_positional(clopts_scope_t *scope, clopts_positional_t *positional);

size_t clopts_scope_option_count(const clopts_scope_t *scope);
/* index must be less than clopts_scope_option_count() (assert). */
const clopts_option_t *clopts_scope_option_at(const clopts_scope_t *scope, size_t index);
size_t clopts_scope_positional_count(const clopts_scope_t *scope);
/* index must be less than clopts_scope_positional_count() (assert). */
const clopts_positional_t *clopts_scope_positional_at(const clopts_scope_t *scope, size_t index);

/* The scope of command (command.c). */
const clopts_scope_t *clopts_command_scope(const clopts_command_t *command);

/* The scope of the program itself: its global options and its positionals (program.c). */
const clopts_scope_t *clopts_program_scope(const clopts_program_t *program);

/*
 * Name rules and lookups (names.c).
 *
 * Names are compared byte by byte, exactly and case-sensitively; there is no
 * prefix matching. The checks of the builder look at one command only. The
 * lookups of the scanner search the selected command and then the program,
 * so a name of the command shadows the same global option.
 */

/* Where a looked-up option was found. */
typedef struct clopts_parser_ref
{
    const clopts_option_t *option;  /* NULL when not found */
    bool global;                    /* true: the program, false: the selected command */
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
 * Returns the first option of scope whose long name equals l (which may be
 * NULL) or whose short name equals s (when s is not '\0'), or NULL.
 */
const clopts_option_t *clopts_parser_find_conflict(const clopts_scope_t *scope, const char *l, char s);

/*
 * Returns the first option of scope whose long name is the name_len bytes
 * at name, or NULL (used for the no-x conflict checks).
 */
const clopts_option_t *clopts_parser_find_named(const clopts_scope_t *scope, const char *name, size_t name_len);

/*
 * Looks up the long name given by the name_len bytes at name in selected
 * (the scope of the selected command; NULL when none) and then in global (the
 * scope of the program). In each scope the exact long name is looked up first
 * and then, when name starts with "no-", the rest as a negatable FLAG
 * (negated is set).
 */
clopts_parser_ref_t clopts_parser_find_long(const clopts_scope_t *global, const clopts_scope_t *selected,
                                            const char *name, size_t name_len);

/* Looks up the short name c in selected (NULL when none) and then in global. */
clopts_parser_ref_t clopts_parser_find_short(const clopts_scope_t *global, const clopts_scope_t *selected, char c);

/*
 * Result handle (result.c).
 *
 * A slot records one option and has the same index as the option in its
 * scope; a slot is found from its option by its address. The value of a slot
 * and its source are always updated together, and value == owned whenever
 * owned is not NULL.
 */

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
 * Only the functions of result.c write to a result; everything else in src/
 * records into it through them.
 */
struct clopts_result
{
    clopts_status_t status;
    const clopts_command_t *command;  /* selected command, or NULL */
    const clopts_option_t *terminal;  /* TERMINATED: the terminal option, else NULL */
    clopts_parser_error_t error;
    clopts_parser_slot_t *global_slots;   /* one per global option */
    size_t global_count;
    clopts_parser_slot_t *command_slots;  /* one per option of the selected command; NULL without one */
    size_t command_count;
    clopts_core_vec_t positionals;    /* const char * elements pointing into argv */
};

/*
 * Allocates a result for program with one slot per global option. Returns NULL
 * on failure (the caller returns the sentinel instead).
 */
clopts_result_t *clopts_parser_result_new(const clopts_program_t *program);

/*
 * Returns the out-of-memory sentinel: a static const result with status
 * CLOPTS_OUT_OF_MEMORY, no error (NONE / argv_index -1) and no slots. No
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
void clopts_parser_result_finish(clopts_result_t *result, clopts_status_t status);

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
 * global options are resolved when include_command is false. Returns 0, or -1 (out of
 * memory) when the lookup fails or a copy cannot be allocated.
 */
int clopts_parser_resolve(clopts_result_t *result, const clopts_program_t *program,
                          clopts_getenv_fn getenv_fn, void *env_ctx, bool include_command);

/*
 * Checks required options and positionals in declaration order (global
 * options, selected command options, positionals); an option counts as given
 * when its source is EXPLICIT or ENV. Returns 0 when nothing is missing;
 * otherwise records the first missing one in result->error and returns -1.
 */
int clopts_parser_check_required(clopts_result_t *result, const clopts_program_t *program, bool include_command);

#endif /* CLOPTS_PARSER_INTERNAL_H */
