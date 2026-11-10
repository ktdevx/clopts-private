/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite result: the result handle (recording rules, accessors, the
 * terminal option and release), driven through the internal API.
 *
 * A program is built with the builder; a result is created with
 * clopts_parser_result_new(), its fields (status, error) are set
 * directly and values are recorded with the slot functions. A result does
 * not point to its program: every slot knows its option, and an option
 * knows its index in its scope, so an accessor finds a value in constant
 * time and checks it by address.
 *
 * The last part goes through clopts_parse_with_env() with the shared fixtures
 * of test_fixtures.c (ported from the first-version suite).
 */

#include "parser/parser_internal.h"
#include "test_alloc.h"
#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <limits.h>
#include <string.h>

/* ---- fixture ---------------------------------------------------------- */

/*
 * tool
 *   global: --force/-f (FLAG, negatable), --verbose/-v (COUNT),
 *           --jobs/-j (COUNT, max 8), --config/-c (VALUE),
 *           --include/-I (LIST, ','), --tag/-t (LIST, no delimiter),
 *           --help/-h (FLAG, terminal)
 *   build:  --release (FLAG), --output/-o (VALUE)
 *   copy:   --output (VALUE)
 *   empty:  no options
 */
typedef struct sample
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of sample_init() succeeded */
    clopts_option_t *force;
    clopts_option_t *verbose;
    clopts_option_t *jobs;
    clopts_option_t *config;
    clopts_option_t *include;
    clopts_option_t *tag;
    clopts_option_t *help;
    clopts_command_t *build;
    clopts_option_t *release;
    clopts_option_t *build_output;
    clopts_command_t *copy;
    clopts_option_t *copy_output;
    clopts_command_t *empty;
} sample_t;

/*
 * Builds the sample program: the program and its global options, then the
 * commands, which test_decl_finish() adds. When the program cannot be
 * completed everything is released and the whole struct is NULL (ok false).
 */
static void sample_init(sample_t *s)
{
    memset(s, 0, sizeof(*s));
    test_decl_reset();
    s->program = test_decl_begin("tool");
    s->force = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    test_decl_check(clopts_option_set_negatable(s->force, true));
    s->force = test_decl_global_attach(s->program, s->force);
    s->verbose = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    s->verbose = test_decl_global_attach(s->program, s->verbose);
    s->jobs = test_decl_option(CLOPTS_OPTION_COUNT, "jobs", 'j');
    test_decl_check(clopts_option_set_count_max(s->jobs, 8));
    s->jobs = test_decl_global_attach(s->program, s->jobs);
    s->config = test_decl_option(CLOPTS_OPTION_VALUE, "config", 'c');
    s->config = test_decl_global_attach(s->program, s->config);
    s->include = test_decl_option(CLOPTS_OPTION_LIST, "include", 'I');
    test_decl_check(clopts_option_set_delimiter(s->include, ','));
    s->include = test_decl_global_attach(s->program, s->include);
    s->tag = test_decl_option(CLOPTS_OPTION_LIST, "tag", 't');
    s->tag = test_decl_global_attach(s->program, s->tag);
    s->help = test_decl_option(CLOPTS_OPTION_FLAG, "help", 'h');
    test_decl_check(clopts_option_set_terminal(s->help, true));
    s->help = test_decl_global_attach(s->program, s->help);
    s->build = test_decl_command(s->program, "build");
    s->release = test_decl_option(CLOPTS_OPTION_FLAG, "release", '\0');
    s->release = test_decl_attach(s->build, s->release);
    s->build_output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    s->build_output = test_decl_attach(s->build, s->build_output);
    s->copy = test_decl_command(s->program, "copy");
    s->copy_output = test_decl_option(CLOPTS_OPTION_VALUE, "output", '\0');
    s->copy_output = test_decl_attach(s->copy, s->copy_output);
    s->empty = test_decl_command(s->program, "empty");
    s->program = test_decl_finish(s->program);
    if (s->program == NULL)
    {
        memset(s, 0, sizeof(*s));
    }
    s->ok = (test_decl_failures == 0);
}

/* Sets *index to the position of option in the options of scope; false when it is not there. */
static bool index_in(const clopts_scope_t *scope, const clopts_option_t *option, size_t *index)
{
    size_t i;

    for (i = 0; i < clopts_scope_option_count(scope); i++)
    {
        if (clopts_scope_option_at(scope, i) == option)
        {
            *index = i;
            return true;
        }
    }
    return false;
}

/* The slots of the global options (global true) or of the selected command, through the reader of result.c. */
static clopts_parser_slot_t *test_slots(clopts_result_t *result, bool global)
{
    size_t count;

    return clopts_parser_result_get_slots(result, global, &count);
}

/* The number of slots of the global options (global true) or of the selected command. */
static size_t test_slot_count(clopts_result_t *result, bool global)
{
    size_t count;

    (void)clopts_parser_result_get_slots(result, global, &count);
    return count;
}

/*
 * Returns the slot of option of s (a global option or one of the selected command),
 * found by its position in the options of its scope (not by its index,
 * which the tests below check on their own).
 */
static clopts_parser_slot_t *slot_of(const sample_t *s, clopts_result_t *result, const clopts_option_t *option)
{
    clopts_parser_ref_t ref;

    ref.option = option;
    ref.negated = false;
    ref.global = index_in(clopts_program_scope(s->program), option, &ref.index);
    if (!ref.global)
    {
        NAOS_REQUIRE_NOT_NULL(clopts_result_get_command(result));
        NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(clopts_result_get_command(result)), option, &ref.index));
    }
    return clopts_parser_result_slot(result, &ref);
}

/*
 * Starts the allocation count of the test (test_alloc_begin, before anything
 * of the test is allocated), builds the sample program into s and creates a
 * result of it.
 */
#define SAMPLE_BEGIN(s, result)                                                     \
    do                                                                              \
    {                                                                               \
        test_alloc_begin(0);                                                        \
        sample_init(&(s));                                                          \
        NAOS_REQUIRE_NOT_NULL((s).program);                                         \
        NAOS_REQUIRE_TRUE((s).ok);                                                  \
        (result) = clopts_parser_result_new((s).program);                           \
        NAOS_REQUIRE_NOT_NULL(result);                                              \
    } while (0)

/* Checks that every value accessor returns the unset value for option. */
#define CHECK_UNSET(result, option)                                                 \
    do                                                                              \
    {                                                                               \
        NAOS_CHECK_FALSE(clopts_result_get_flag((result), (option)));                   \
        NAOS_CHECK_UINT_EQ(clopts_result_get_count((result), (option)), 0);             \
        NAOS_CHECK_NULL(clopts_result_get_value((result), (option)));                   \
        NAOS_CHECK_UINT_EQ(clopts_result_get_list_count((result), (option)), 0);        \
        NAOS_CHECK_INT_EQ(clopts_result_get_source((result), (option)), CLOPTS_SOURCE_NONE); \
    } while (0)

/* Checks that every error accessor returns "no error". */
#define CHECK_NO_ERROR(result)                                                      \
    do                                                                              \
    {                                                                               \
        NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_NONE);    \
        NAOS_CHECK_NULL(clopts_result_get_error_command(result));                       \
        NAOS_CHECK_NULL(clopts_result_get_error_option(result));                        \
        NAOS_CHECK_NULL(clopts_result_get_error_positional(result));                    \
        NAOS_CHECK_NULL(clopts_result_get_error_token(result));                         \
        NAOS_CHECK_INT_EQ(clopts_result_get_error_argv_index(result), -1);              \
        NAOS_CHECK_UINT_EQ(clopts_result_get_error_name_len(result), 0);                \
        NAOS_CHECK_INT_EQ(clopts_result_get_error_short_name(result), '\0');            \
    } while (0)

/* ---- allocation ------------------------------------------------------- */

NAOS_TEST(new_allocates_one_slot_per_global_option)
{
    sample_t s;
    clopts_result_t *result;
    size_t i;
    long live;

    SAMPLE_BEGIN(s, result);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_NULL(clopts_parser_result_get_terminal(result));
    NAOS_REQUIRE_UINT_EQ(test_slot_count(result, true), 7);
    NAOS_REQUIRE_NOT_NULL(test_slots(result, true));
    NAOS_CHECK_NULL(test_slots(result, false));
    NAOS_CHECK_UINT_EQ(test_slot_count(result, false), 0);
    for (i = 0; i < test_slot_count(result, true); i++)
    {
        /* Slot i records global option i. */
        NAOS_CHECK_PTR_EQ(test_slots(result, true)[i].option,
                          clopts_program_get_option_at(s.program, i));
        NAOS_CHECK_INT_EQ(test_slots(result, true)[i].source, CLOPTS_SOURCE_NONE);
        NAOS_CHECK_NULL(test_slots(result, true)[i].value);
        NAOS_CHECK_NULL(test_slots(result, true)[i].owned);
        NAOS_CHECK_UINT_EQ(test_slots(result, true)[i].items.len, 0);
        NAOS_CHECK_UINT_EQ(test_slots(result, true)[i].items.elem_size, sizeof(char *));
    }

    /* The result and its slot array are two blocks of the allocator. */
    live = test_alloc.live;
    clopts_result_destroy(result);
    NAOS_CHECK_INT_EQ(test_alloc.live, live - 2);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(new_without_global_options_has_no_slots)
{
    clopts_program_t *program;
    clopts_result_t *result;
    size_t calls;
    long live;

    test_alloc_begin(0);
    program = test_decl_program("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    calls = test_alloc.calls;
    live = test_alloc.live;
    result = clopts_parser_result_new(program);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_NULL(test_slots(result, true));
    NAOS_CHECK_UINT_EQ(test_slot_count(result, true), 0);
    /* Only the result itself is allocated. */
    NAOS_CHECK_UINT_EQ(test_alloc.calls, calls + 1);
    NAOS_CHECK_INT_EQ(test_alloc.live, live + 1);
    clopts_result_destroy(result);
    clopts_program_destroy(program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(select_allocates_command_slots)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), s.build);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), s.build);
    NAOS_REQUIRE_UINT_EQ(test_slot_count(result, false), 2);
    NAOS_REQUIRE_NOT_NULL(test_slots(result, false));
    NAOS_CHECK_PTR_EQ(test_slots(result, false)[0].option, s.release);
    NAOS_CHECK_PTR_EQ(test_slots(result, false)[1].option, s.build_output);
    NAOS_CHECK_INT_EQ(test_slots(result, false)[1].source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(test_slots(result, false)[1].items.elem_size, sizeof(char *));

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(select_command_without_options)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.empty), 0);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), s.empty);
    NAOS_CHECK_NULL(test_slots(result, false));
    NAOS_CHECK_UINT_EQ(test_slot_count(result, false), 0);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(result_slot_returns_the_slot_of_the_ref)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    NAOS_CHECK_PTR_EQ(slot_of(&s, result, s.force), &test_slots(result, true)[0]);
    NAOS_CHECK_PTR_EQ(slot_of(&s, result, s.tag), &test_slots(result, true)[5]);
    NAOS_CHECK_PTR_EQ(slot_of(&s, result, s.help), &test_slots(result, true)[6]);
    NAOS_CHECK_PTR_EQ(slot_of(&s, result, s.release), &test_slots(result, false)[0]);
    NAOS_CHECK_PTR_EQ(slot_of(&s, result, s.build_output), &test_slots(result, false)[1]);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

/* ---- recording rules -------------------------------------------------- */

NAOS_TEST(flag_last_one_wins_including_negation)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.force);
    clopts_parser_slot_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.force));
    clopts_parser_slot_set_flag(slot, false, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, s.force));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.force), CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.force));

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

NAOS_TEST(count_clamps_at_its_max)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    int i;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.jobs);
    for (i = 0; i < 12; i++)
    {
        clopts_parser_slot_add_count(slot, clopts_option_get_count_max(s.jobs));
    }
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.jobs), 8);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.jobs), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

NAOS_TEST(count_default_max_is_uint_max)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    int i;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.verbose);
    NAOS_CHECK_UINT_EQ(clopts_option_get_count_max(s.verbose), 0);
    for (i = 0; i < 12; i++)
    {
        clopts_parser_slot_add_count(slot, clopts_option_get_count_max(s.verbose));
    }
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.verbose), 12);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.verbose), CLOPTS_SOURCE_EXPLICIT);

    slot->count = UINT_MAX - 1u;
    clopts_parser_slot_add_count(slot, 0);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.verbose), UINT_MAX);
    clopts_parser_slot_add_count(slot, 0);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.verbose), UINT_MAX);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

NAOS_TEST(value_last_one_wins)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    char first[] = "a.conf";
    char second[] = "b.conf";

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.config);
    clopts_parser_slot_set_value(slot, first, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, s.config), first);
    clopts_parser_slot_set_value(slot, second, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, s.config), second);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.config), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

NAOS_TEST(list_concatenates_split_items)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    char first[] = "a,b";
    char second[] = "c";

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.include);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, first, ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, second, ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, s.include), 3);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 2), "c");
    /* items are copies owned by the result */
    NAOS_CHECK_TRUE(clopts_result_get_list_at(result, s.include, 2) != second);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.include), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(list_keeps_empty_items)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.include);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, "a,,b,", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, s.include), 4);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 1), "");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 2), "b");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 3), "");

    /* an empty value is one empty item */
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, "", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, s.include), 5);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 4), "");

    /* a lone delimiter gives two empty items */
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, ",", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, s.include), 7);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 5), "");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 6), "");

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(list_without_delimiter_takes_the_value_as_one_item)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.tag);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, "a,b", '\0', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, "", '\0', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, s.tag), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.tag, 0), "a,b");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.tag, 1), "");

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(source_is_overwritten_by_the_last_record)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.config);
    clopts_parser_slot_set_value(slot, "default", CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.config), CLOPTS_SOURCE_DEFAULT);
    clopts_parser_slot_set_value(slot, "env", CLOPTS_SOURCE_ENV);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.config), CLOPTS_SOURCE_ENV);
    clopts_parser_slot_set_value(slot, "argv", CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.config), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.config), "argv");

    slot = slot_of(&s, result, s.include);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, "x", ',', CLOPTS_SOURCE_DEFAULT), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.include), CLOPTS_SOURCE_DEFAULT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot, "y", ',', CLOPTS_SOURCE_ENV), 0);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.include), CLOPTS_SOURCE_ENV);

    slot = slot_of(&s, result, s.force);
    clopts_parser_slot_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.force), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(environment_copy_is_owned_by_the_slot)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    char *copy;
    char *second;
    long before;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(&s, result, s.config);
    before = test_alloc.live;
    copy = clopts_core_strdup("from-env");
    NAOS_REQUIRE_NOT_NULL(copy);
    clopts_parser_slot_set_owned_value(slot, copy, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_PTR_EQ(slot->owned, copy);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, s.config), copy);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_INT_EQ(test_alloc.live, before + 1);

    /* owning a new copy releases the previous one */
    second = clopts_core_strdup("again");
    NAOS_REQUIRE_NOT_NULL(second);
    clopts_parser_slot_set_owned_value(slot, second, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_PTR_EQ(slot->owned, second);
    NAOS_CHECK_PTR_EQ(slot->value, second);
    NAOS_CHECK_INT_EQ(test_alloc.live, before + 1);

    /* a borrowed value replaces it and the copy is released (value == owned) */
    clopts_parser_slot_set_value(slot, "argv", CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_NULL(slot->owned);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.config), "argv");
    NAOS_CHECK_INT_EQ(test_alloc.live, before);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(positionals_point_into_argv_in_order)
{
    sample_t s;
    clopts_result_t *result;
    char t0[] = "src";
    char t1[] = "-";
    char t2[] = "dst";
    char *argv[3];
    size_t i;
    int j;

    argv[0] = t0;
    argv[1] = t1;
    argv[2] = t2;
    SAMPLE_BEGIN(s, result);
    for (j = 0; j < 30; j++)
    {
        NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, argv[j % 3]), 0);
    }
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 30);
    for (i = 0; i < 30; i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, i), argv[i % 3]);
    }

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- accessors by handle ---------------------------------------------- */

NAOS_TEST(accessors_return_the_slot_of_the_handle)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_flag(slot_of(&s, result, s.force), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_add_count(slot_of(&s, result, s.verbose), 0);
    clopts_parser_slot_add_count(slot_of(&s, result, s.verbose), 0);
    clopts_parser_slot_add_count(slot_of(&s, result, s.jobs), 8);
    clopts_parser_slot_set_value(slot_of(&s, result, s.config), "c.conf", CLOPTS_SOURCE_DEFAULT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot_of(&s, result, s.include), "i", ',',
                                                       CLOPTS_SOURCE_ENV), 0);
    clopts_parser_slot_set_flag(slot_of(&s, result, s.release), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_value(slot_of(&s, result, s.build_output), "out", CLOPTS_SOURCE_EXPLICIT);

    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.force));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.verbose), 2);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.jobs), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.config), "c.conf");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.config), CLOPTS_SOURCE_DEFAULT);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, s.include), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 0), "i");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.include), CLOPTS_SOURCE_ENV);
    CHECK_UNSET(result, s.tag);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.release));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.build_output), "out");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.build_output), CLOPTS_SOURCE_EXPLICIT);

    /* an option of a command that was not selected (same long name) is unset */
    CHECK_UNSET(result, s.copy_output);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(command_options_are_unset_without_a_selected_command)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    CHECK_UNSET(result, s.release);
    CHECK_UNSET(result, s.build_output);
    CHECK_UNSET(result, s.force);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

NAOS_TEST(null_option_reads_as_unset)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    clopts_parser_slot_set_flag(slot_of(&s, result, s.force), true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    CHECK_UNSET(result, NULL);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

/* Every attached option is at the position of its attachment in the options of its scope (program or command). */
NAOS_TEST(options_keep_their_attach_position_in_their_command)
{
    sample_t s;
    const clopts_option_t *global_options[7];
    size_t index = 0;
    size_t i;

    test_alloc_begin(0);
    sample_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    global_options[0] = s.force;
    global_options[1] = s.verbose;
    global_options[2] = s.jobs;
    global_options[3] = s.config;
    global_options[4] = s.include;
    global_options[5] = s.tag;
    global_options[6] = s.help;
    for (i = 0; i < sizeof(global_options) / sizeof(global_options[0]); i++)
    {
        NAOS_REQUIRE_TRUE(index_in(clopts_program_scope(s.program), global_options[i], &index));
        NAOS_CHECK_UINT_EQ(index, i);
    }
    NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(s.build), s.release, &index));
    NAOS_CHECK_UINT_EQ(index, 0);
    NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(s.build), s.build_output, &index));
    NAOS_CHECK_UINT_EQ(index, 1);
    /* copy's --output is at the same position as build's --release. */
    NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(s.copy), s.copy_output, &index));
    NAOS_CHECK_UINT_EQ(index, 0);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * An option is found by its address, so an option at the same position as a
 * recorded one, but of a command that was not selected, of another program or
 * not attached at all, reads as unset.
 */
NAOS_TEST(option_at_the_same_position_elsewhere_reads_as_unset)
{
    sample_t s;
    sample_t other;
    clopts_option_t *loose;
    clopts_result_t *result;
    size_t pos_a = 0;
    size_t pos_b = 0;

    SAMPLE_BEGIN(s, result);
    sample_init(&other);
    NAOS_REQUIRE_NOT_NULL(other.program);
    NAOS_REQUIRE_TRUE(other.ok);
    loose = test_decl_option(CLOPTS_OPTION_FLAG, "force", 'f');
    NAOS_REQUIRE_NOT_NULL(loose);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_flag(slot_of(&s, result, s.force), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_flag(slot_of(&s, result, s.release), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_value(slot_of(&s, result, s.build_output), "out", CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_REQUIRE_TRUE(clopts_result_get_flag(result, s.force));
    NAOS_REQUIRE_TRUE(clopts_result_get_flag(result, s.release));
    NAOS_REQUIRE_STR_EQ(clopts_result_get_value(result, s.build_output), "out");

    /* Same position as a recorded global slot and command slot. */
    NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(s.build), s.release, &pos_a));
    NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(s.copy), s.copy_output, &pos_b));
    NAOS_REQUIRE_UINT_EQ(pos_a, pos_b);
    NAOS_REQUIRE_TRUE(index_in(clopts_program_scope(s.program), s.force, &pos_a));
    NAOS_REQUIRE_TRUE(index_in(clopts_program_scope(other.program), other.force, &pos_b));
    NAOS_REQUIRE_UINT_EQ(pos_a, pos_b);
    NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(s.build), s.build_output, &pos_a));
    NAOS_REQUIRE_TRUE(index_in(clopts_command_scope(other.build), other.build_output, &pos_b));
    NAOS_REQUIRE_UINT_EQ(pos_a, pos_b);
    /* An option of a command that was not selected. */
    CHECK_UNSET(result, s.copy_output);
    /* Options of another program: global, and of the command of the same name. */
    CHECK_UNSET(result, other.force);
    CHECK_UNSET(result, other.release);
    CHECK_UNSET(result, other.build_output);
    CHECK_UNSET(result, other.help);
    /* An unattached option. */
    CHECK_UNSET(result, loose);

    clopts_option_destroy(loose);
    clopts_result_destroy(result);
    clopts_program_destroy(other.program);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* An index beyond the slots of both scopes reads as unset too. */
NAOS_TEST(option_beyond_the_slots_reads_as_unset)
{
    sample_t s;
    clopts_program_t *small;
    clopts_option_t *first;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    clopts_result_destroy(result);
    /* A one-option program without commands: its result has a single global slot. */
    small = test_decl_begin("small");
    first = test_decl_global_add(small, CLOPTS_OPTION_FLAG, "first", '\0');
    small = test_decl_finish(small);
    NAOS_REQUIRE_NOT_NULL(small);
    NAOS_REQUIRE_NOT_NULL(first);
    result = clopts_parser_result_new(small);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_REQUIRE_UINT_EQ(test_slot_count(result, true), 1);
    NAOS_REQUIRE_UINT_EQ(test_slot_count(result, false), 0);
    clopts_parser_slot_set_flag(&test_slots(result, true)[0], true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_TRUE(clopts_result_get_flag(result, first));
    /* s.help has index 6 and s.build_output index 1: beyond the single slot. */
    CHECK_UNSET(result, s.help);
    CHECK_UNSET(result, s.build_output);
    /* s.force has index 0, the index of first: the address check rejects it. */
    CHECK_UNSET(result, s.force);
    clopts_parser_result_finish(result, CLOPTS_PARSE_NO_COMMAND);
    NAOS_REQUIRE_TRUE(clopts_result_get_flag(result, first));
    CHECK_UNSET(result, s.help);
    CHECK_UNSET(result, s.force);

    clopts_result_destroy(result);
    clopts_program_destroy(small);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ---- per-status defaults ---------------------------------------------- */

/* Records a value in every global option of s and in build's options, and adds a positional. */
static void record_everything(sample_t *s, clopts_result_t *result)
{
    static char token[] = "file";

    (void)clopts_parser_result_select(result, s->build);
    clopts_parser_slot_set_flag(slot_of(s, result, s->force), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_add_count(slot_of(s, result, s->verbose), 0);
    clopts_parser_slot_add_count(slot_of(s, result, s->jobs), 8);
    clopts_parser_slot_set_value(slot_of(s, result, s->config), "c", CLOPTS_SOURCE_EXPLICIT);
    (void)clopts_parser_slot_append_list(slot_of(s, result, s->include), "a,b", ',',
                                         CLOPTS_SOURCE_EXPLICIT);
    (void)clopts_parser_slot_append_list(slot_of(s, result, s->tag), "t", '\0',
                                         CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_flag(slot_of(s, result, s->release), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_value(slot_of(s, result, s->build_output), "o", CLOPTS_SOURCE_EXPLICIT);
    (void)clopts_parser_result_add_positional(result, token);
}

NAOS_TEST(value_accessors_are_unset_unless_ok_terminated_or_no_command)
{
    static const clopts_parse_status_t statuses[] = {
        CLOPTS_PARSE_USAGE_ERROR, CLOPTS_PARSE_OUT_OF_MEMORY
    };
    sample_t s;
    clopts_result_t *result;
    size_t i;

    SAMPLE_BEGIN(s, result);
    record_everything(&s, result);
    NAOS_REQUIRE_PTR_EQ(clopts_result_get_command(result), s.build);
    NAOS_REQUIRE_TRUE(clopts_result_get_flag(result, s.force));
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 1);

    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++)
    {
        clopts_parser_result_finish(result, statuses[i]);
        NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), statuses[i]);
        CHECK_UNSET(result, s.force);
        CHECK_UNSET(result, s.verbose);
        CHECK_UNSET(result, s.jobs);
        CHECK_UNSET(result, s.config);
        CHECK_UNSET(result, s.include);
        CHECK_UNSET(result, s.tag);
        CHECK_UNSET(result, s.release);
        CHECK_UNSET(result, s.build_output);
        CHECK_UNSET(result, s.help);
        NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 0);
    }

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(no_command_keeps_only_the_global_values)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    record_everything(&s, result);
    clopts_parser_result_finish(result, CLOPTS_PARSE_NO_COMMAND);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.force));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.verbose), 1);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.jobs), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.config), "c");
    NAOS_CHECK_UINT_EQ(clopts_result_get_list_count(result, s.include), 2);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.tag), CLOPTS_SOURCE_EXPLICIT);
    CHECK_UNSET(result, s.release);
    CHECK_UNSET(result, s.build_output);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/*
 * TERMINATED: the values recorded before the terminal option are readable,
 * the global ones and those of the selected command, and so are the
 * positionals.
 */
NAOS_TEST(terminated_keeps_the_global_and_command_values)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    record_everything(&s, result);
    clopts_parser_slot_set_flag(slot_of(&s, result, s.help), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_result_set_terminal(result, s.help);
    clopts_parser_result_finish(result, CLOPTS_PARSE_TERMINATED);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), s.help);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), s.build);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.help));
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.force));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.verbose), 1);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.jobs), 1);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.config), "c");
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, s.include), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, s.include, 1), "b");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, s.tag), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, s.release));
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, s.build_output), "o");
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);
    /* An option of a command that was not selected still reads as unset. */
    CHECK_UNSET(result, s.copy_output);
    CHECK_NO_ERROR(result);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* Fills every error field of result. */
static void fill_error(sample_t *s, clopts_result_t *result)
{
    static char token[] = "-xq";
    clopts_parser_error_t error;

    error.usage = CLOPTS_USAGE_UNKNOWN_OPTION;
    error.command = s->build;
    error.option = s->release;
    error.positional = NULL;
    error.token = token;
    error.argv_index = 3;
    error.name_len = 2;
    error.short_name = 'q';
    clopts_parser_result_set_error(result, &error);
}

NAOS_TEST(error_accessors_report_only_errors)
{
    static const clopts_parse_status_t statuses[] = {
        CLOPTS_PARSE_OK, CLOPTS_PARSE_TERMINATED, CLOPTS_PARSE_NO_COMMAND, CLOPTS_PARSE_OUT_OF_MEMORY
    };
    sample_t s;
    clopts_result_t *result;
    size_t i;

    SAMPLE_BEGIN(s, result);
    fill_error(&s, result);
    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++)
    {
        clopts_parser_result_finish(result, statuses[i]);
        CHECK_NO_ERROR(result);
    }

    clopts_parser_result_finish(result, CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_command(result), s.build);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_option(result), s.release);
    NAOS_CHECK_NULL(clopts_result_get_error_positional(result));
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_token(result), clopts_parser_result_get_error(result)->token);
    NAOS_CHECK_INT_EQ(clopts_result_get_error_argv_index(result), 3);
    NAOS_CHECK_UINT_EQ(clopts_result_get_error_name_len(result), 2);
    NAOS_CHECK_INT_EQ(clopts_result_get_error_short_name(result), 'q');

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

NAOS_TEST(fresh_result_has_no_error)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    clopts_parser_result_finish(result, CLOPTS_PARSE_USAGE_ERROR);
    CHECK_NO_ERROR(result);
    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

/* ---- sentinel and NULL ------------------------------------------------ */

NAOS_TEST(null_result_and_sentinel_read_like_the_sentinel)
{
    sample_t s;
    clopts_result_t *result;
    clopts_result_t *sentinel;

    SAMPLE_BEGIN(s, result);
    sentinel = clopts_parser_result_oom();
    NAOS_REQUIRE_NOT_NULL(sentinel);
    NAOS_CHECK_PTR_EQ(clopts_parse(NULL, 0, NULL), sentinel);

    NAOS_CHECK_INT_EQ(clopts_result_get_status(NULL), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(sentinel), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_get_command(NULL));
    NAOS_CHECK_NULL(clopts_result_get_command(sentinel));
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(NULL));
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(sentinel));
    CHECK_NO_ERROR(NULL);
    CHECK_NO_ERROR(sentinel);
    CHECK_UNSET(NULL, NULL);
    CHECK_UNSET(sentinel, NULL);
    /* any program's option reads as unset from the sentinel */
    CHECK_UNSET(NULL, s.force);
    CHECK_UNSET(sentinel, s.force);
    CHECK_UNSET(sentinel, s.build_output);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(NULL), 0);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(sentinel), 0);

    clopts_result_destroy(NULL);
    clopts_result_destroy(sentinel);
    clopts_result_destroy(sentinel);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(sentinel), CLOPTS_PARSE_OUT_OF_MEMORY);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
}

/* ---- release ---------------------------------------------------------- */

NAOS_TEST(free_after_the_program_releases_everything)
{
    sample_t s;
    clopts_result_t *result;
    char *copy;
    char token[] = "file";

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    copy = clopts_core_strdup("env");
    NAOS_REQUIRE_NOT_NULL(copy);
    clopts_parser_slot_set_owned_value(slot_of(&s, result, s.config), copy, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(slot_of(&s, result, s.include), "a,b,c", ',',
                                                       CLOPTS_SOURCE_EXPLICIT), 0);
    clopts_parser_slot_set_value(slot_of(&s, result, s.build_output), "o", CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, token), 0);

    clopts_program_destroy(s.program);
    NAOS_CHECK_INT_GT(test_alloc.live, 0);
    clopts_result_destroy(result);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(failing_result_new_leaks_nothing)
{
    sample_t s;
    clopts_result_t *result;
    size_t n;
    long live;

    test_alloc_begin(0);
    sample_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    NAOS_REQUIRE_TRUE(s.ok);
    live = test_alloc.live;
    for (n = 1; n < 10; n++)
    {
        /* The n-th fault point from here on fails. */
        test_alloc_set_fail_at(test_alloc.calls + n);
        result = clopts_parser_result_new(s.program);
        if (result != NULL)
        {
            break;
        }
        NAOS_CHECK_INT_EQ(test_alloc.live, live);
    }
    test_alloc_set_fail_at(0);
    NAOS_REQUIRE_NOT_NULL(result);
    /* the result (calloc) and its slot array (calloc) are two fault points */
    NAOS_CHECK_UINT_EQ(n, 3);
    clopts_result_destroy(result);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(failing_select_leaves_the_result_unchanged)
{
    sample_t s;
    clopts_result_t *result;
    long live;

    SAMPLE_BEGIN(s, result);
    clopts_parser_slot_add_count(slot_of(&s, result, s.verbose), 0);
    live = test_alloc.live;
    /* the slot array of build is the next fault point */
    test_alloc_set_fail_at(test_alloc.calls + 1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_select(result, s.build), -1);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_NULL(test_slots(result, false));
    NAOS_CHECK_UINT_EQ(test_slot_count(result, false), 0);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, s.verbose), 1);

    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(failing_append_list_keeps_a_freeable_result)
{
    sample_t s;
    clopts_result_t *result;
    size_t n;
    int rc;

    for (n = 1; n < 20; n++)
    {
        SAMPLE_BEGIN(s, result);
        test_alloc_set_fail_at(test_alloc.calls + n);
        rc = clopts_parser_slot_append_list(slot_of(&s, result, s.include), "a,b,c", ',',
                                            CLOPTS_SOURCE_EXPLICIT);
        test_alloc_set_fail_at(0);
        if (rc == 0)
        {
            NAOS_CHECK_UINT_EQ(clopts_result_get_list_count(result, s.include), 3);
        }
        else
        {
            NAOS_CHECK_INT_EQ(rc, -1);
            NAOS_CHECK_UINT_LT(slot_of(&s, result, s.include)->items.len, 3);
        }
        clopts_result_destroy(result);
        clopts_program_destroy(s.program);
        NAOS_CHECK_TRUE(test_alloc_end());
        if (rc == 0)
        {
            break;
        }
    }
    /*
     * Three item copies and the item buffer, allocated by the first push
     * (capacity 8; the other two pushes do not grow it): four fault points,
     * so the fifth is not reached.
     */
    NAOS_CHECK_UINT_EQ(n, 5);
}

NAOS_TEST(failing_add_positional_keeps_a_freeable_result)
{
    sample_t s;
    clopts_result_t *result;
    char token[] = "file";
    long live;

    SAMPLE_BEGIN(s, result);
    /* the first push allocates the buffer of the positionals */
    live = test_alloc.live;
    test_alloc_set_fail_at(test_alloc.calls + 1);
    NAOS_CHECK_INT_EQ(clopts_parser_result_add_positional(result, token), -1);
    test_alloc_set_fail_at(0);
    NAOS_CHECK_UINT_EQ(clopts_parser_result_get_positional_count(result), 0);
    NAOS_CHECK_INT_EQ(test_alloc.live, live);
    clopts_result_destroy(result);
    clopts_program_destroy(s.program);
    NAOS_CHECK_TRUE(test_alloc_end());
}

/* ==== through clopts_parse_with_env (7.5) ============================== */

/*
 * The cases below go through the parse function with the shared fixtures of
 * test_fixtures.c and the mock environment fixture_env_t. They are ported
 * from the parse-level cases of the first-version result suite (same argv),
 * with the result-reuse cases mapped to the handle API (independent results,
 * a single release, NULL and the sentinel as no-ops), plus the per-status
 * defaults of the error and value accessors on real parse results. The
 * status is always checked with NAOS_REQUIRE_INT_EQ before values are read.
 */

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Maximum number of argv elements parse_env handles. */
#define MAX_ARGV 16

/*
 * Parses the NULL-terminated argv for program with the mock environment env,
 * checks that the result is a parse result (not the sentinel) and that
 * neither the argv array nor the strings it points to changed.
 */
static clopts_result_t *parse_env(clopts_program_t *program, char **argv, fixture_env_t *env)
{
    char *saved[MAX_ARGV];
    char text[MAX_ARGV][32];
    clopts_result_t *result;
    int argc = 0;
    int i;

    while (argv[argc] != NULL)
    {
        NAOS_REQUIRE_TRUE(argc + 1 < MAX_ARGV);
        NAOS_REQUIRE_TRUE(strlen(argv[argc]) < sizeof(text[0]));
        saved[argc] = argv[argc];
        strcpy(text[argc], argv[argc]);
        argc++;
    }
    result = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, env);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_TRUE(result != clopts_parser_result_oom());
    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(argv[i], saved[i]);
        NAOS_CHECK_STR_EQ(argv[i], text[i]);
    }
    NAOS_CHECK_NULL(argv[argc]);
    return result;
}

#define TOOL_BEGIN(f)                                                               \
    do                                                                              \
    {                                                                               \
        fixture_tool_init(&(f));                                                    \
        NAOS_REQUIRE_NOT_NULL((f).program);                                         \
        NAOS_REQUIRE_TRUE((f).ok);                                                  \
    } while (0)

/* Checks that every option of the tool fixture reads as unset and that there are no positionals. */
static void check_tool_unset(const clopts_result_t *result, const fixture_tool_t *t)
{
    CHECK_UNSET(result, t->verbose);
    CHECK_UNSET(result, t->color);
    CHECK_UNSET(result, t->config);
    CHECK_UNSET(result, t->output);
    CHECK_UNSET(result, t->keys);
    CHECK_UNSET(result, t->jobs);
    CHECK_UNSET(result, t->tags);
    CHECK_UNSET(result, t->help);
    CHECK_UNSET(result, t->version);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 0);
}

/* ---- accessors on a parse result (12.2, 12.4, 8.1 to 8.4) ------------- */

NAOS_TEST(parse_accessors_return_every_kind_and_source)
{
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "-vv", "--no-color", "build", "src", "-o", "out", "-k", "a,b", "-jjj", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    CHECK_NO_ERROR(result);

    /* COUNT and negated FLAG of the global scope. */
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, t.verbose), 2);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.verbose), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_get_flag(result, t.color));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.color), CLOPTS_SOURCE_EXPLICIT);
    /* VALUE from the environment: a copy owned by the result. */
    NAOS_CHECK_STR_EQ(clopts_result_get_value(result, t.config), "/etc/t");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_ENV);

    /* VALUE, LIST and COUNT of the selected command. */
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, t.output), argv[6]);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.output), CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(result, t.keys), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, t.keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(result, t.keys, 1), "b");
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.keys), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, t.jobs), 3);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.jobs), CLOPTS_SOURCE_EXPLICIT);

    /* tags belongs to cat, which was not selected. */
    CHECK_UNSET(result, t.tags);

    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_accessors_report_none_for_options_not_given)
{
    char *argv[] = { "tool", "build", "src", "-o", "out", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    CHECK_UNSET(result, t.verbose);
    CHECK_UNSET(result, t.color);
    CHECK_UNSET(result, t.config);
    CHECK_UNSET(result, t.keys);
    CHECK_UNSET(result, t.jobs);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.output), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_result_has_the_command_and_positionals_from_argv)
{
    char *argv[] = { "tool", "build", "-o", "out", "src", "e1", "--", "-e2", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), t.build);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 3);
    /* Positionals are the argv elements themselves, not copies. */
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, 0), argv[4]);
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, 1), argv[5]);
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, 2), argv[7]);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_result_without_subcommands_has_no_command)
{
    char *argv[] = { "filt", "-n", "3", "a", "b", NULL };
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *result;

    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(f.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, f.lines), argv[2]);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 2);
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, 0), argv[3]);
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, 1), argv[4]);
    clopts_result_destroy(result);
    fixture_filt_free(&f);
}

/* ---- release and independence (4.10, 13.x) ---------------------------- */

NAOS_TEST(parse_out_of_memory_result_is_freed_once)
{
    /*
     * First version: parse_out_of_memory_result_is_freed_twice (a caller
     * struct released twice). Now the OUT_OF_MEMORY result is a handle that
     * is released once; releasing NULL or the sentinel afterwards is a no-op.
     */
    static const fixture_env_var_t vars[] = { { "TOOL_CONFIG", "/etc/t" } };
    char *argv[] = { "tool", "cat", "--tags", "a,b,c", "f", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    /* The result, the cat slots and the list items exist when the lookup fails. */
    test_alloc_begin(0);
    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    env.fail_next = true;
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    clopts_result_destroy(result);
    clopts_result_destroy(NULL);
    clopts_result_destroy(clopts_parser_result_oom());
    fixture_tool_free(&t);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(parse_twice_gives_independent_results)
{
    /*
     * First version: parse_twice_into_the_same_result_does_not_leak (the
     * second parse released the first). Now each parse returns its own
     * result; both stay valid together and each is released on its own.
     */
    static const fixture_env_var_t vars[] = {
        { "TOOL_CONFIG", "/etc/t" },
        { "TOOL_TAGS", "x,y" },
    };
    char *first_argv[] = { "tool", "cat", "--tags", "p,q", "f", NULL };
    char *second_argv[] = { "tool", "build", "src", "-o", "out", "-k", "k1,k2", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *first;
    clopts_result_t *second;

    test_alloc_begin(0);
    fixture_tool_init(&t);
    NAOS_REQUIRE_NOT_NULL(t.program);
    NAOS_REQUIRE_TRUE(t.ok);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    first = parse_env(t.program, first_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(first), CLOPTS_PARSE_OK);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    second = parse_env(t.program, second_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(second), CLOPTS_PARSE_OK);
    NAOS_CHECK_TRUE(first != second);

    NAOS_CHECK_PTR_EQ(clopts_result_get_command(first), t.cat);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(first, t.tags), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(first, t.tags, 0), "p");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(first, t.tags, 1), "q");
    NAOS_CHECK_STR_EQ(clopts_result_get_value(first, t.config), "/etc/t");
    CHECK_UNSET(first, t.keys);

    NAOS_CHECK_PTR_EQ(clopts_result_get_command(second), t.build);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(second, t.config), "/etc/t");
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_list_count(second, t.keys), 2);
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(second, t.keys, 1), "k2");
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(second), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(second, 0), second_argv[2]);
    CHECK_UNSET(second, t.tags);

    /* Releasing the first leaves the second intact. */
    clopts_result_destroy(first);
    NAOS_CHECK_STR_EQ(clopts_result_get_value(second, t.config), "/etc/t");
    NAOS_CHECK_STR_EQ(clopts_result_get_list_at(second, t.keys, 0), "k1");
    clopts_result_destroy(second);
    fixture_tool_free(&t);
    NAOS_CHECK_TRUE(test_alloc_end());
}

NAOS_TEST(parse_after_an_error_gives_an_independent_result)
{
    /*
     * First version: parse_after_an_error_result_is_reusable (the same
     * struct reused). Now the error result keeps its error and the next
     * parse starts from a fresh result without it.
     */
    char *bad[] = { "tool", "build", "--bad", NULL };
    char *good[] = { "filt", "a", NULL };
    fixture_tool_t t;
    fixture_filt_t f;
    fixture_env_t env;
    clopts_result_t *error_result;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    fixture_env_init(&env, NULL, 0);
    error_result = parse_env(t.program, bad, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(error_result), CLOPTS_PARSE_USAGE_ERROR);
    result = parse_env(f.program, good, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    CHECK_NO_ERROR(result);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 1);

    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(error_result), CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_token(error_result), bad[2]);
    NAOS_CHECK_INT_EQ(clopts_result_get_error_argv_index(error_result), 2);

    clopts_result_destroy(error_result);
    clopts_result_destroy(result);
    fixture_filt_free(&f);
    fixture_tool_free(&t);
}

/* ---- per-status defaults on parse results (11.7, 12.7) ---------------- */

NAOS_TEST(parse_help_result_has_no_error_and_the_values_before_it)
{
    char *argv[] = { "tool", "-vv", "--config", "c", "build", "-o", "out", "src", "--help", "-k", "a", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), t.help);
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), t.build);
    CHECK_NO_ERROR(result);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, t.help));
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, t.verbose), 2);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, t.config), argv[3]);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, t.output), argv[6]);
    NAOS_REQUIRE_UINT_EQ(clopts_result_get_positional_count(result), 1);
    NAOS_CHECK_PTR_EQ(clopts_result_get_positional_at(result, 0), argv[7]);
    /* -k after --help was not read. */
    CHECK_UNSET(result, t.keys);
    CHECK_UNSET(result, t.jobs);
    CHECK_UNSET(result, t.color);
    CHECK_UNSET(result, t.version);
    CHECK_UNSET(result, t.tags);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_version_result_has_no_error_and_nothing_after_it)
{
    char *argv[] = { "tool", "--version", "-vv", "--config", "c", "build", "-o", "out", "src", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_TERMINATED);
    NAOS_CHECK_PTR_EQ(clopts_result_get_terminal_option(result), t.version);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    CHECK_NO_ERROR(result);
    NAOS_CHECK_TRUE(clopts_result_get_flag(result, t.version));
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.version), CLOPTS_SOURCE_EXPLICIT);
    CHECK_UNSET(result, t.verbose);
    CHECK_UNSET(result, t.config);
    CHECK_UNSET(result, t.help);
    /* build was not selected: its options read as unset. */
    CHECK_UNSET(result, t.output);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 0);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

/*
 * clopts_result_get_terminal_option() is NULL for every status but TERMINATED, also
 * when a terminal option was given but the parse did not end with it.
 */
NAOS_TEST(parse_terminal_is_null_unless_terminated)
{
    char *ok_argv[] = { "tool", "build", "-o", "x", "src", NULL };
    char *no_command_argv[] = { "tool", "-v", NULL };
    char *error_argv[] = { "tool", "--help=x", NULL };
    char *oom_argv[] = { "tool", "--help", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, ok_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    clopts_result_destroy(result);

    result = parse_env(t.program, no_command_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    clopts_result_destroy(result);

    result = parse_env(t.program, error_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    clopts_result_destroy(result);

    /* --help was given, but the lookup of TOOL_CONFIG fails before TERMINATED. */
    env.fail_next = true;
    result = parse_env(t.program, oom_argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(result));
    CHECK_UNSET(result, t.help);
    clopts_result_destroy(result);

    NAOS_CHECK_NULL(clopts_result_get_terminal_option(clopts_parser_result_oom()));
    NAOS_CHECK_NULL(clopts_result_get_terminal_option(NULL));
    fixture_tool_free(&t);
}

NAOS_TEST(parse_usage_error_result_has_unset_values)
{
    char *argv[] = { "tool", "-vv", "--config", "c", "build", "-o", "out", "src", "--bad", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_USAGE_ERROR);
    NAOS_CHECK_INT_EQ(clopts_result_get_usage_error(result), CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_command(result), t.build);
    NAOS_CHECK_NULL(clopts_result_get_error_option(result));
    NAOS_CHECK_NULL(clopts_result_get_error_positional(result));
    NAOS_CHECK_PTR_EQ(clopts_result_get_error_token(result), argv[8]);
    NAOS_CHECK_INT_EQ(clopts_result_get_error_argv_index(result), 8);
    NAOS_CHECK_UINT_EQ(clopts_result_get_error_name_len(result), 5);
    NAOS_CHECK_INT_EQ(clopts_result_get_error_short_name(result), '\0');
    NAOS_CHECK_PTR_EQ(clopts_result_get_command(result), t.build);
    check_tool_unset(result, &t);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_env_failure_result_has_no_error_and_unset_values)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x" } };
    char *argv[] = { "tool", "-vv", "cat", "--tags", "p,q", "f", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    env.fail_next = true;
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OUT_OF_MEMORY);
    CHECK_NO_ERROR(result);
    check_tool_unset(result, &t);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_TEST(parse_no_command_result_has_no_error_and_only_global_values)
{
    char *argv[] = { "tool", "-vv", "--config", "c", NULL };
    fixture_tool_t t;
    fixture_env_t env;
    clopts_result_t *result;

    TOOL_BEGIN(t);
    fixture_env_init(&env, NULL, 0);
    result = parse_env(t.program, argv, &env);
    NAOS_REQUIRE_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_NO_COMMAND);
    NAOS_CHECK_NULL(clopts_result_get_command(result));
    CHECK_NO_ERROR(result);
    NAOS_CHECK_UINT_EQ(clopts_result_get_count(result, t.verbose), 2);
    NAOS_CHECK_PTR_EQ(clopts_result_get_value(result, t.config), argv[3]);
    NAOS_CHECK_INT_EQ(clopts_result_get_source(result, t.config), CLOPTS_SOURCE_EXPLICIT);
    CHECK_UNSET(result, t.color);
    CHECK_UNSET(result, t.output);
    CHECK_UNSET(result, t.keys);
    CHECK_UNSET(result, t.jobs);
    CHECK_UNSET(result, t.tags);
    NAOS_CHECK_UINT_EQ(clopts_result_get_positional_count(result), 0);
    clopts_result_destroy(result);
    fixture_tool_free(&t);
}

NAOS_SUITE(result,
    NAOS_ADD_TEST(new_allocates_one_slot_per_global_option),
    NAOS_ADD_TEST(new_without_global_options_has_no_slots),
    NAOS_ADD_TEST(select_allocates_command_slots),
    NAOS_ADD_TEST(select_command_without_options),
    NAOS_ADD_TEST(result_slot_returns_the_slot_of_the_ref),
    NAOS_ADD_TEST(flag_last_one_wins_including_negation),
    NAOS_ADD_TEST(count_clamps_at_its_max),
    NAOS_ADD_TEST(count_default_max_is_uint_max),
    NAOS_ADD_TEST(value_last_one_wins),
    NAOS_ADD_TEST(list_concatenates_split_items),
    NAOS_ADD_TEST(list_keeps_empty_items),
    NAOS_ADD_TEST(list_without_delimiter_takes_the_value_as_one_item),
    NAOS_ADD_TEST(source_is_overwritten_by_the_last_record),
    NAOS_ADD_TEST(environment_copy_is_owned_by_the_slot),
    NAOS_ADD_TEST(positionals_point_into_argv_in_order),
    NAOS_ADD_TEST(accessors_return_the_slot_of_the_handle),
    NAOS_ADD_TEST(command_options_are_unset_without_a_selected_command),
    NAOS_ADD_TEST(null_option_reads_as_unset),
    NAOS_ADD_TEST(options_keep_their_attach_position_in_their_command),
    NAOS_ADD_TEST(option_at_the_same_position_elsewhere_reads_as_unset),
    NAOS_ADD_TEST(option_beyond_the_slots_reads_as_unset),
    NAOS_ADD_TEST(value_accessors_are_unset_unless_ok_terminated_or_no_command),
    NAOS_ADD_TEST(no_command_keeps_only_the_global_values),
    NAOS_ADD_TEST(terminated_keeps_the_global_and_command_values),
    NAOS_ADD_TEST(error_accessors_report_only_errors),
    NAOS_ADD_TEST(fresh_result_has_no_error),
    NAOS_ADD_TEST(null_result_and_sentinel_read_like_the_sentinel),
    NAOS_ADD_TEST(free_after_the_program_releases_everything),
    NAOS_ADD_TEST(failing_result_new_leaks_nothing),
    NAOS_ADD_TEST(failing_select_leaves_the_result_unchanged),
    NAOS_ADD_TEST(failing_append_list_keeps_a_freeable_result),
    NAOS_ADD_TEST(failing_add_positional_keeps_a_freeable_result),
    NAOS_ADD_TEST(parse_accessors_return_every_kind_and_source),
    NAOS_ADD_TEST(parse_accessors_report_none_for_options_not_given),
    NAOS_ADD_TEST(parse_result_has_the_command_and_positionals_from_argv),
    NAOS_ADD_TEST(parse_result_without_subcommands_has_no_command),
    NAOS_ADD_TEST(parse_out_of_memory_result_is_freed_once),
    NAOS_ADD_TEST(parse_twice_gives_independent_results),
    NAOS_ADD_TEST(parse_after_an_error_gives_an_independent_result),
    NAOS_ADD_TEST(parse_help_result_has_no_error_and_the_values_before_it),
    NAOS_ADD_TEST(parse_version_result_has_no_error_and_nothing_after_it),
    NAOS_ADD_TEST(parse_terminal_is_null_unless_terminated),
    NAOS_ADD_TEST(parse_usage_error_result_has_unset_values),
    NAOS_ADD_TEST(parse_env_failure_result_has_no_error_and_unset_values),
    NAOS_ADD_TEST(parse_no_command_result_has_no_error_and_only_global_values)
)
