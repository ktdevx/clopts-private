/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_TEST_DECL_H
#define CLOPTS_TEST_DECL_H

/*
 * Declaration helpers for the unit tests (tests/support/).
 *
 * They wrap the builder functions of clopts/decl/program.h so that a test can build a
 * program in the "handle or NULL" style: each helper returns the new handle,
 * or NULL when the call failed, and accepts NULL handles (giving NULL again).
 * The builder functions themselves do not accept NULL handles (a contract
 * violation), so only the helpers can be chained on a handle that may be NULL.
 * With the fault-injecting allocator
 * (test_alloc.h) a failed allocation thus leaves NULL in the handles that
 * could not be made, and nothing leaks: an option or positional whose attach
 * fails is released by the helper.
 *
 * Setters must be called before the attach: an attached option, positional
 * or command is read-only.
 *
 * A program is created by name and filled; commands are created on their
 * own, filled and then added (clopts_program_add_command()), and an added
 * command is read-only. To keep tests short, test_decl_begin() creates the
 * program and test_decl_command() makes a command and remembers it for that
 * program; once everything is declared, test_decl_finish() adds the
 * remembered commands, in order. When it fails (or the program is NULL), the
 * program and the commands are released and NULL is returned, so a test under
 * fault injection leaks nothing either. A program that is never finished
 * must be released with test_decl_discard().
 *
 * test_decl_last holds the result of the last builder call made by a helper,
 * so a test can check why a helper gave NULL. test_decl_failures counts the
 * failed calls made by the helpers and passed to test_decl_check() since the
 * last test_decl_reset(), so a fixture can tell whether it was built
 * completely (for example under fault injection).
 */

#include "clopts/decl/program.h"

extern clopts_error_t test_decl_last;
extern unsigned test_decl_failures;

/* Clears test_decl_last and test_decl_failures. */
void test_decl_reset(void);

/* Records error (counting it in test_decl_failures unless it is NONE) and returns it. */
clopts_error_t test_decl_check(clopts_error_t error);

/* Maximum number of commands test_decl_command() remembers per program. */
#define TEST_DECL_MAX_COMMANDS 16

/* clopts_program_create(name); NULL on failure. */
clopts_program_t *test_decl_begin(const char *name);

/*
 * clopts_command_create(name), remembered as a subcommand of program; NULL on
 * failure or when program is NULL (nothing is created then).
 */
clopts_command_t *test_decl_command(clopts_program_t *program, const char *name);

/*
 * Remembers command (made elsewhere) as a
 * subcommand of program and returns it. NULL command gives NULL; NULL program
 * releases command and gives NULL.
 */
clopts_command_t *test_decl_adopt(clopts_program_t *program, clopts_command_t *command);

/*
 * The help command of the fixtures, remembered as a subcommand of program like
 * test_decl_command(): "help" ("Show help for a command") with one optional
 * positional COMMAND ("Command to show help for"), as examples/taskbook
 * declares it. NULL on failure or when program is NULL.
 */
clopts_command_t *test_decl_help_command(clopts_program_t *program);

/*
 * clopts_program_add_command(program, c) for each remembered command c of
 * program, in order. Returns program, or NULL on failure after releasing
 * program and its commands. NULL program gives NULL (the commands remembered
 * for it, if any, are released).
 */
clopts_program_t *test_decl_finish(clopts_program_t *program);

/* test_decl_begin(name) then test_decl_finish(): an empty program. */
clopts_program_t *test_decl_program(const char *name);

/* Releases program and its remembered commands without adding them. NULL is fine. */
void test_decl_discard(clopts_program_t *program);

/* clopts_option_create(kind, long_name, short_name); NULL on failure. */
clopts_option_t *test_decl_option(clopts_option_kind_t kind, const char *long_name, char short_name);

/* clopts_positional_create(name); NULL on failure. */
clopts_positional_t *test_decl_positional(const char *name);

/*
 * clopts_command_add_option(command, option). Returns option on success. On
 * failure releases option (when it is not NULL) and returns NULL.
 */
clopts_option_t *test_decl_attach(clopts_command_t *command, clopts_option_t *option);

/* As test_decl_attach(), for a positional. */
clopts_positional_t *test_decl_attach_positional(clopts_command_t *command, clopts_positional_t *positional);

/* test_decl_attach(command, test_decl_option(kind, long_name, short_name)): an option without attributes. */
clopts_option_t *test_decl_add(clopts_command_t *command, clopts_option_kind_t kind, const char *long_name,
                               char short_name);

/* test_decl_attach_positional(command, test_decl_positional(name)): a positional without attributes. */
clopts_positional_t *test_decl_add_positional(clopts_command_t *command, const char *name);

/* As test_decl_attach(), with clopts_program_add_option(program, option): a global option. */
clopts_option_t *test_decl_global_attach(clopts_program_t *program, clopts_option_t *option);

/* As test_decl_attach_positional(), with clopts_program_add_positional(program, positional). */
clopts_positional_t *test_decl_global_attach_positional(clopts_program_t *program, clopts_positional_t *positional);

/* test_decl_global_attach(program, test_decl_option(kind, long_name, short_name)). */
clopts_option_t *test_decl_global_add(clopts_program_t *program, clopts_option_kind_t kind, const char *long_name,
                                      char short_name);

/* test_decl_global_attach_positional(program, test_decl_positional(name)). */
clopts_positional_t *test_decl_global_add_positional(clopts_program_t *program, const char *name);

/*
 * The terminal FLAG -h / --help with the help "Show this help", unattached;
 * NULL on failure. The usual way for a program to declare its --help.
 */
clopts_option_t *test_decl_help_option(void);

/* The terminal FLAG --version with the help "Show version information", unattached; NULL on failure. */
clopts_option_t *test_decl_version_option(void);

#endif /* CLOPTS_TEST_DECL_H */
