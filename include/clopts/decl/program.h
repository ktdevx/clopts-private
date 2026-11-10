/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_PROGRAM_H
#define CLOPTS_DECL_PROGRAM_H

#include <stddef.h>

#include "clopts/base/error.h"
#include "clopts/base/export.h"
#include "clopts/decl/command.h"
#include "clopts/decl/option.h"
#include "clopts/decl/positional.h"
#include "clopts/decl/scope.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Program: creation, release, its own texts, the options, positionals and
 * subcommands added to it, the declaration error, and the read functions. A
 * program is the scope of the program itself: its name is the program name,
 * its options are the global options (found in every subcommand too, unless
 * the subcommand has the same name), and it holds the positionals of a
 * program without subcommands.
 *
 * Once the program is first parsed (clopts/parse/parse.h) or written
 * (clopts/text/write.h), it must not change any more: calling a builder
 * function on it or on anything added to it is a contract violation (the
 * behavior is undefined; it is not detected). It can then be parsed and read
 * from several threads at the same time.
 */

/*
 * Declaration API: the rules every builder function shares. A declaration is
 * built at run time with the builder functions of clopts/decl/option.h,
 * clopts/decl/positional.h, clopts/decl/command.h and this header (this
 * header includes the others) and is referred to through opaque handles
 * only; their layout is private to the library.
 *
 * A program is created by name. Options, positionals and subcommands are
 * created by adding them to the program or to a command, which returns the
 * new handle; it is then configured with its setters. Nothing needs to be
 * checked call by call: the program keeps the first declaration error, and
 * the caller checks it once, after the declaration:
 *
 *     clopts_program_t *program = clopts_program_create("tool");
 *     clopts_option_t *verbose = clopts_program_add_option(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
 *     clopts_option_set_help(verbose, "Print more details");
 *     clopts_program_add_help_option(program);
 *     if (clopts_program_get_error(program) != CLOPTS_ERROR_NONE) {
 *         clopts_program_destroy(program);  // NULL is fine
 *         ...
 *     }
 *
 * Errors:
 *   - A builder function that fails changes nothing, and the program keeps
 *     its error (clopts_program_get_error()). Once the program has an error,
 *     every builder function on it or on anything added to it does nothing,
 *     and the add functions return NULL. The first error is kept: the later
 *     calls cannot report theirs.
 *   - Every builder function accepts a NULL handle and does nothing (the add
 *     functions return NULL), so the handle returned by a failed call can be
 *     passed on. clopts_program_create() returns NULL when it runs out of
 *     memory, and clopts_program_get_error(NULL) reports OUT_OF_MEMORY: the
 *     one check after the declaration covers it too.
 *   - A program with an error is only good for clopts_program_get_error() and
 *     clopts_program_destroy(): parsing or writing it is a contract violation
 *     (caught by assert). Its handles must not be read either.
 *   - Each rule is checked when the call that could break it is made: the
 *     rules of an option or positional alone by its setters; the rules
 *     against the other options and positionals of the same program or
 *     command by the add functions and by the setters that change them
 *     (clopts_option_set_negatable(), clopts_positional_set_required(),
 *     clopts_positional_set_variadic()); the rules between commands (command
 *     names, the layout of the program) by clopts_program_add_command() and
 *     clopts_program_add_positional(). Option names are not checked between
 *     the program and its commands: a name of a command shadows the same
 *     global option (clopts/parse/parse.h).
 *
 * Ownership and lifetime:
 *   - Every string passed to a builder function is copied; the caller may
 *     change or release it right after the call.
 *   - The program owns everything added to it, directly or through a
 *     command. The handles stay valid, at the same address, until
 *     clopts_program_destroy().
 *
 * Other rules:
 *   - A program must not change once it is first parsed or written (see
 *     above). Keeping to this is the caller's responsibility.
 *   - Setting a string attribute again replaces the previous copy (last call
 *     wins). NULL, false, '\0' or 0 resets an attribute.
 *   - The read functions (clopts_option_get_long_name() and the others) do
 *     not accept NULL handles: a NULL program, command, option or positional
 *     is a contract violation (caught by assert).
 */

typedef struct clopts_program clopts_program_t;

/*
 * Creates an empty program named name (copied). Returns NULL when it runs
 * out of memory. A name that is NULL or empty, starts with '-' or contains
 * white space gives a program with the error INVALID_NAME.
 */
CLOPTS_API clopts_program_t *clopts_program_create(const char *name);

/* Releases program and everything added to it. NULL does nothing. */
CLOPTS_API void clopts_program_destroy(clopts_program_t *program);

/*
 * The first declaration error of program, or CLOPTS_ERROR_NONE.
 * OUT_OF_MEMORY for a NULL program (clopts_program_create() ran out of
 * memory).
 */
CLOPTS_API clopts_error_t clopts_program_get_error(const clopts_program_t *program);

/* Description in the program help. */
CLOPTS_API void clopts_program_set_description(clopts_program_t *program, const char *description);

/*
 * Adds a global option of kind to program and returns it, or NULL on
 * failure. long_name excludes the leading "--" (NULL for none); short_name
 * is '\0' for none. INVALID_ARGUMENT (kind), INVALID_NAME (neither name, or
 * a name that breaks the name rules), DUPLICATE (a name already used by a
 * global option) or CONFLICT (the negated form of a flag), in that order.
 */
CLOPTS_API clopts_option_t *clopts_program_add_option(clopts_program_t *program, clopts_option_kind_t kind,
                                                      const char *long_name, char short_name);

/*
 * Adds a positional named name (not empty), used in help and messages, to
 * program after the positionals already added, and returns it, or NULL on
 * failure. INVALID_NAME, CONFLICT (program has subcommands), then ORDER (the
 * last one is variadic).
 */
CLOPTS_API clopts_positional_t *clopts_program_add_positional(clopts_program_t *program, const char *name);

/*
 * Adds a subcommand named name to program after the subcommands already
 * added, and returns it, or NULL on failure. INVALID_NAME (the rule of
 * clopts_program_create()), DUPLICATE (a subcommand of the same name) or
 * CONFLICT (program has positionals), in that order.
 */
CLOPTS_API clopts_command_t *clopts_program_add_command(clopts_program_t *program, const char *name);

/*
 * Adds the terminal FLAG -h / --help with the help "Show this help" as a
 * global option, and returns it, or NULL on failure (as
 * clopts_program_add_option()). When argv gives it, the parse ends with
 * CLOPTS_PARSE_TERMINATED and the caller writes the help
 * (clopts/text/write.h). For other names, add a FLAG and call
 * clopts_option_set_terminal().
 */
CLOPTS_API clopts_option_t *clopts_program_add_help_option(clopts_program_t *program);

/*
 * Adds the terminal FLAG --version with the help "Show version information"
 * as a global option, as clopts_program_add_help_option() does.
 */
CLOPTS_API clopts_option_t *clopts_program_add_version_option(clopts_program_t *program);

/* ---- read functions: a NULL program is a contract violation (assert) ---- */

CLOPTS_API const char *clopts_program_get_name(const clopts_program_t *program);
/* NULL when not set. */
CLOPTS_API const char *clopts_program_get_description(const clopts_program_t *program);
/* The global options and the positionals of the program itself (clopts/decl/scope.h). */
CLOPTS_API const clopts_scope_t *clopts_program_get_scope(const clopts_program_t *program);
/* Number of subcommands, in the order they were added. */
CLOPTS_API size_t clopts_program_get_command_count(const clopts_program_t *program);
/* index must be less than clopts_program_get_command_count() (caught by assert). */
CLOPTS_API const clopts_command_t *clopts_program_get_command_at(const clopts_program_t *program, size_t index);
/* The subcommand named name (exact, case-sensitive), or NULL. name must not be NULL (caught by assert). */
CLOPTS_API const clopts_command_t *clopts_program_find_command(const clopts_program_t *program, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_PROGRAM_H */
