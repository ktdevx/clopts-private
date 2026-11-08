#ifndef CLOPTS_DECL_H
#define CLOPTS_DECL_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declaration types. A program is described by a tree of read-only tables
 * rooted at a clopts_program_t, and every type here can be written with
 * static const initializers only: fields are string pointers, single
 * characters, bools, unsigned integers, sizes, enums, function pointers and
 * void pointers. The library never modifies a declaration table.
 *
 * Designated initializers (.long_name = ...) are recommended so that code does
 * not depend on field order.
 */

/* Kind of an option. */
typedef enum clopts_kind
{
    /* Takes no value. True when given; false via --no-<name> if negatable. */
    CLOPTS_FLAG,
    /* Takes no value. Counts occurrences, clamped to count_max. */
    CLOPTS_COUNT,
    /* Takes a single value. When repeated, the last one wins. */
    CLOPTS_VALUE,
    /* Takes a list value. Repeats are concatenated; split by list_delimiter. */
    CLOPTS_LIST
} clopts_kind_t;

/* Count limit used when an option's count_max is 0. */
#define CLOPTS_COUNT_MAX_DEFAULT UINT_MAX

/* An option. */
typedef struct clopts_option
{
    /* Long name without the leading "--". NULL for a short-name-only option. */
    const char *long_name;
    /* Single-character short name. '\0' for a long-name-only option. */
    char short_name;
    clopts_kind_t kind;
    /* VALUE / LIST only: value name shown in help. May be NULL. */
    const char *value_name;
    /* One-line description (English). May be NULL. */
    const char *help;
    /*
     * VALUE / LIST only: default value, or NULL for none. For LIST it is split
     * by list_delimiter.
     */
    const char *default_value;
    /* VALUE / LIST only: environment variable to fall back to, or NULL. */
    const char *env;
    /* LIST only: item delimiter. '\0' makes each occurrence a single item. */
    char list_delimiter;
    /* COUNT only: upper limit of the count. 0 means CLOPTS_COUNT_MAX_DEFAULT. */
    unsigned count_max;
    /* VALUE / LIST only: the option must get a value from argv or env. */
    bool required;
    /* FLAG only: --no-<long_name> is accepted and sets the flag to false. */
    bool negatable;
} clopts_option_t;

/* A positional argument. */
typedef struct clopts_positional
{
    /* Name used in help and messages (e.g. "FILE"), without angle brackets. */
    const char *name;
    /* One-line description (English). May be NULL. */
    const char *help;
    bool required;
    /* Takes all remaining tokens. Allowed only as the last positional. */
    bool variadic;
} clopts_positional_t;

struct clopts_result;

/*
 * Handler callback. Called by clopts_result_run() with the parse result and
 * the userdata of the selected command (or of the program when it has no
 * commands). Its return value is returned by clopts_result_run() unchanged.
 */
typedef int (*clopts_handler_fn)(const struct clopts_result *result, void *userdata);

/* A subcommand. */
typedef struct clopts_command
{
    /* Command name. Must not be empty. Matched exactly, case-sensitively. */
    const char *name;
    /* One-line description shown in the program help. May be NULL. */
    const char *summary;
    /* Description shown in the command help. NULL means summary is used. */
    const char *description;
    /* Options of this command. May be NULL when option_count == 0. */
    const clopts_option_t *options;
    size_t option_count;
    /* Positionals of this command. May be NULL when positional_count == 0. */
    const clopts_positional_t *positionals;
    size_t positional_count;
    /* Handler run by clopts_result_run(). May be NULL. */
    clopts_handler_fn handler;
    /* Passed to handler. Owned by the caller. */
    void *userdata;
} clopts_command_t;

/*
 * A program. With command_count == 0 the program has no subcommands and its
 * options, positionals and handler are parsed and used directly. With
 * command_count > 0, options are global options and positionals and handler
 * are ignored.
 */
typedef struct clopts_program
{
    /* Program name. Must not be empty. */
    const char *name;
    /* Description shown in the program help. May be NULL. */
    const char *description;
    /*
     * Version string, referenced by pointer so that it may be determined at
     * run time. NULL means --version is not reserved. Owned by the caller and
     * must outlive any result.
     */
    const char *version;
    /* Global options, or the options of a program without subcommands. */
    const clopts_option_t *options;
    size_t option_count;
    /* Without subcommands only; ignored otherwise. */
    const clopts_positional_t *positionals;
    size_t positional_count;
    /* Subcommands. command_count == 0 means no subcommands. */
    const clopts_command_t *commands;
    size_t command_count;
    /* Without subcommands only; ignored otherwise. May be NULL. */
    clopts_handler_fn handler;
    /* Passed to handler. Owned by the caller. */
    void *userdata;
} clopts_program_t;

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_H */
