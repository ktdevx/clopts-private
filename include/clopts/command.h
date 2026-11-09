#ifndef CLOPTS_COMMAND_H
#define CLOPTS_COMMAND_H

#include "clopts/decl_types.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Command (including the root): its texts and handler, and the attach of
 * options and positionals. The attach functions make the checks that depend
 * on the other options, positionals and commands. The rules shared by every
 * builder function are in clopts/decl_types.h.
 */

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

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_COMMAND_H */
