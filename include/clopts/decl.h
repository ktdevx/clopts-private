#ifndef CLOPTS_DECL_H
#define CLOPTS_DECL_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declaration API. A program is built at run time with the builder functions
 * below and is referred to through opaque handles only; their layout is
 * private to the library.
 *
 * Ownership and lifetime:
 *   - Every string passed to a builder function is copied; the caller may
 *     change or release it right after the call.
 *   - The program owns every handle derived from it (root, commands, options,
 *     positionals). A handle stays valid, at the same address, until
 *     clopts_program_free().
 *
 * Builder rules:
 *   - A NULL handle makes a builder function do nothing (functions returning
 *     a handle return NULL). Builder code therefore needs no NULL checks
 *     between calls.
 *   - Every call is checked right away. The first violation (a declaration
 *     error) or allocation failure is recorded in the program; after that
 *     builder functions do nothing and return NULL. clopts_parse*() and
 *     clopts_program_validate() report what was recorded.
 *   - The first clopts_parse*() or clopts_program_validate() freezes the
 *     program. Calling a builder function on a frozen program is a contract
 *     violation caught by assert (without assert it does nothing).
 *   - Setting a string attribute again replaces the previous copy (last call
 *     wins). NULL, false, '\0' or 0 resets an attribute.
 */

typedef struct clopts_program clopts_program_t;
typedef struct clopts_command clopts_command_t;
typedef struct clopts_option clopts_option_t;
typedef struct clopts_positional clopts_positional_t;
typedef struct clopts_result clopts_result_t;

/*
 * Handler of the selected scope, called by clopts_result_run() with the
 * result and the userdata given to clopts_command_set_handler(). Its return
 * value is returned by clopts_result_run() unchanged.
 */
typedef int (*clopts_handler_fn)(const clopts_result_t *result, void *userdata);

/* ---- program ---- */

/*
 * Creates a program named name (copied). Returns NULL when allocation fails.
 * An invalid name (NULL, empty, starting with '-' or containing white space)
 * still returns a program, with CLOPTS_DECL_INVALID_NAME recorded.
 */
CLOPTS_API clopts_program_t *clopts_program_new(const char *name);

/* Releases program and every handle derived from it. NULL does nothing. */
CLOPTS_API void clopts_program_free(clopts_program_t *program);

/* Sets the version string (copied). NULL resets it: --version is not reserved. */
CLOPTS_API void clopts_program_set_version(clopts_program_t *program, const char *version);

/*
 * Returns the root scope: global options, and for a program without
 * subcommands its positionals and handler; its description is the program
 * description. NULL when program is NULL. Not a builder call: the same handle
 * is returned whether or not an error was recorded or the program is frozen.
 */
CLOPTS_API clopts_command_t *clopts_program_root(clopts_program_t *program);

/* Adds a subcommand named name (copied). */
CLOPTS_API clopts_command_t *clopts_program_add_command(clopts_program_t *program, const char *name);

/* ---- command (including the root) ---- */

/* One-line description in the program help. Not used for the root. */
CLOPTS_API void clopts_command_set_summary(clopts_command_t *command, const char *summary);

/* Description in the command help. For the root, the program description. */
CLOPTS_API void clopts_command_set_description(clopts_command_t *command, const char *description);

/* Handler run by clopts_result_run(). userdata is owned by the caller. */
CLOPTS_API void clopts_command_set_handler(clopts_command_t *command, clopts_handler_fn handler, void *userdata);

/*
 * Option adders. long_name excludes the leading "--" (NULL for none);
 * short_name is '\0' for none. At least one of them is required.
 */

/* Takes no value. True when given; false via --no-<name> when negatable. */
CLOPTS_API clopts_option_t *clopts_command_add_flag(clopts_command_t *scope, const char *long_name, char short_name);

/* Takes no value. Counts occurrences, clamped to the count limit. */
CLOPTS_API clopts_option_t *clopts_command_add_count(clopts_command_t *scope, const char *long_name, char short_name);

/* Takes one value. When repeated, the last one wins. */
CLOPTS_API clopts_option_t *clopts_command_add_value(clopts_command_t *scope, const char *long_name, char short_name);

/* Takes a list value. Repeats are concatenated and split by the delimiter. */
CLOPTS_API clopts_option_t *clopts_command_add_list(clopts_command_t *scope, const char *long_name, char short_name);

/* Adds a positional named name (copied), used in help and messages. */
CLOPTS_API clopts_positional_t *clopts_command_add_positional(clopts_command_t *scope, const char *name);

/* ---- option ---- strings are copied; NULL resets */

/* One-line description in help. */
CLOPTS_API void clopts_option_set_help(clopts_option_t *option, const char *help);

/* VALUE / LIST: value name shown in help. */
CLOPTS_API void clopts_option_set_value_name(clopts_option_t *option, const char *value_name);

/* VALUE / LIST: environment variable used when argv gives no value. */
CLOPTS_API void clopts_option_set_env(clopts_option_t *option, const char *env);

/* VALUE / LIST: default value. For LIST it is split by the delimiter. */
CLOPTS_API void clopts_option_set_default(clopts_option_t *option, const char *default_value);

/* VALUE / LIST: the option must get a value from argv or the environment. */
CLOPTS_API void clopts_option_set_required(clopts_option_t *option, bool required);

/* FLAG: --no-<long_name> is accepted and sets the flag to false. */
CLOPTS_API void clopts_option_set_negatable(clopts_option_t *option, bool negatable);

/* LIST: item delimiter. '\0' resets it: each occurrence is a single item. */
CLOPTS_API void clopts_option_set_delimiter(clopts_option_t *option, char delimiter);

/* COUNT: upper limit of the count. 0 resets it to the default (UINT_MAX). */
CLOPTS_API void clopts_option_set_count_max(clopts_option_t *option, unsigned count_max);

/* ---- positional ---- */

/* One-line description in help. NULL resets it. */
CLOPTS_API void clopts_positional_set_help(clopts_positional_t *positional, const char *help);

/* Required positionals must precede optional ones. */
CLOPTS_API void clopts_positional_set_required(clopts_positional_t *positional, bool required);

/* Takes all remaining tokens. Allowed only for the last positional. */
CLOPTS_API void clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_H */
