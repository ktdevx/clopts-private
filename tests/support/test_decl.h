/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_TEST_DECL_H
#define CLOPTS_TEST_DECL_H

/*
 * Declaration helpers for the unit tests (tests/support/).
 *
 * The builder functions of clopts/decl/program.h already work in the "handle
 * or NULL" style: an add function returns the new handle or NULL, every
 * builder function accepts NULL handles, and the program keeps the first
 * declaration error (clopts_program_get_error()). A test or fixture
 * therefore declares everything without checks and looks at the program once
 * at the end. With the fault-injecting allocator (test_alloc.h) a failed
 * allocation leaves the error OUT_OF_MEMORY in the program (or a NULL
 * program), and nothing leaks once the program is released.
 *
 * The helpers below only shorten what every fixture repeats.
 */

#include "clopts/decl/program.h"

/*
 * Returns program when it has no declaration error. Otherwise releases it
 * and returns NULL, so a fixture built under fault injection ends with NULL
 * and leaks nothing. NULL gives NULL.
 */
clopts_program_t *test_decl_finish(clopts_program_t *program);

/* test_decl_finish(clopts_program_create(name)): an empty program, or NULL. */
clopts_program_t *test_decl_program(const char *name);

/*
 * The help command of the fixtures, added to program: "help" ("Show help for
 * a command") with one optional positional COMMAND ("Command to show help
 * for"), as examples/taskbook declares it. NULL when the add fails.
 */
clopts_command_t *test_decl_help_command(clopts_program_t *program);

#endif /* CLOPTS_TEST_DECL_H */
