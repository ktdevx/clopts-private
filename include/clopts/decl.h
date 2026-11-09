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
 * Options and positionals are created on their own, configured, and then
 * attached to a command:
 *
 *     clopts_option_t *verbose;
 *     if (clopts_option_new(CLOPTS_OPTION_COUNT, "verbose", 'v', &verbose) != CLOPTS_DECL_NONE) ...
 *     clopts_option_set_help(verbose, "Print more details");
 *     if (clopts_command_add_option(root, verbose) != CLOPTS_DECL_NONE) {
 *         clopts_option_free(verbose);  // a failed attach leaves it with the caller
 *         ...
 *     }
 *
 * An option or positional does not know the command it is attached to.
 *
 * Ownership and lifetime:
 *   - Every string passed to a builder function is copied; the caller may
 *     change or release it right after the call.
 *   - A new option or positional is owned by the caller until it is attached
 *     successfully; the caller releases an unattached one with
 *     clopts_option_free() / clopts_positional_free(). A successful attach
 *     moves ownership to the command; a failed one changes nothing.
 *   - The program owns its root, its commands and everything attached to
 *     them. These handles stay valid, at the same address, until
 *     clopts_program_free().
 *
 * Errors: every builder function returns CLOPTS_DECL_NONE on success and
 * another clopts_decl_error_t right away on failure, leaving every handle
 * unchanged. Nothing is recorded for later: a program built only from
 * successful calls is always valid. Checks that need only the option or
 * positional itself (its kind and attributes) are made by its constructor and
 * setters; checks against the other options, positionals and commands (name
 * collisions, reserved names, order and layout) are made when it is attached.
 *
 * Other rules:
 *   - A NULL handle or out pointer gives CLOPTS_DECL_INVALID_ARGUMENT. On
 *     failure, an out pointer that is not NULL is set to NULL.
 *   - A setter called on an attached option or positional gives
 *     CLOPTS_DECL_ALREADY_ATTACHED: attached handles are read-only.
 *   - The first clopts_parse*() freezes the program. Changing a frozen
 *     program is a contract violation caught by assert (without assert it
 *     gives CLOPTS_DECL_FROZEN).
 *   - Setting a string attribute again replaces the previous copy (last call
 *     wins). NULL, false, '\0' or 0 resets an attribute.
 */

typedef struct clopts_program clopts_program_t;
typedef struct clopts_command clopts_command_t;
typedef struct clopts_option clopts_option_t;
typedef struct clopts_positional clopts_positional_t;
typedef struct clopts_result clopts_result_t;

/* Outcome of a builder function. */
typedef enum clopts_decl_error
{
    CLOPTS_DECL_NONE = 0,
    /* Malformed program / command / option / positional name or empty env name. */
    CLOPTS_DECL_INVALID_NAME = 1,
    /* An option has neither a long nor a short name. */
    CLOPTS_DECL_NO_NAME = 2,
    /* Two commands share a name. */
    CLOPTS_DECL_DUPLICATE_COMMAND = 3,
    /* Option names collide within a scope or between the root and a command. */
    CLOPTS_DECL_DUPLICATE_OPTION = 4,
    /* help / h / version (when a version is set) or the help command. */
    CLOPTS_DECL_RESERVED_NAME = 5,
    /* negatable set on an option that is not a FLAG. */
    CLOPTS_DECL_NOT_NEGATABLE = 6,
    /* A negatable FLAG x and an option named no-x. */
    CLOPTS_DECL_NEGATION_CONFLICT = 7,
    /* A default value set on a FLAG / COUNT. */
    CLOPTS_DECL_DEFAULT_NOT_ALLOWED = 8,
    /* An environment variable set on a FLAG / COUNT. */
    CLOPTS_DECL_ENV_NOT_ALLOWED = 9,
    /* required and a default value both set. */
    CLOPTS_DECL_REQUIRED_WITH_DEFAULT = 10,
    /* required set on a FLAG / COUNT. */
    CLOPTS_DECL_REQUIRED_NOT_ALLOWED = 11,
    /* A required positional attached after an optional one. */
    CLOPTS_DECL_POSITIONAL_ORDER = 12,
    /* A positional attached after a variadic one. */
    CLOPTS_DECL_VARIADIC_POSITION = 13,
    /* A delimiter on a non-LIST option or a count limit on a non-COUNT option. */
    CLOPTS_DECL_ATTRIBUTE_KIND = 14,
    /* Positionals or a handler on the root of a program with subcommands. */
    CLOPTS_DECL_MIXED_LAYOUT = 15,
    /* An option kind outside clopts_option_kind_t. */
    CLOPTS_DECL_INVALID_KIND = 16,
    /* The option or positional is already attached (attach again, or a setter after the attach). */
    CLOPTS_DECL_ALREADY_ATTACHED = 17,
    /* A NULL handle or out pointer. */
    CLOPTS_DECL_INVALID_ARGUMENT = 18,
    /* The program was frozen by a parse (a contract violation, caught by assert). */
    CLOPTS_DECL_FROZEN = 19,
    /* An allocation failed. */
    CLOPTS_DECL_OUT_OF_MEMORY = 20
} clopts_decl_error_t;

/* Kind of an option. The values are part of the ABI. */
typedef enum clopts_option_kind
{
    /* Takes no value. True when given; false via --no-<name> when negatable. */
    CLOPTS_OPTION_FLAG = 0,
    /* Takes no value. Counts occurrences, clamped to the count limit. */
    CLOPTS_OPTION_COUNT = 1,
    /* Takes one value. When repeated, the last one wins. */
    CLOPTS_OPTION_VALUE = 2,
    /* Takes a list value. Repeats are concatenated and split by the delimiter. */
    CLOPTS_OPTION_LIST = 3
} clopts_option_kind_t;

/*
 * Handler of the selected scope, called by clopts_result_run() with the
 * result and the userdata given to clopts_command_set_handler(). Its return
 * value is returned by clopts_result_run() unchanged.
 */
typedef int (*clopts_handler_fn)(const clopts_result_t *result, void *userdata);

/* ---- program ---- */

/*
 * Creates a program named name (copied) into *out. The name must not be
 * NULL or empty, start with '-' or contain white space (INVALID_NAME).
 */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_program_new(const char *name, clopts_program_t **out);

/* Releases program and everything attached to it. NULL does nothing. */
CLOPTS_API void clopts_program_free(clopts_program_t *program);

/*
 * Sets the version string (copied). NULL resets it: --version is not
 * reserved. RESERVED_NAME when an option named version is attached.
 */
CLOPTS_API clopts_decl_error_t clopts_program_set_version(clopts_program_t *program, const char *version);

/*
 * Returns the root scope: global options, and for a program without
 * subcommands its positionals and handler; its description is the program
 * description. NULL when program is NULL. Not a builder call: the same handle
 * is returned whether or not the program is frozen.
 */
CLOPTS_API clopts_command_t *clopts_program_root(clopts_program_t *program);

/*
 * Adds a subcommand named name (copied) into *out. The program owns it.
 * INVALID_NAME, RESERVED_NAME (help), DUPLICATE_COMMAND, or MIXED_LAYOUT when
 * the root has positionals or a handler.
 */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_program_add_command(clopts_program_t *program,
                                                                           const char *name,
                                                                           clopts_command_t **out);

/* ---- command (including the root) ---- */

/* One-line description in the program help. Not used for the root. */
CLOPTS_API clopts_decl_error_t clopts_command_set_summary(clopts_command_t *command, const char *summary);

/* Description in the command help. For the root, the program description. */
CLOPTS_API clopts_decl_error_t clopts_command_set_description(clopts_command_t *command, const char *description);

/*
 * Handler run by clopts_result_run(). userdata is owned by the caller.
 * MIXED_LAYOUT for the root of a program with subcommands.
 */
CLOPTS_API clopts_decl_error_t clopts_command_set_handler(clopts_command_t *command, clopts_handler_fn handler,
                                                          void *userdata);

/*
 * Attaches option to command. On success command owns option, and option
 * stays valid (read-only) until clopts_program_free(). On failure nothing
 * changes and the caller still owns option: ALREADY_ATTACHED, RESERVED_NAME
 * (help, h, or version when a version is set), DUPLICATE_OPTION (within the
 * command, or between the root and a command) or NEGATION_CONFLICT.
 */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_command_add_option(clopts_command_t *command,
                                                                          clopts_option_t *option);

/*
 * Attaches positional to command, after the positionals already attached.
 * Ownership as with clopts_command_add_option(). ALREADY_ATTACHED,
 * MIXED_LAYOUT (the root of a program with subcommands), VARIADIC_POSITION
 * (the last attached one is variadic) or POSITIONAL_ORDER (positional is
 * required and the last attached one is optional).
 */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_command_add_positional(clopts_command_t *command,
                                                                              clopts_positional_t *positional);

/* ---- option ---- strings are copied; NULL resets */

/*
 * Creates an unattached option of kind into *out. long_name excludes the
 * leading "--" (NULL for none); short_name is '\0' for none. At least one of
 * them is required (NO_NAME). INVALID_KIND, INVALID_NAME.
 */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_option_new(clopts_option_kind_t kind, const char *long_name,
                                                                  char short_name, clopts_option_t **out);

/*
 * Releases an unattached option. NULL does nothing. Passing an attached
 * option is a contract violation caught by assert (without assert it does
 * nothing).
 */
CLOPTS_API void clopts_option_free(clopts_option_t *option);

/* One-line description in help. */
CLOPTS_API clopts_decl_error_t clopts_option_set_help(clopts_option_t *option, const char *help);

/* VALUE / LIST: value name shown in help. */
CLOPTS_API clopts_decl_error_t clopts_option_set_value_name(clopts_option_t *option, const char *value_name);

/* VALUE / LIST: environment variable used when argv gives no value. ENV_NOT_ALLOWED, INVALID_NAME (empty). */
CLOPTS_API clopts_decl_error_t clopts_option_set_env(clopts_option_t *option, const char *env);

/* VALUE / LIST: default value. For LIST it is split by the delimiter. DEFAULT_NOT_ALLOWED, REQUIRED_WITH_DEFAULT. */
CLOPTS_API clopts_decl_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value);

/* VALUE / LIST: the option must get a value from argv or the environment. REQUIRED_NOT_ALLOWED, REQUIRED_WITH_DEFAULT. */
CLOPTS_API clopts_decl_error_t clopts_option_set_required(clopts_option_t *option, bool required);

/* FLAG: --no-<long_name> is accepted and sets the flag to false. NOT_NEGATABLE. */
CLOPTS_API clopts_decl_error_t clopts_option_set_negatable(clopts_option_t *option, bool negatable);

/* LIST: item delimiter. '\0' resets it: each occurrence is a single item. ATTRIBUTE_KIND. */
CLOPTS_API clopts_decl_error_t clopts_option_set_delimiter(clopts_option_t *option, char delimiter);

/* COUNT: upper limit of the count. 0 resets it to the default (UINT_MAX). ATTRIBUTE_KIND. */
CLOPTS_API clopts_decl_error_t clopts_option_set_count_max(clopts_option_t *option, unsigned count_max);

/* ---- positional ---- */

/* Creates an unattached positional named name (copied, not empty), used in help and messages. */
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_positional_new(const char *name, clopts_positional_t **out);

/* Releases an unattached positional, as clopts_option_free() does for an option. */
CLOPTS_API void clopts_positional_free(clopts_positional_t *positional);

/* One-line description in help. NULL resets it. */
CLOPTS_API clopts_decl_error_t clopts_positional_set_help(clopts_positional_t *positional, const char *help);

/* Required positionals must precede optional ones (checked when attached). */
CLOPTS_API clopts_decl_error_t clopts_positional_set_required(clopts_positional_t *positional, bool required);

/* Takes all remaining tokens. Must be the last positional (checked when the next one is attached). */
CLOPTS_API clopts_decl_error_t clopts_positional_set_variadic(clopts_positional_t *positional, bool variadic);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_H */
