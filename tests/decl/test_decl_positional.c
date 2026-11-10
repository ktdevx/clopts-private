/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl_positional: the checks of a positional alone, made by the add
 * functions (the name) and by the positional setters (positional.c). The
 * order of the positionals of one program or command is checked by the adds
 * and by clopts_positional_set_required() / set_variadic() (decl suite).
 */

#include "clopts/decl/program.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

/* ---- helpers ----------------------------------------------------------- */

/* Checks that program has the declaration error error (CLOPTS_ERROR_NONE: none). */
#define EXPECT_ERROR(program, error) NAOS_CHECK_INT_EQ(clopts_program_get_error(program), (error))

/* A new empty program "tool"; the test stops when it cannot be made. */
static clopts_program_t *new_program(void)
{
    clopts_program_t *program = test_decl_program("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/*
 * Adds a positional named name expecting INVALID_NAME: to a new program, and
 * to a command of another new program. Each add returns NULL, adds nothing
 * and allocates nothing.
 */
static void expect_name_invalid(const char *name)
{
    clopts_program_t *program;
    clopts_command_t *command;
    size_t calls;

    NAOS_NOTE("on the program");
    program = new_program();
    calls = test_alloc.calls;
    NAOS_CHECK_NULL(clopts_program_add_positional(program, name));
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_program_get_scope(program)), 0);
    clopts_program_destroy(program);

    NAOS_NOTE("on a command");
    program = new_program();
    command = clopts_program_add_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(command);
    calls = test_alloc.calls;
    NAOS_CHECK_NULL(clopts_command_add_positional(command, name));
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls);
    EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_positional_count(clopts_command_get_scope(command)), 0);
    clopts_program_destroy(program);
}

/* ---- the name (add functions) ------------------------------------------- */

NAOS_TEST(positional_name_invalid)
{
    char name[] = "FILE";
    clopts_program_t *program;
    clopts_positional_t *positional;

    test_alloc_begin(0);
    expect_name_invalid(NULL);
    expect_name_invalid("");

    /* any other name is fine, and it is copied */
    program = new_program();
    positional = clopts_program_add_positional(program, name);
    NAOS_REQUIRE_NOT_NULL(positional);
    name[0] = 'X';
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(positional), "FILE");
    NAOS_REQUIRE_NOT_NULL(clopts_program_add_positional(program, "-x y"));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- positional setters ---------------------------------------------- */

/* A positional alone is always in order: its setters never fail. */
NAOS_TEST(single_positional_setters_never_fail)
{
    clopts_program_t *program = new_program();
    clopts_positional_t *positional = clopts_program_add_positional(program, "file");

    NAOS_REQUIRE_NOT_NULL(positional);
    clopts_positional_set_variadic(positional, true);
    clopts_positional_set_required(positional, true);
    NAOS_CHECK_TRUE(clopts_positional_is_required(positional));
    NAOS_CHECK_TRUE(clopts_positional_is_variadic(positional));
    clopts_positional_set_variadic(positional, false);
    clopts_positional_set_required(positional, false);
    clopts_positional_set_help(positional, "File.");
    NAOS_CHECK_STR_EQ(clopts_positional_get_help(positional), "File.");
    clopts_positional_set_help(positional, NULL);
    NAOS_CHECK_FALSE(clopts_positional_is_required(positional));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(positional));
    NAOS_CHECK_NULL(clopts_positional_get_help(positional));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

NAOS_SUITE(decl_positional,
    NAOS_ADD_TEST(positional_name_invalid),
    NAOS_ADD_TEST(single_positional_setters_never_fail)
)
