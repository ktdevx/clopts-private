/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

/* Declaration helpers for the unit tests (see test_decl.h). */

#include "test_decl.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

clopts_decl_error_t test_decl_last = CLOPTS_DECL_NONE;
unsigned test_decl_failures = 0;

void test_decl_reset(void)
{
    test_decl_last = CLOPTS_DECL_NONE;
    test_decl_failures = 0;
}

clopts_decl_error_t test_decl_check(clopts_decl_error_t error)
{
    test_decl_last = error;
    if (error != CLOPTS_DECL_NONE)
    {
        test_decl_failures++;
    }
    return error;
}

/* Number of roots whose commands can be remembered at the same time. */
#define TEST_DECL_MAX_ROOTS 8

/* The commands remembered for one root. */
typedef struct test_decl_draft
{
    clopts_command_t *root;  /* NULL: free entry */
    clopts_command_t *commands[TEST_DECL_MAX_COMMANDS];
    size_t count;
} test_decl_draft_t;

static test_decl_draft_t test_decl_drafts[TEST_DECL_MAX_ROOTS];

/* The draft of root, or a new one when create is true; NULL when there is none. */
static test_decl_draft_t *test_decl_draft(const clopts_command_t *root, bool create)
{
    size_t i;

    for (i = 0; i < TEST_DECL_MAX_ROOTS; i++)
    {
        if (test_decl_drafts[i].root == root)
        {
            return &test_decl_drafts[i];
        }
    }
    if (!create)
    {
        return NULL;
    }
    for (i = 0; i < TEST_DECL_MAX_ROOTS; i++)
    {
        if (test_decl_drafts[i].root == NULL)
        {
            test_decl_drafts[i].root = (clopts_command_t *)root;
            test_decl_drafts[i].count = 0;
            return &test_decl_drafts[i];
        }
    }
    assert(!"too many test_decl roots at the same time");
    return NULL;
}

/* Forgets draft (without releasing anything). */
static void test_decl_forget(test_decl_draft_t *draft)
{
    draft->root = NULL;
    draft->count = 0;
}

clopts_command_t *test_decl_root(const char *name)
{
    clopts_command_t *root = NULL;

    test_decl_check(clopts_command_create(name, &root));
    return root;
}

clopts_command_t *test_decl_adopt(clopts_command_t *root, clopts_command_t *command)
{
    test_decl_draft_t *draft;

    if (command == NULL)
    {
        return NULL;
    }
    if (root == NULL)
    {
        clopts_command_destroy(command);
        return NULL;
    }
    draft = test_decl_draft(root, true);
    assert(draft->count < TEST_DECL_MAX_COMMANDS);
    draft->commands[draft->count++] = command;
    return command;
}

clopts_command_t *test_decl_command(clopts_command_t *root, const char *name)
{
    clopts_command_t *command = NULL;

    if (root == NULL)
    {
        return NULL;
    }
    test_decl_check(clopts_command_create(name, &command));
    return test_decl_adopt(root, command);
}

void test_decl_discard(clopts_command_t *root)
{
    test_decl_draft_t *draft;
    size_t i;

    if (root == NULL)
    {
        return;
    }
    draft = test_decl_draft(root, false);
    if (draft != NULL)
    {
        for (i = 0; i < draft->count; i++)
        {
            clopts_command_destroy(draft->commands[i]);
        }
        test_decl_forget(draft);
    }
    clopts_command_destroy(root);
}

clopts_program_t *test_decl_finish(clopts_command_t *root)
{
    clopts_program_t *program = NULL;
    test_decl_draft_t *draft;

    if (root == NULL)
    {
        return NULL;
    }
    draft = test_decl_draft(root, false);
    test_decl_check(clopts_program_create(root, (draft != NULL) ? draft->commands : NULL,
                                          (draft != NULL) ? draft->count : 0, &program));
    if (program == NULL)
    {
        test_decl_discard(root);
        return NULL;
    }
    if (draft != NULL)
    {
        test_decl_forget(draft);
    }
    return program;
}

clopts_program_t *test_decl_program(const char *name)
{
    return test_decl_finish(test_decl_root(name));
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

clopts_option_t *test_decl_attach(clopts_command_t *command, clopts_option_t *option)
{
    if ((command == NULL) || (option == NULL))
    {
        /* The call that made the NULL handle has already been recorded. */
        clopts_option_destroy(option);
        return NULL;
    }
    test_decl_check(clopts_command_add_option(command, option));
    if (test_decl_last != CLOPTS_DECL_NONE)
    {
        clopts_option_destroy(option);
        return NULL;
    }
    return option;
}

clopts_positional_t *test_decl_attach_positional(clopts_command_t *command, clopts_positional_t *positional)
{
    if ((command == NULL) || (positional == NULL))
    {
        clopts_positional_destroy(positional);
        return NULL;
    }
    test_decl_check(clopts_command_add_positional(command, positional));
    if (test_decl_last != CLOPTS_DECL_NONE)
    {
        clopts_positional_destroy(positional);
        return NULL;
    }
    return positional;
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
