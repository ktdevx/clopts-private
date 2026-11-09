/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl_option: the checks of an option alone, made by
 * clopts_option_create() and the option setters (option.c). None of these
 * tests creates a program or a command: an option does not depend on them.
 * The checks against the other options are in the decl suite.
 *
 * Every call returns its clopts_decl_error_t right away and a failing call
 * changes nothing (the out pointer NULL, the attribute unchanged).
 */

#include "parser/parser_internal.h"
#include "test_decl.h"

#include <naos/naos.h>

/* ---- helpers ----------------------------------------------------------- */

/* Checks that call returns CLOPTS_DECL_NONE. */
#define EXPECT_OK(call) NAOS_CHECK_INT_EQ((call), CLOPTS_DECL_NONE)

/* An object whose address an out pointer holds before a call; never read or written. */
static clopts_option_t sentinel_option;

/* A new unattached option; the test stops when it cannot be made. */
static clopts_option_t *make(clopts_option_kind_t kind, const char *long_name, char short_name)
{
    clopts_option_t *option = test_decl_option(kind, long_name, short_name);

    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/* Calls clopts_option_create() expecting error and checks that *out is set to NULL. */
static void expect_option_new_error(clopts_option_kind_t kind, const char *long_name, char short_name,
                                    clopts_decl_error_t error)
{
    clopts_option_t *out = &sentinel_option;

    NAOS_CHECK_INT_EQ(clopts_option_create(kind, long_name, short_name, &out), error);
    NAOS_CHECK_NULL(out);
    if ((out != NULL) && (out != &sentinel_option))
    {
        clopts_option_destroy(out);
    }
}

/* ---- clopts_option_create ----------------------------------------------- */

NAOS_TEST(option_invalid_kind)
{
    expect_option_new_error((clopts_option_kind_t)4, "flag", 'f', CLOPTS_DECL_INVALID_KIND);
    expect_option_new_error((clopts_option_kind_t)-1, "flag", 'f', CLOPTS_DECL_INVALID_KIND);
    expect_option_new_error((clopts_option_kind_t)100, NULL, 'f', CLOPTS_DECL_INVALID_KIND);

    /* the kind is checked first */
    expect_option_new_error((clopts_option_kind_t)4, NULL, '\0', CLOPTS_DECL_INVALID_KIND);
    expect_option_new_error((clopts_option_kind_t)4, "-x", '=', CLOPTS_DECL_INVALID_KIND);
}

NAOS_TEST(option_without_name)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE,
                                                  CLOPTS_OPTION_LIST };
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        expect_option_new_error(kinds[i], NULL, '\0', CLOPTS_DECL_NO_NAME);
    }
}

NAOS_TEST(option_long_name_invalid)
{
    static const char *const names[] = { "", "-o", "--o", "out=put", "out put", "out\vput" };
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        NAOS_NOTE("long=\"%s\"", names[i]);
        expect_option_new_error(CLOPTS_OPTION_VALUE, names[i], 'o', CLOPTS_DECL_INVALID_NAME);
        expect_option_new_error(CLOPTS_OPTION_FLAG, names[i], '\0', CLOPTS_DECL_INVALID_NAME);
    }
}

NAOS_TEST(option_short_name_invalid)
{
    static const char shorts[] = { '-', '=', ' ', '\t' };
    size_t i;

    for (i = 0; i < sizeof(shorts); i++)
    {
        NAOS_NOTE("short=%d", (int)shorts[i]);
        expect_option_new_error(CLOPTS_OPTION_FLAG, "flag", shorts[i], CLOPTS_DECL_INVALID_NAME);
        expect_option_new_error(CLOPTS_OPTION_COUNT, NULL, shorts[i], CLOPTS_DECL_INVALID_NAME);
    }
}

NAOS_TEST(option_new_valid_names)
{
    clopts_option_t *option = NULL;

    EXPECT_OK(clopts_option_create(CLOPTS_OPTION_LIST, "no-", '4', &option));
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_INT_EQ(option->kind, CLOPTS_OPTION_LIST);
    NAOS_CHECK_STR_EQ(option->long_name, "no-");
    NAOS_CHECK_INT_EQ(option->short_name, '4');
    clopts_option_destroy(option);

    /* there are no reserved names: help, h and version are ordinary names */
    option = NULL;
    EXPECT_OK(clopts_option_create(CLOPTS_OPTION_FLAG, "help", 'h', &option));
    NAOS_REQUIRE_NOT_NULL(option);
    clopts_option_destroy(option);
    option = NULL;
    EXPECT_OK(clopts_option_create(CLOPTS_OPTION_VALUE, "version", 'V', &option));
    NAOS_REQUIRE_NOT_NULL(option);
    clopts_option_destroy(option);
}

/* ---- clopts_option_set_env ------------------------------------------- */

NAOS_TEST(env_on_flag_or_count)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, "TOOL_FORCE"), CLOPTS_DECL_ENV_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->env);
    clopts_option_destroy(option);

    /* the kind is checked before the empty name */
    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, ""), CLOPTS_DECL_ENV_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->env);
    clopts_option_destroy(option);
}

NAOS_TEST(env_empty_name)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_LIST, "include", 'I');
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, ""), CLOPTS_DECL_INVALID_NAME);
    NAOS_CHECK_NULL(option->env);

    /* a failing call keeps the previous value */
    EXPECT_OK(clopts_option_set_env(option, "TOOL_INCLUDE"));
    NAOS_CHECK_INT_EQ(clopts_option_set_env(option, ""), CLOPTS_DECL_INVALID_NAME);
    NAOS_CHECK_STR_EQ(option->env, "TOOL_INCLUDE");
    clopts_option_destroy(option);
}

NAOS_TEST(env_unset_is_not_checked)
{
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = make(CLOPTS_OPTION_FLAG, "force", 'f');
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_env(flag, NULL));
    EXPECT_OK(clopts_option_set_env(value, "TOOL_OUTPUT"));
    NAOS_CHECK_STR_EQ(value->env, "TOOL_OUTPUT");
    EXPECT_OK(clopts_option_set_env(value, NULL));
    NAOS_CHECK_NULL(value->env);
    clopts_option_destroy(flag);
    clopts_option_destroy(value);
}

/* ---- clopts_option_set_default --------------------------------------- */

NAOS_TEST(default_on_flag_or_count)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_INT_EQ(clopts_option_set_default(option, "1"), CLOPTS_DECL_DEFAULT_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->default_value);
    clopts_option_destroy(option);

    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    NAOS_CHECK_INT_EQ(clopts_option_set_default(option, "2"), CLOPTS_DECL_DEFAULT_NOT_ALLOWED);
    NAOS_CHECK_NULL(option->default_value);
    clopts_option_destroy(option);
}

NAOS_TEST(default_on_required)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_required(option, true));
    NAOS_CHECK_INT_EQ(clopts_option_set_default(option, "a.out"), CLOPTS_DECL_REQUIRED_WITH_DEFAULT);
    NAOS_CHECK_NULL(option->default_value);
    NAOS_CHECK_TRUE(option->required);

    /* allowed once required is reset */
    EXPECT_OK(clopts_option_set_required(option, false));
    EXPECT_OK(clopts_option_set_default(option, "a.out"));
    NAOS_CHECK_STR_EQ(option->default_value, "a.out");
    clopts_option_destroy(option);
}

NAOS_TEST(default_unset_is_not_checked)
{
    clopts_option_t *flag;
    clopts_option_t *list;

    flag = make(CLOPTS_OPTION_FLAG, "force", 'f');
    list = make(CLOPTS_OPTION_LIST, "include", 'I');
    EXPECT_OK(clopts_option_set_default(flag, NULL));
    EXPECT_OK(clopts_option_set_required(list, true));
    EXPECT_OK(clopts_option_set_default(list, NULL));
    NAOS_CHECK_TRUE(list->required);
    clopts_option_destroy(flag);
    clopts_option_destroy(list);
}

/* ---- clopts_option_set_required -------------------------------------- */

NAOS_TEST(required_on_flag_or_count)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_CHECK_INT_EQ(clopts_option_set_required(option, true), CLOPTS_DECL_REQUIRED_NOT_ALLOWED);
    NAOS_CHECK_FALSE(option->required);
    clopts_option_destroy(option);

    option = make(CLOPTS_OPTION_COUNT, "verbose", 'v');
    NAOS_CHECK_INT_EQ(clopts_option_set_required(option, true), CLOPTS_DECL_REQUIRED_NOT_ALLOWED);
    NAOS_CHECK_FALSE(option->required);
    clopts_option_destroy(option);
}

NAOS_TEST(required_on_option_with_default)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_LIST, "include", 'I');
    EXPECT_OK(clopts_option_set_default(option, "a,b"));
    NAOS_CHECK_INT_EQ(clopts_option_set_required(option, true), CLOPTS_DECL_REQUIRED_WITH_DEFAULT);
    NAOS_CHECK_FALSE(option->required);
    NAOS_CHECK_STR_EQ(option->default_value, "a,b");

    /* allowed once the default is reset */
    EXPECT_OK(clopts_option_set_default(option, NULL));
    EXPECT_OK(clopts_option_set_required(option, true));
    NAOS_CHECK_TRUE(option->required);
    clopts_option_destroy(option);
}

NAOS_TEST(required_unset_is_not_checked)
{
    clopts_option_t *flag;
    clopts_option_t *value;

    flag = make(CLOPTS_OPTION_FLAG, "force", 'f');
    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_required(flag, false));
    EXPECT_OK(clopts_option_set_default(value, "a.out"));
    EXPECT_OK(clopts_option_set_required(value, false));
    clopts_option_destroy(flag);
    clopts_option_destroy(value);
}

/* ---- clopts_option_set_negatable ------------------------------------- */

NAOS_TEST(negatable_on_non_flag)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = make(kinds[i], "color", '\0');
        NAOS_CHECK_INT_EQ(clopts_option_set_negatable(option, true), CLOPTS_DECL_NOT_NEGATABLE);
        NAOS_CHECK_FALSE(option->negatable);
        clopts_option_destroy(option);
    }
}

/* ---- terminal ---------------------------------------------------------- */

NAOS_TEST(terminal_on_flag)
{
    clopts_option_t *option;

    option = make(CLOPTS_OPTION_FLAG, "help", 'h');
    NAOS_CHECK_FALSE(option->terminal);
    EXPECT_OK(clopts_option_set_terminal(option, true));
    NAOS_CHECK_TRUE(option->terminal);
    NAOS_CHECK_TRUE(clopts_option_terminal(option));
    /* false resets it */
    EXPECT_OK(clopts_option_set_terminal(option, false));
    NAOS_CHECK_FALSE(option->terminal);
    /* a terminal FLAG may be negatable too */
    EXPECT_OK(clopts_option_set_terminal(option, true));
    EXPECT_OK(clopts_option_set_negatable(option, true));
    NAOS_CHECK_TRUE(option->terminal);
    NAOS_CHECK_TRUE(option->negatable);
    clopts_option_destroy(option);
}

NAOS_TEST(terminal_on_non_flag)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = make(kinds[i], "help", 'h');
        NAOS_CHECK_INT_EQ(clopts_option_set_terminal(option, true), CLOPTS_DECL_ATTRIBUTE_KIND);
        NAOS_CHECK_FALSE(option->terminal);
        /* false is not checked */
        EXPECT_OK(clopts_option_set_terminal(option, false));
        clopts_option_destroy(option);
    }
}

NAOS_TEST(terminal_null_option)
{
    NAOS_CHECK_INT_EQ(clopts_option_set_terminal(NULL, true), CLOPTS_DECL_INVALID_ARGUMENT);
}

/* ---- delimiter and count maximum ------------------------------------- */

NAOS_TEST(delimiter_on_non_list)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE };
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = make(kinds[i], "output", 'o');
        NAOS_CHECK_INT_EQ(clopts_option_set_delimiter(option, ','), CLOPTS_DECL_ATTRIBUTE_KIND);
        NAOS_CHECK_INT_EQ(option->delimiter, '\0');
        clopts_option_destroy(option);
    }
}

NAOS_TEST(count_max_on_non_count)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        option = make(kinds[i], "force", 'f');
        NAOS_CHECK_INT_EQ(clopts_option_set_count_max(option, 3), CLOPTS_DECL_ATTRIBUTE_KIND);
        NAOS_CHECK_UINT_EQ(option->count_max, 0);
        clopts_option_destroy(option);
    }
}

NAOS_TEST(delimiter_and_count_max_unset_are_not_checked)
{
    clopts_option_t *value;

    value = make(CLOPTS_OPTION_VALUE, "output", 'o');
    EXPECT_OK(clopts_option_set_delimiter(value, '\0'));
    EXPECT_OK(clopts_option_set_count_max(value, 0));
    clopts_option_destroy(value);
}

NAOS_SUITE(decl_option,
    NAOS_ADD_TEST(option_invalid_kind),
    NAOS_ADD_TEST(option_without_name),
    NAOS_ADD_TEST(option_long_name_invalid),
    NAOS_ADD_TEST(option_short_name_invalid),
    NAOS_ADD_TEST(option_new_valid_names),
    NAOS_ADD_TEST(env_on_flag_or_count),
    NAOS_ADD_TEST(env_empty_name),
    NAOS_ADD_TEST(env_unset_is_not_checked),
    NAOS_ADD_TEST(default_on_flag_or_count),
    NAOS_ADD_TEST(default_on_required),
    NAOS_ADD_TEST(default_unset_is_not_checked),
    NAOS_ADD_TEST(required_on_flag_or_count),
    NAOS_ADD_TEST(required_on_option_with_default),
    NAOS_ADD_TEST(required_unset_is_not_checked),
    NAOS_ADD_TEST(negatable_on_non_flag),
    NAOS_ADD_TEST(terminal_on_flag),
    NAOS_ADD_TEST(terminal_on_non_flag),
    NAOS_ADD_TEST(terminal_null_option),
    NAOS_ADD_TEST(delimiter_on_non_list),
    NAOS_ADD_TEST(count_max_on_non_count),
    NAOS_ADD_TEST(delimiter_and_count_max_unset_are_not_checked)
)
