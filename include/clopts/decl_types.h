#ifndef CLOPTS_DECL_TYPES_H
#define CLOPTS_DECL_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declaration API: the handle types, the result of the builder functions and
 * the rules they share. A program is built at run time with the builder
 * functions of clopts/program.h, clopts/command.h, clopts/option.h and
 * clopts/positional.h (all included by clopts/decl.h) and is referred to
 * through opaque handles only; their layout is private to the library.
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

/*
 * Handler of the selected scope, called by clopts_result_run() with the
 * result and the userdata given to clopts_command_set_handler(). Its return
 * value is returned by clopts_result_run() unchanged.
 */
typedef int (*clopts_handler_fn)(const clopts_result_t *result, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_TYPES_H */
