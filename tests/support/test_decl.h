#ifndef CLOPTS_TEST_DECL_H
#define CLOPTS_TEST_DECL_H

/*
 * Declaration helpers for the unit tests (tests/support/).
 *
 * They wrap the builder functions of clopts/decl.h so that a test can build a
 * program in the "handle or NULL" style: each helper returns the new handle,
 * or NULL when the call failed, and accepts NULL handles (giving NULL again).
 * The builder functions themselves accept NULL handles too (they give
 * CLOPTS_DECL_INVALID_ARGUMENT), so a sequence of helper and setter calls
 * needs no checks in between. With the fault-injecting allocator
 * (test_alloc.h) a failed allocation thus leaves NULL in the handles that
 * could not be made, and nothing leaks: an option or positional whose attach
 * fails is released by the helper.
 *
 * Setters must be called before the attach: an attached option or
 * positional is read-only.
 *
 * test_decl_last holds the result of the last builder call made by a helper,
 * so a test can check why a helper gave NULL. test_decl_failures counts the
 * failed calls made by the helpers and passed to test_decl_check() since the
 * last test_decl_reset(), so a fixture can tell whether it was built
 * completely (for example under fault injection).
 */

#include "clopts/decl.h"

extern clopts_decl_error_t test_decl_last;
extern unsigned test_decl_failures;

/* Clears test_decl_last and test_decl_failures. */
void test_decl_reset(void);

/* Records error (counting it in test_decl_failures unless it is NONE) and returns it. */
clopts_decl_error_t test_decl_check(clopts_decl_error_t error);

/* clopts_program_new(name); NULL on failure. */
clopts_program_t *test_decl_program(const char *name);

/* clopts_program_add_command(program, name); NULL on failure. */
clopts_command_t *test_decl_command(clopts_program_t *program, const char *name);

/* clopts_option_new(kind, long_name, short_name); NULL on failure. */
clopts_option_t *test_decl_option(clopts_option_kind_t kind, const char *long_name, char short_name);

/* clopts_positional_new(name); NULL on failure. */
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

#endif /* CLOPTS_TEST_DECL_H */
