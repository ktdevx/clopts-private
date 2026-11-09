/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

/* Declaration helpers for the unit tests (see test_decl.h). */

#include "test_decl.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

clopts_error_t test_decl_last = CLOPTS_ERROR_NONE;
unsigned test_decl_failures = 0;

void test_decl_reset(void)
{
    test_decl_last = CLOPTS_ERROR_NONE;
    test_decl_failures = 0;
}

clopts_error_t test_decl_check(clopts_error_t error)
{
    test_decl_last = error;
    if (error != CLOPTS_ERROR_NONE)
    {
        test_decl_failures++;
    }
    return error;
}

/* Number of programs whose commands can be remembered at the same time. */
#define TEST_DECL_MAX_PROGRAMS 8

/* The commands remembered for one program. */
typedef struct test_decl_draft
{
    clopts_program_t *program;  /* NULL: free entry */
    clopts_command_t *commands[TEST_DECL_MAX_COMMANDS];
    size_t count;
} test_decl_draft_t;

static test_decl_draft_t test_decl_drafts[TEST_DECL_MAX_PROGRAMS];

/* The draft of program, or a new one when create is true; NULL when there is none. */
static test_decl_draft_t *test_decl_draft(const clopts_program_t *program, bool create)
{
    size_t i;

    for (i = 0; i < TEST_DECL_MAX_PROGRAMS; i++)
    {
        if (test_decl_drafts[i].program == program)
        {
            return &test_decl_drafts[i];
        }
    }
    if (!create)
    {
        return NULL;
    }
    for (i = 0; i < TEST_DECL_MAX_PROGRAMS; i++)
    {
        if (test_decl_drafts[i].program == NULL)
        {
            test_decl_drafts[i].program = (clopts_program_t *)program;
            test_decl_drafts[i].count = 0;
            return &test_decl_drafts[i];
        }
    }
    assert(!"too many test_decl programs at the same time");
    return NULL;
}

/* Forgets draft (without releasing anything). */
static void test_decl_forget(test_decl_draft_t *draft)
{
    draft->program = NULL;
    draft->count = 0;
}

clopts_program_t *test_decl_begin(const char *name)
{
    clopts_program_t *program = NULL;

    test_decl_check(clopts_program_create(name, &program));
    return program;
}

clopts_command_t *test_decl_adopt(clopts_program_t *program, clopts_command_t *command)
{
    test_decl_draft_t *draft;

    if (command == NULL)
    {
        return NULL;
    }
    if (program == NULL)
    {
        clopts_command_destroy(command);
        return NULL;
    }
    draft = test_decl_draft(program, true);
    assert(draft->count < TEST_DECL_MAX_COMMANDS);
    draft->commands[draft->count++] = command;
    return command;
}

clopts_command_t *test_decl_command(clopts_program_t *program, const char *name)
{
    clopts_command_t *command = NULL;

    if (program == NULL)
    {
        return NULL;
    }
    test_decl_check(clopts_command_create(name, &command));
    return test_decl_adopt(program, command);
}

/* Releases the remembered commands of draft from index first on, and forgets draft. */
static void test_decl_release_from(test_decl_draft_t *draft, size_t first)
{
    size_t i;

    for (i = first; i < draft->count; i++)
    {
        clopts_command_destroy(draft->commands[i]);
    }
    test_decl_forget(draft);
}

void test_decl_discard(clopts_program_t *program)
{
    test_decl_draft_t *draft;

    if (program == NULL)
    {
        return;
    }
    draft = test_decl_draft(program, false);
    if (draft != NULL)
    {
        test_decl_release_from(draft, 0);
    }
    clopts_program_destroy(program);
}

clopts_program_t *test_decl_finish(clopts_program_t *program)
{
    test_decl_draft_t *draft;
    size_t i;

    if (program == NULL)
    {
        return NULL;
    }
    draft = test_decl_draft(program, false);
    if (draft == NULL)
    {
        return program;
    }
    for (i = 0; i < draft->count; i++)
    {
        if (test_decl_check(clopts_program_add_command(program, draft->commands[i])) != CLOPTS_ERROR_NONE)
        {
            /* The commands before i belong to program now. */
            test_decl_release_from(draft, i);
            clopts_program_destroy(program);
            return NULL;
        }
    }
    test_decl_forget(draft);
    return program;
}

clopts_program_t *test_decl_program(const char *name)
{
    return test_decl_finish(test_decl_begin(name));
}

clopts_option_t *test_decl_option(clopts_option_kind_t kind, const char *long_name, char short_name)
{
    clopts_option_t *option = NULL;

    test_decl_check(clopts_option_create(kind, long_name, short_name, &option));
    return option;
}

clopts_positional_t *test_decl_positional(const char *name)
{
    clopts_positional_t *positional = NULL;

    test_decl_check(clopts_positional_create(name, &positional));
    return positional;
}

/* Keeps option when error is NONE; otherwise releases it. Returns what is kept. */
static clopts_option_t *test_decl_keep_option(clopts_error_t error, clopts_option_t *option)
{
    if (test_decl_check(error) != CLOPTS_ERROR_NONE)
    {
        clopts_option_destroy(option);
        return NULL;
    }
    return option;
}

/* Keeps positional when error is NONE; otherwise releases it. Returns what is kept. */
static clopts_positional_t *test_decl_keep_positional(clopts_error_t error, clopts_positional_t *positional)
{
    if (test_decl_check(error) != CLOPTS_ERROR_NONE)
    {
        clopts_positional_destroy(positional);
        return NULL;
    }
    return positional;
}

clopts_option_t *test_decl_attach(clopts_command_t *command, clopts_option_t *option)
{
    if ((command == NULL) || (option == NULL))
    {
        /* The call that made the NULL handle has already been recorded. */
        clopts_option_destroy(option);
        return NULL;
    }
    return test_decl_keep_option(clopts_command_add_option(command, option), option);
}

clopts_positional_t *test_decl_attach_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    if ((command == NULL) || (positional == NULL))
    {
        clopts_positional_destroy(positional);
        return NULL;
    }
    return test_decl_keep_positional(clopts_command_add_positional(command, positional), positional);
}

clopts_option_t *test_decl_add(clopts_command_t *command, clopts_option_kind_t kind, const char *long_name,
                               char short_name)
{
    return test_decl_attach(command, test_decl_option(kind, long_name, short_name));
}

clopts_positional_t *test_decl_add_positional(clopts_command_t *command, const char *name)
{
    return test_decl_attach_positional(command, test_decl_positional(name));
}

clopts_option_t *test_decl_global_attach(clopts_program_t *program, clopts_option_t *option)
{
    if ((program == NULL) || (option == NULL))
    {
        clopts_option_destroy(option);
        return NULL;
    }
    return test_decl_keep_option(clopts_program_add_option(program, option), option);
}

clopts_positional_t *test_decl_global_attach_positional(clopts_program_t *program, clopts_positional_t *positional)
{
    if ((program == NULL) || (positional == NULL))
    {
        clopts_positional_destroy(positional);
        return NULL;
    }
    return test_decl_keep_positional(clopts_program_add_positional(program, positional), positional);
}

clopts_option_t *test_decl_global_add(clopts_program_t *program, clopts_option_kind_t kind, const char *long_name,
                                      char short_name)
{
    return test_decl_global_attach(program, test_decl_option(kind, long_name, short_name));
}

clopts_positional_t *test_decl_global_add_positional(clopts_program_t *program, const char *name)
{
    return test_decl_global_attach_positional(program, test_decl_positional(name));
}

/* A terminal FLAG named l / s with help, unattached; NULL on failure. */
static clopts_option_t *test_decl_terminal(const char *l, char s, const char *help)
{
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, l, s);

    if (option == NULL)
    {
        return NULL;
    }
    if ((test_decl_check(clopts_option_set_terminal(option, true)) != CLOPTS_ERROR_NONE)
        || (test_decl_check(clopts_option_set_help(option, help)) != CLOPTS_ERROR_NONE))
    {
        clopts_option_destroy(option);
        return NULL;
    }
    return option;
}

clopts_option_t *test_decl_help_option(void)
{
    return test_decl_terminal("help", 'h', "Show this help");
}

clopts_option_t *test_decl_version_option(void)
{
    return test_decl_terminal("version", '\0', "Show version information");
}
