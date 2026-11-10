/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl_option: the checks of an option alone, made by the add
 * functions (the kind and the names) and by the option setters (option.c).
 * The checks against the other options of the same program or command are in
 * the decl suite.
 *
 * A failing call changes nothing and leaves its error in the program
 * (clopts_program_get_error()). Once the program has an error, every later
 * builder call does nothing (decl_error suite), so each failing call here is
 * made on a program of its own.
 */

#include "clopts/decl/program.h"
#include "test_alloc.h"
#include "test_decl.h"

#include <naos/naos.h>

/* ---- helpers ----------------------------------------------------------- */

/* Number of elements of the array a. */
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* Checks that program has the declaration error error (CLOPTS_ERROR_NONE: none). */
#define EXPECT_ERROR(program, error) NAOS_CHECK_INT_EQ(clopts_program_get_error(program), (error))

/* A new empty program "tool"; the test stops when it cannot be made. */
static clopts_program_t *new_program(void)
{
    clopts_program_t *program = test_decl_program("tool");

    NAOS_REQUIRE_NOT_NULL(program);
    return program;
}

/* Adds a global option to program; the test stops when the add fails. */
static clopts_option_t *add(clopts_program_t *program, clopts_option_kind_t kind, const char *long_name,
                            char short_name)
{
    clopts_option_t *option = clopts_program_add_option(program, kind, long_name, short_name);

    NAOS_REQUIRE_NOT_NULL(option);
    return option;
}

/*
 * Adds an option of kind named long_name / short_name expecting error: as a
 * global option of a new program, and to a command of another new program.
 * Each add returns NULL, adds nothing, allocates nothing for good, and leaves
 * error in the program. Call it with the fault allocator installed.
 */
static void expect_add_error(clopts_option_kind_t kind, const char *long_name, char short_name, clopts_error_t error)
{
    clopts_program_t *program;
    clopts_command_t *command;
    long live;

    NAOS_NOTE("on the program");
    program = new_program();
    live = test_alloc.live;
    NAOS_CHECK_NULL(clopts_program_add_option(program, kind, long_name, short_name));
    EXPECT_ERROR(program, error);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_program_get_scope(program)), 0);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);

    NAOS_NOTE("on a command");
    program = new_program();
    command = clopts_program_add_command(program, "build");
    NAOS_REQUIRE_NOT_NULL(command);
    live = test_alloc.live;
    NAOS_CHECK_NULL(clopts_command_add_option(command, kind, long_name, short_name));
    EXPECT_ERROR(program, error);
    NAOS_CHECK_UINT_EQ(clopts_scope_get_option_count(clopts_command_get_scope(command)), 0);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(program);
}

/* ---- the kind and the names (add functions) ------------------------------ */

NAOS_TEST(option_invalid_kind)
{
    test_alloc_begin(0);
    expect_add_error((clopts_option_kind_t)4, "flag", 'f', CLOPTS_ERROR_INVALID_ARGUMENT);
    expect_add_error((clopts_option_kind_t)-1, "flag", 'f', CLOPTS_ERROR_INVALID_ARGUMENT);
    expect_add_error((clopts_option_kind_t)100, NULL, 'f', CLOPTS_ERROR_INVALID_ARGUMENT);

    /* the kind is checked first */
    expect_add_error((clopts_option_kind_t)4, NULL, '\0', CLOPTS_ERROR_INVALID_ARGUMENT);
    expect_add_error((clopts_option_kind_t)4, "-x", '=', CLOPTS_ERROR_INVALID_ARGUMENT);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(option_without_name)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE,
                                                  CLOPTS_OPTION_LIST };
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(kinds); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        expect_add_error(kinds[i], NULL, '\0', CLOPTS_ERROR_INVALID_NAME);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(option_long_name_invalid)
{
    static const char *const names[] = { "", "-o", "--o", "out=put", "out put", "out\vput" };
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < COUNT_OF(names); i++)
    {
        NAOS_NOTE("long=\"%s\"", names[i]);
        expect_add_error(CLOPTS_OPTION_VALUE, names[i], 'o', CLOPTS_ERROR_INVALID_NAME);
        expect_add_error(CLOPTS_OPTION_FLAG, names[i], '\0', CLOPTS_ERROR_INVALID_NAME);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(option_short_name_invalid)
{
    static const char shorts[] = { '-', '=', ' ', '\t' };
    size_t i;

    test_alloc_begin(0);
    for (i = 0; i < sizeof(shorts); i++)
    {
        NAOS_NOTE("short=%d", (int)shorts[i]);
        expect_add_error(CLOPTS_OPTION_FLAG, "flag", shorts[i], CLOPTS_ERROR_INVALID_NAME);
        expect_add_error(CLOPTS_OPTION_COUNT, NULL, shorts[i], CLOPTS_ERROR_INVALID_NAME);
    }
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(option_add_valid_names)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option;

    option = add(program, CLOPTS_OPTION_LIST, "no-", '4');
    NAOS_CHECK_INT_EQ(clopts_option_get_kind(option), CLOPTS_OPTION_LIST);
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(option), "no-");
    NAOS_CHECK_INT_EQ(clopts_option_get_short_name(option), '4');

    /* there are no reserved names: help, h and version are ordinary names */
    option = add(program, CLOPTS_OPTION_FLAG, "help", 'h');
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(option), "help");
    option = add(program, CLOPTS_OPTION_VALUE, "version", 'V');
    NAOS_CHECK_STR_EQ(clopts_option_get_long_name(option), "version");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/* ---- clopts_option_set_env ------------------------------------------- */

NAOS_TEST(env_on_flag_or_count)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_program();
    option = add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_env(option, "TOOL_FORCE");
    EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
    NAOS_CHECK_NULL(clopts_option_get_env(option));
    clopts_program_destroy(program);

    /* the kind is checked before the empty name */
    program = new_program();
    option = add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_env(option, "");
    EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
    NAOS_CHECK_NULL(clopts_option_get_env(option));
    clopts_program_destroy(program);
}

NAOS_TEST(env_empty_name)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_program();
    option = add(program, CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_env(option, "");
    EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_NULL(clopts_option_get_env(option));
    clopts_program_destroy(program);

    /* a failing call keeps the previous value */
    program = new_program();
    option = add(program, CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_env(option, "TOOL_INCLUDE");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_option_set_env(option, "");
    EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_STR_EQ(clopts_option_get_env(option), "TOOL_INCLUDE");
    clopts_program_destroy(program);
}

/* No system can look up a name with '=', and the name is converted from UTF-8 on Windows. */
NAOS_TEST(env_name_with_equals_or_invalid_utf8)
{
    static const char *const bad[] = {
        "A=B", "=A", "A=",
        "\xff",                     /* never in UTF-8 */
        "A\xc3",                    /* truncated sequence */
        "\xc0\xaf",                 /* overlong form of '/' */
        "\xe0\x80\xaf",             /* overlong form of '/' */
        "\xed\xa0\x80",             /* surrogate U+D800 */
        "\xf4\x90\x80\x80",         /* above U+10FFFF */
        "\x80",                     /* lone continuation byte */
    };
    static const char *const good[] = {
        "TOOL_FORMAT", "tool-format", "A B",
        "\xe5\x90\x8d",             /* U+540D */
        "\xf0\x9f\x98\x80",         /* U+1F600 */
        "\xed\x9f\xbf",             /* U+D7FF, just below the surrogates */
        "\xf4\x8f\xbf\xbf",         /* U+10FFFF */
    };
    clopts_program_t *program;
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < COUNT_OF(bad); i++)
    {
        NAOS_NOTE("bad name %u", (unsigned)i);
        program = new_program();
        option = add(program, CLOPTS_OPTION_VALUE, "format", '\0');
        clopts_option_set_env(option, bad[i]);
        EXPECT_ERROR(program, CLOPTS_ERROR_INVALID_NAME);
        NAOS_CHECK_NULL(clopts_option_get_env(option));
        clopts_program_destroy(program);
    }

    program = new_program();
    option = add(program, CLOPTS_OPTION_VALUE, "format", '\0');
    for (i = 0; i < COUNT_OF(good); i++)
    {
        NAOS_NOTE("good name %u", (unsigned)i);
        clopts_option_set_env(option, good[i]);
        NAOS_CHECK_STR_EQ(clopts_option_get_env(option), good[i]);
        clopts_option_set_env(option, NULL);
    }
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

NAOS_TEST(env_unset_is_not_checked)
{
    clopts_program_t *program = new_program();
    clopts_option_t *flag = add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_t *value = add(program, CLOPTS_OPTION_VALUE, "output", 'o');

    clopts_option_set_env(flag, NULL);
    clopts_option_set_env(value, "TOOL_OUTPUT");
    NAOS_CHECK_STR_EQ(clopts_option_get_env(value), "TOOL_OUTPUT");
    clopts_option_set_env(value, NULL);
    NAOS_CHECK_NULL(clopts_option_get_env(value));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/* ---- clopts_option_set_default --------------------------------------- */

NAOS_TEST(default_on_flag_or_count)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_program();
    option = add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_default(option, "1");
    EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
    NAOS_CHECK_NULL(clopts_option_get_default(option));
    clopts_program_destroy(program);

    program = new_program();
    option = add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_default(option, "2");
    EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
    NAOS_CHECK_NULL(clopts_option_get_default(option));
    clopts_program_destroy(program);
}

NAOS_TEST(default_on_required)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_program();
    option = add(program, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_required(option, true);
    clopts_option_set_default(option, "a.out");
    EXPECT_ERROR(program, CLOPTS_ERROR_CONFLICT);
    NAOS_CHECK_NULL(clopts_option_get_default(option));
    NAOS_CHECK_TRUE(clopts_option_is_required(option));
    clopts_program_destroy(program);

    /* allowed once required is reset */
    program = new_program();
    option = add(program, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_required(option, true);
    clopts_option_set_required(option, false);
    clopts_option_set_default(option, "a.out");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_STR_EQ(clopts_option_get_default(option), "a.out");
    clopts_program_destroy(program);
}

NAOS_TEST(default_unset_is_not_checked)
{
    clopts_program_t *program = new_program();
    clopts_option_t *flag = add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_t *list = add(program, CLOPTS_OPTION_LIST, "include", 'I');

    clopts_option_set_default(flag, NULL);
    clopts_option_set_required(list, true);
    clopts_option_set_default(list, NULL);
    NAOS_CHECK_TRUE(clopts_option_is_required(list));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/* ---- clopts_option_set_required -------------------------------------- */

NAOS_TEST(required_on_flag_or_count)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_program();
    option = add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_set_required(option, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
    NAOS_CHECK_FALSE(clopts_option_is_required(option));
    clopts_program_destroy(program);

    program = new_program();
    option = add(program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_required(option, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
    NAOS_CHECK_FALSE(clopts_option_is_required(option));
    clopts_program_destroy(program);
}

NAOS_TEST(required_on_option_with_default)
{
    clopts_program_t *program;
    clopts_option_t *option;

    program = new_program();
    option = add(program, CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_default(option, "a,b");
    clopts_option_set_required(option, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_CONFLICT);
    NAOS_CHECK_FALSE(clopts_option_is_required(option));
    NAOS_CHECK_STR_EQ(clopts_option_get_default(option), "a,b");
    clopts_program_destroy(program);

    /* allowed once the default is reset */
    program = new_program();
    option = add(program, CLOPTS_OPTION_LIST, "include", 'I');
    clopts_option_set_default(option, "a,b");
    clopts_option_set_default(option, NULL);
    clopts_option_set_required(option, true);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    NAOS_CHECK_TRUE(clopts_option_is_required(option));
    clopts_program_destroy(program);
}

NAOS_TEST(required_unset_is_not_checked)
{
    clopts_program_t *program = new_program();
    clopts_option_t *flag = add(program, CLOPTS_OPTION_FLAG, "force", 'f');
    clopts_option_t *value = add(program, CLOPTS_OPTION_VALUE, "output", 'o');

    clopts_option_set_required(flag, false);
    clopts_option_set_default(value, "a.out");
    clopts_option_set_required(value, false);
    NAOS_CHECK_STR_EQ(clopts_option_get_default(value), "a.out");
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

/* ---- clopts_option_set_negatable ------------------------------------- */

NAOS_TEST(negatable_on_non_flag)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_program_t *program;
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < COUNT_OF(kinds); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        program = new_program();
        option = add(program, kinds[i], "color", '\0');
        clopts_option_set_negatable(option, true);
        EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
        NAOS_CHECK_FALSE(clopts_option_is_negatable(option));
        clopts_program_destroy(program);
    }
}

/* ---- terminal ---------------------------------------------------------- */

NAOS_TEST(terminal_on_flag)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option = add(program, CLOPTS_OPTION_FLAG, "help", 'h');

    NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
    clopts_option_set_terminal(option, true);
    NAOS_CHECK_TRUE(clopts_option_is_terminal(option));
    clopts_option_set_terminal(option, true);
    NAOS_CHECK_TRUE(clopts_option_is_terminal(option));
    /* false resets it */
    clopts_option_set_terminal(option, false);
    NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
    /* a terminal FLAG may be negatable too */
    clopts_option_set_terminal(option, true);
    clopts_option_set_negatable(option, true);
    NAOS_CHECK_TRUE(clopts_option_is_terminal(option));
    NAOS_CHECK_TRUE(clopts_option_is_negatable(option));
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

NAOS_TEST(terminal_on_non_flag)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_program_t *program;
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < COUNT_OF(kinds); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        program = new_program();
        option = add(program, kinds[i], "help", 'h');
        clopts_option_set_terminal(option, true);
        EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
        NAOS_CHECK_FALSE(clopts_option_is_terminal(option));
        clopts_program_destroy(program);

        /* false is not checked */
        program = new_program();
        option = add(program, kinds[i], "help", 'h');
        clopts_option_set_terminal(option, false);
        EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
        clopts_program_destroy(program);
    }
}

/* ---- delimiter and count maximum ------------------------------------- */

NAOS_TEST(delimiter_on_non_list)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_COUNT, CLOPTS_OPTION_VALUE };
    clopts_program_t *program;
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < COUNT_OF(kinds); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        program = new_program();
        option = add(program, kinds[i], "output", 'o');
        clopts_option_set_delimiter(option, ',');
        EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
        NAOS_CHECK_INT_EQ(clopts_option_get_delimiter(option), '\0');
        clopts_program_destroy(program);
    }
}

NAOS_TEST(count_max_on_non_count)
{
    static const clopts_option_kind_t kinds[] = { CLOPTS_OPTION_FLAG, CLOPTS_OPTION_VALUE, CLOPTS_OPTION_LIST };
    clopts_program_t *program;
    clopts_option_t *option;
    size_t i;

    for (i = 0; i < COUNT_OF(kinds); i++)
    {
        NAOS_NOTE("kind=%d", (int)kinds[i]);
        program = new_program();
        option = add(program, kinds[i], "force", 'f');
        clopts_option_set_count_max(option, 3);
        EXPECT_ERROR(program, CLOPTS_ERROR_NOT_APPLICABLE);
        NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(option), 0);
        clopts_program_destroy(program);
    }
}

NAOS_TEST(delimiter_and_count_max_unset_are_not_checked)
{
    clopts_program_t *program = new_program();
    clopts_option_t *value = add(program, CLOPTS_OPTION_VALUE, "output", 'o');

    clopts_option_set_delimiter(value, '\0');
    clopts_option_set_count_max(value, 0);
    EXPECT_ERROR(program, CLOPTS_ERROR_NONE);
    clopts_program_destroy(program);
}

NAOS_SUITE(decl_option,
    NAOS_ADD_TEST(option_invalid_kind),
    NAOS_ADD_TEST(option_without_name),
    NAOS_ADD_TEST(option_long_name_invalid),
    NAOS_ADD_TEST(option_short_name_invalid),
    NAOS_ADD_TEST(option_add_valid_names),
    NAOS_ADD_TEST(env_on_flag_or_count),
    NAOS_ADD_TEST(env_empty_name),
    NAOS_ADD_TEST(env_name_with_equals_or_invalid_utf8),
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
    NAOS_ADD_TEST(delimiter_on_non_list),
    NAOS_ADD_TEST(count_max_on_non_count),
    NAOS_ADD_TEST(delimiter_and_count_max_unset_are_not_checked)
)
