/*
 * Suite result: the result handle (recording rules, accessors, run and
 * release), driven through the internal API.
 *
 * A program is built with the builder; a result is created with
 * clopts_parser_result_new(), its fields (status, parsed, error) are set
 * directly and values are recorded with the slot functions. The parse
 * functions are not used here.
 */

#include "parser/parser_internal.h"

#include <naos/naos.h>

#include <limits.h>
#include <string.h>

/* ---- fixture ---------------------------------------------------------- */

/*
 * tool
 *   root:  --force/-f (FLAG, negatable), --verbose/-v (COUNT),
 *          --jobs/-j (COUNT, max 8), --config/-c (VALUE),
 *          --include/-I (LIST, ','), --tag/-t (LIST, no delimiter)
 *   build: --release (FLAG), --output/-o (VALUE); handler
 *   copy:  --output (VALUE); no handler
 *   empty: no options
 */
typedef struct sample
{
    clopts_parser_probe_t probe;
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_option_t *force;
    clopts_option_t *verbose;
    clopts_option_t *jobs;
    clopts_option_t *config;
    clopts_option_t *include;
    clopts_option_t *tag;
    clopts_command_t *build;
    clopts_option_t *release;
    clopts_option_t *build_output;
    clopts_command_t *copy;
    clopts_option_t *copy_output;
    clopts_command_t *empty;
} sample_t;

/* What a handler saw. */
typedef struct handler_record
{
    int calls;
    const clopts_result_t *result;
    void *userdata;
} handler_record_t;

static handler_record_t handler_record;

static int record_handler(const clopts_result_t *result, void *userdata)
{
    handler_record.calls++;
    handler_record.result = result;
    handler_record.userdata = userdata;
    return 42;
}

static int other_handler(const clopts_result_t *result, void *userdata)
{
    handler_record.calls++;
    handler_record.result = result;
    handler_record.userdata = userdata;
    return 7;
}

static int build_userdata;

/* Builds the sample program with a probe that never fails (counts only). */
static void sample_init(sample_t *s)
{
    s->probe.fail_at = 0;
    s->probe.calls = 0;
    s->probe.live = 0;
    s->program = clopts_program_new("tool");
    if (s->program == NULL)
    {
        return;
    }
    clopts_parser_set_probe(s->program, &s->probe);
    s->root = clopts_program_root(s->program);
    s->force = clopts_command_add_flag(s->root, "force", 'f');
    clopts_option_set_negatable(s->force, true);
    s->verbose = clopts_command_add_count(s->root, "verbose", 'v');
    s->jobs = clopts_command_add_count(s->root, "jobs", 'j');
    clopts_option_set_count_max(s->jobs, 8);
    s->config = clopts_command_add_value(s->root, "config", 'c');
    s->include = clopts_command_add_list(s->root, "include", 'I');
    clopts_option_set_delimiter(s->include, ',');
    s->tag = clopts_command_add_list(s->root, "tag", 't');
    s->build = clopts_program_add_command(s->program, "build");
    s->release = clopts_command_add_flag(s->build, "release", '\0');
    s->build_output = clopts_command_add_value(s->build, "output", 'o');
    clopts_command_set_handler(s->build, record_handler, &build_userdata);
    s->copy = clopts_program_add_command(s->program, "copy");
    s->copy_output = clopts_command_add_value(s->copy, "output", '\0');
    s->empty = clopts_program_add_command(s->program, "empty");
}

/* Returns the slot of option (in the root or the selected command). */
static clopts_parser_slot_t *slot_of(clopts_result_t *result, const clopts_option_t *option)
{
    clopts_parser_ref_t ref;

    ref.option = option;
    ref.global = (option->scope == &option->program->root);
    ref.index = option->index;
    ref.negated = false;
    return clopts_parser_result_slot(result, &ref);
}

#define SAMPLE_BEGIN(s, result)                                                     \
    do                                                                              \
    {                                                                               \
        sample_init(&(s));                                                          \
        NAOS_REQUIRE_NOT_NULL((s).program);                                         \
        NAOS_REQUIRE_INT_EQ((s).program->error.status, CLOPTS_OK);                  \
        (result) = clopts_parser_result_new((s).program);                           \
        NAOS_REQUIRE_NOT_NULL(result);                                              \
    } while (0)

/* Checks that every value accessor returns the unset value for option. */
#define CHECK_UNSET(result, option)                                                 \
    do                                                                              \
    {                                                                               \
        NAOS_CHECK_FALSE(clopts_result_flag((result), (option)));                   \
        NAOS_CHECK_UINT_EQ(clopts_result_count((result), (option)), 0);             \
        NAOS_CHECK_NULL(clopts_result_value((result), (option)));                   \
        NAOS_CHECK_UINT_EQ(clopts_result_list_count((result), (option)), 0);        \
        NAOS_CHECK_INT_EQ(clopts_result_source((result), (option)), CLOPTS_SOURCE_NONE); \
    } while (0)

/* Checks that every error accessor returns "no error". */
#define CHECK_NO_ERROR(result)                                                      \
    do                                                                              \
    {                                                                               \
        NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_NONE);    \
        NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_NONE);      \
        NAOS_CHECK_NULL(clopts_result_error_command(result));                       \
        NAOS_CHECK_NULL(clopts_result_error_option(result));                        \
        NAOS_CHECK_NULL(clopts_result_error_positional(result));                    \
        NAOS_CHECK_NULL(clopts_result_error_token(result));                         \
        NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), -1);              \
        NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 0);                \
        NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), '\0');            \
    } while (0)

/* ---- allocation ------------------------------------------------------- */

NAOS_TEST(new_allocates_one_slot_per_root_option)
{
    sample_t s;
    clopts_result_t *result;
    size_t i;

    SAMPLE_BEGIN(s, result);
    NAOS_CHECK_INT_EQ(result->status, CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result->program, s.program);
    NAOS_CHECK_PTR_EQ(result->probe, &s.probe);
    NAOS_CHECK_FALSE(result->parsed);
    NAOS_CHECK_NULL(result->command);
    NAOS_REQUIRE_UINT_EQ(result->global_count, 6);
    NAOS_REQUIRE_NOT_NULL(result->global_slots);
    NAOS_CHECK_NULL(result->command_slots);
    NAOS_CHECK_UINT_EQ(result->command_count, 0);
    for (i = 0; i < result->global_count; i++)
    {
        NAOS_CHECK_INT_EQ(result->global_slots[i].source, CLOPTS_SOURCE_NONE);
        NAOS_CHECK_NULL(result->global_slots[i].value);
        NAOS_CHECK_NULL(result->global_slots[i].owned);
        NAOS_CHECK_UINT_EQ(result->global_slots[i].items.len, 0);
        NAOS_CHECK_UINT_EQ(result->global_slots[i].items.elem_size, sizeof(char *));
    }

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(new_without_root_options_has_no_slots)
{
    clopts_program_t *program;
    clopts_result_t *result;

    program = clopts_program_new("tool");
    NAOS_REQUIRE_NOT_NULL(program);
    result = clopts_parser_result_new(program);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_CHECK_NULL(result->global_slots);
    NAOS_CHECK_UINT_EQ(result->global_count, 0);
    clopts_result_free(result);
    clopts_program_free(program);
}

NAOS_TEST(select_allocates_command_slots)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    NAOS_CHECK_PTR_EQ(result->command, s.build);
    NAOS_CHECK_PTR_EQ(clopts_result_command(result), s.build);
    NAOS_REQUIRE_UINT_EQ(result->command_count, 2);
    NAOS_REQUIRE_NOT_NULL(result->command_slots);
    NAOS_CHECK_INT_EQ(result->command_slots[1].source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(result->command_slots[1].items.elem_size, sizeof(char *));

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(select_command_without_options)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.empty), 0);
    NAOS_CHECK_PTR_EQ(result->command, s.empty);
    NAOS_CHECK_NULL(result->command_slots);
    NAOS_CHECK_UINT_EQ(result->command_count, 0);

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(result_slot_returns_the_slot_of_the_ref)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    NAOS_CHECK_PTR_EQ(slot_of(result, s.force), &result->global_slots[0]);
    NAOS_CHECK_PTR_EQ(slot_of(result, s.tag), &result->global_slots[5]);
    NAOS_CHECK_PTR_EQ(slot_of(result, s.release), &result->command_slots[0]);
    NAOS_CHECK_PTR_EQ(slot_of(result, s.build_output), &result->command_slots[1]);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

/* ---- recording rules -------------------------------------------------- */

NAOS_TEST(flag_last_one_wins_including_negation)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.force);
    clopts_parser_slot_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_flag(result, s.force));
    clopts_parser_slot_set_flag(slot, false, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(clopts_result_flag(result, s.force));
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.force), CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_flag(result, s.force));

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(count_clamps_at_its_max)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    int i;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.jobs);
    for (i = 0; i < 12; i++)
    {
        clopts_parser_slot_add_count(slot, s.jobs->count_max);
    }
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.jobs), 8);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.jobs), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(count_default_max_is_uint_max)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    int i;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.verbose);
    NAOS_CHECK_UINT_EQ(s.verbose->count_max, 0);
    for (i = 0; i < 12; i++)
    {
        clopts_parser_slot_add_count(slot, s.verbose->count_max);
    }
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.verbose), 12);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.verbose), CLOPTS_SOURCE_EXPLICIT);

    slot->count = UINT_MAX - 1u;
    clopts_parser_slot_add_count(slot, 0);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.verbose), UINT_MAX);
    clopts_parser_slot_add_count(slot, 0);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.verbose), UINT_MAX);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(value_last_one_wins)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    char first[] = "a.conf";
    char second[] = "b.conf";

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.config);
    clopts_parser_slot_set_value(result->probe, slot, first, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, s.config), first);
    clopts_parser_slot_set_value(result->probe, slot, second, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, s.config), second);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.config), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(list_concatenates_split_items)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;
    char first[] = "a,b";
    char second[] = "c";

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.include);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, first, ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, second, ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, s.include), 3);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 1), "b");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 2), "c");
    /* items are copies owned by the result */
    NAOS_CHECK_TRUE(clopts_result_list_at(result, s.include, 2) != second);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.include), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(list_keeps_empty_items)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.include);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, "a,,b,", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, s.include), 4);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 1), "");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 2), "b");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 3), "");

    /* an empty value is one empty item */
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, "", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, s.include), 5);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 4), "");

    /* a lone delimiter gives two empty items */
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, ",", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, s.include), 7);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 5), "");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 6), "");

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(list_without_delimiter_takes_the_value_as_one_item)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.tag);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, "a,b", '\0', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, "", '\0', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, s.tag), 2);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.tag, 0), "a,b");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.tag, 1), "");

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(source_is_overwritten_by_the_last_record)
{
    sample_t s;
    clopts_result_t *result;
    clopts_parser_slot_t *slot;

    SAMPLE_BEGIN(s, result);
    slot = slot_of(result, s.config);
    clopts_parser_slot_set_value(result->probe, slot, "default", CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.config), CLOPTS_SOURCE_DEFAULT);
    clopts_parser_slot_set_value(result->probe, slot, "env", CLOPTS_SOURCE_ENV);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.config), CLOPTS_SOURCE_ENV);
    clopts_parser_slot_set_value(result->probe, slot, "argv", CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.config), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, s.config), "argv");

    slot = slot_of(result, s.include);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, "x", ',', CLOPTS_SOURCE_DEFAULT), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.include), CLOPTS_SOURCE_DEFAULT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot, "y", ',', CLOPTS_SOURCE_ENV), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.include), CLOPTS_SOURCE_ENV);

    slot = slot_of(result, s.force);
    clopts_parser_slot_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.force), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
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
    slot = slot_of(result, s.config);
    before = s.probe.live;
    copy = clopts_parser_strdup(result->probe, "from-env");
    NAOS_REQUIRE_NOT_NULL(copy);
    clopts_parser_slot_own(result->probe, slot, copy);
    clopts_parser_slot_set_value(result->probe, slot, copy, CLOPTS_SOURCE_ENV);
    NAOS_CHECK_PTR_EQ(slot->owned, copy);
    NAOS_CHECK_PTR_EQ(clopts_result_value(result, s.config), copy);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.config), CLOPTS_SOURCE_ENV);
    NAOS_CHECK_INT_EQ(s.probe.live, before + 1);

    /* owning a new copy releases the previous one */
    second = clopts_parser_strdup(result->probe, "again");
    NAOS_REQUIRE_NOT_NULL(second);
    clopts_parser_slot_own(result->probe, slot, second);
    NAOS_CHECK_PTR_EQ(slot->owned, second);
    NAOS_CHECK_PTR_EQ(slot->value, second);
    NAOS_CHECK_INT_EQ(s.probe.live, before + 1);

    /* a borrowed value replaces it and the copy is released (value == owned) */
    clopts_parser_slot_set_value(result->probe, slot, "argv", CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_NULL(slot->owned);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, s.config), "argv");
    NAOS_CHECK_INT_EQ(s.probe.live, before);

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
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
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 30);
    for (i = 0; i < 30; i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_result_positional_at(result, i), argv[i % 3]);
    }

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

/* ---- accessors by handle ---------------------------------------------- */

NAOS_TEST(accessors_return_the_slot_of_the_handle)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    clopts_parser_slot_set_flag(slot_of(result, s.force), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_add_count(slot_of(result, s.verbose), 0);
    clopts_parser_slot_add_count(slot_of(result, s.verbose), 0);
    clopts_parser_slot_add_count(slot_of(result, s.jobs), 8);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.config), "c.conf", CLOPTS_SOURCE_DEFAULT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot_of(result, s.include), "i", ',',
                                                       CLOPTS_SOURCE_ENV), 0);
    clopts_parser_slot_set_flag(slot_of(result, s.release), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.build_output), "out", CLOPTS_SOURCE_EXPLICIT);

    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(result, s.force));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.verbose), 2);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.jobs), 1);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, s.config), "c.conf");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.config), CLOPTS_SOURCE_DEFAULT);
    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, s.include), 1);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(result, s.include, 0), "i");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.include), CLOPTS_SOURCE_ENV);
    CHECK_UNSET(result, s.tag);
    NAOS_CHECK_TRUE(clopts_result_flag(result, s.release));
    NAOS_CHECK_STR_EQ(clopts_result_value(result, s.build_output), "out");
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.build_output), CLOPTS_SOURCE_EXPLICIT);

    /* an option of a command that was not selected (same long name) is unset */
    CHECK_UNSET(result, s.copy_output);

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(command_options_are_unset_without_a_selected_command)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    NAOS_CHECK_NULL(clopts_result_command(result));
    CHECK_UNSET(result, s.release);
    CHECK_UNSET(result, s.build_output);
    CHECK_UNSET(result, s.force);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(null_option_reads_as_unset)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    clopts_parser_slot_set_flag(slot_of(result, s.force), true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    CHECK_UNSET(result, NULL);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(option_of_another_program_reads_as_unset_with_ndebug)
{
#ifdef NDEBUG
    sample_t s;
    sample_t other;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    sample_init(&other);
    NAOS_REQUIRE_NOT_NULL(other.program);
    clopts_parser_slot_set_flag(slot_of(result, s.force), true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    CHECK_UNSET(result, other.force);

    clopts_result_free(result);
    clopts_program_free(other.program);
    clopts_program_free(s.program);
#else
    NAOS_SKIP("needs NDEBUG: an option of another program is caught by assert");
#endif
}

/* ---- per-status defaults ---------------------------------------------- */

/* Records a value in every root option of s and in build's options, and adds a positional. */
static void record_everything(sample_t *s, clopts_result_t *result)
{
    static char token[] = "file";

    (void)clopts_parser_result_select(result, s->build);
    clopts_parser_slot_set_flag(slot_of(result, s->force), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_add_count(slot_of(result, s->verbose), 0);
    clopts_parser_slot_add_count(slot_of(result, s->jobs), 8);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s->config), "c", CLOPTS_SOURCE_EXPLICIT);
    (void)clopts_parser_slot_append_list(result->probe, slot_of(result, s->include), "a,b", ',',
                                         CLOPTS_SOURCE_EXPLICIT);
    (void)clopts_parser_slot_append_list(result->probe, slot_of(result, s->tag), "t", '\0',
                                         CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_flag(slot_of(result, s->release), true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s->build_output), "o", CLOPTS_SOURCE_EXPLICIT);
    (void)clopts_parser_result_add_positional(result, token);
}

NAOS_TEST(value_accessors_are_unset_unless_ok_or_no_command)
{
    static const clopts_status_t statuses[] = {
        CLOPTS_HELP, CLOPTS_VERSION, CLOPTS_USAGE_ERROR, CLOPTS_DECL_ERROR, CLOPTS_OUT_OF_MEMORY
    };
    sample_t s;
    clopts_result_t *result;
    size_t i;

    SAMPLE_BEGIN(s, result);
    record_everything(&s, result);
    NAOS_REQUIRE_PTR_EQ(result->command, s.build);
    NAOS_REQUIRE_TRUE(clopts_result_flag(result, s.force));
    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(result), 1);

    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++)
    {
        result->status = statuses[i];
        NAOS_REQUIRE_INT_EQ(clopts_result_status(result), statuses[i]);
        CHECK_UNSET(result, s.force);
        CHECK_UNSET(result, s.verbose);
        CHECK_UNSET(result, s.jobs);
        CHECK_UNSET(result, s.config);
        CHECK_UNSET(result, s.include);
        CHECK_UNSET(result, s.tag);
        CHECK_UNSET(result, s.release);
        CHECK_UNSET(result, s.build_output);
        NAOS_CHECK_UINT_EQ(clopts_result_positional_count(result), 0);
    }

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(no_command_keeps_only_the_root_values)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    record_everything(&s, result);
    result->status = CLOPTS_NO_COMMAND;
    NAOS_REQUIRE_INT_EQ(clopts_result_status(result), CLOPTS_NO_COMMAND);
    NAOS_CHECK_TRUE(clopts_result_flag(result, s.force));
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.verbose), 1);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.jobs), 1);
    NAOS_CHECK_STR_EQ(clopts_result_value(result, s.config), "c");
    NAOS_CHECK_UINT_EQ(clopts_result_list_count(result, s.include), 2);
    NAOS_CHECK_INT_EQ(clopts_result_source(result, s.tag), CLOPTS_SOURCE_EXPLICIT);
    CHECK_UNSET(result, s.release);
    CHECK_UNSET(result, s.build_output);

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

/* Fills every error field of result. */
static void fill_error(sample_t *s, clopts_result_t *result)
{
    static char token[] = "-xq";

    result->error.usage = CLOPTS_USAGE_UNKNOWN_OPTION;
    result->error.decl = CLOPTS_DECL_DUPLICATE_OPTION;
    result->error.command = s->build;
    result->error.option = s->release;
    result->error.positional = NULL;
    result->error.token = token;
    result->error.argv_index = 3;
    result->error.name_len = 2;
    result->error.short_name = 'q';
}

NAOS_TEST(error_accessors_report_only_errors)
{
    static const clopts_status_t statuses[] = {
        CLOPTS_OK, CLOPTS_HELP, CLOPTS_VERSION, CLOPTS_NO_COMMAND, CLOPTS_OUT_OF_MEMORY
    };
    sample_t s;
    clopts_result_t *result;
    size_t i;

    SAMPLE_BEGIN(s, result);
    fill_error(&s, result);
    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++)
    {
        result->status = statuses[i];
        CHECK_NO_ERROR(result);
    }

    result->status = CLOPTS_USAGE_ERROR;
    NAOS_CHECK_INT_EQ(clopts_result_usage_error(result), CLOPTS_USAGE_UNKNOWN_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_command(result), s.build);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), s.release);
    NAOS_CHECK_NULL(clopts_result_error_positional(result));
    NAOS_CHECK_PTR_EQ(clopts_result_error_token(result), result->error.token);
    NAOS_CHECK_INT_EQ(clopts_result_error_argv_index(result), 3);
    NAOS_CHECK_UINT_EQ(clopts_result_error_name_len(result), 2);
    NAOS_CHECK_INT_EQ(clopts_result_error_short_name(result), 'q');

    result->status = CLOPTS_DECL_ERROR;
    NAOS_CHECK_INT_EQ(clopts_result_decl_error(result), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_PTR_EQ(clopts_result_error_option(result), s.release);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(fresh_result_has_no_error)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    result->status = CLOPTS_USAGE_ERROR;
    CHECK_NO_ERROR(result);
    clopts_result_free(result);
    clopts_program_free(s.program);
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

    NAOS_CHECK_INT_EQ(clopts_result_status(NULL), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_result_status(sentinel), CLOPTS_OUT_OF_MEMORY);
    NAOS_CHECK_NULL(clopts_result_command(NULL));
    NAOS_CHECK_NULL(clopts_result_command(sentinel));
    CHECK_NO_ERROR(NULL);
    CHECK_NO_ERROR(sentinel);
    CHECK_UNSET(NULL, NULL);
    CHECK_UNSET(sentinel, NULL);
    /* any program's option reads as unset from the sentinel */
    CHECK_UNSET(NULL, s.force);
    CHECK_UNSET(sentinel, s.force);
    CHECK_UNSET(sentinel, s.build_output);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(NULL), 0);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(sentinel), 0);

    clopts_result_free(NULL);
    clopts_result_free(sentinel);
    clopts_result_free(sentinel);
    NAOS_CHECK_INT_EQ(clopts_result_status(sentinel), CLOPTS_OUT_OF_MEMORY);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

/* ---- run -------------------------------------------------------------- */

NAOS_TEST(run_calls_the_selected_command_handler)
{
    sample_t s;
    clopts_result_t *result;

    SAMPLE_BEGIN(s, result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_select(result, s.build), 0);
    result->parsed = true;
    result->status = CLOPTS_OK;
    memset(&handler_record, 0, sizeof(handler_record));

    NAOS_CHECK_INT_EQ(clopts_result_run(result), 42);
    NAOS_CHECK_INT_EQ(handler_record.calls, 1);
    NAOS_CHECK_PTR_EQ(handler_record.result, result);
    NAOS_CHECK_PTR_EQ(handler_record.userdata, &build_userdata);

    clopts_result_free(result);
    clopts_program_free(s.program);
}

NAOS_TEST(run_calls_the_root_handler_without_subcommands)
{
    clopts_program_t *program;
    clopts_result_t *result;
    int userdata;

    program = clopts_program_new("filt");
    NAOS_REQUIRE_NOT_NULL(program);
    (void)clopts_command_add_flag(clopts_program_root(program), "all", 'a');
    clopts_command_set_handler(clopts_program_root(program), other_handler, &userdata);
    result = clopts_parser_result_new(program);
    NAOS_REQUIRE_NOT_NULL(result);
    result->parsed = true;
    result->status = CLOPTS_OK;
    memset(&handler_record, 0, sizeof(handler_record));

    NAOS_CHECK_INT_EQ(clopts_result_run(result), 7);
    NAOS_CHECK_INT_EQ(handler_record.calls, 1);
    NAOS_CHECK_PTR_EQ(handler_record.result, result);
    NAOS_CHECK_PTR_EQ(handler_record.userdata, &userdata);

    clopts_result_free(result);
    clopts_program_free(program);
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
    copy = clopts_parser_strdup(result->probe, "env");
    NAOS_REQUIRE_NOT_NULL(copy);
    clopts_parser_slot_own(result->probe, slot_of(result, s.config), copy);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.config), copy, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_INT_EQ(clopts_parser_slot_append_list(result->probe, slot_of(result, s.include), "a,b,c", ',',
                                                       CLOPTS_SOURCE_EXPLICIT), 0);
    clopts_parser_slot_set_value(result->probe, slot_of(result, s.build_output), "o", CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(clopts_parser_result_add_positional(result, token), 0);

    clopts_program_free(s.program);
    NAOS_CHECK_INT_GT(s.probe.live, 0);
    clopts_result_free(result);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(failing_result_new_leaks_nothing)
{
    sample_t s;
    clopts_result_t *result;
    size_t n;
    size_t base;
    long live;

    sample_init(&s);
    NAOS_REQUIRE_NOT_NULL(s.program);
    base = s.probe.calls;
    live = s.probe.live;
    for (n = 1; n < 10; n++)
    {
        s.probe.fail_at = base + n;
        result = clopts_parser_result_new(s.program);
        if (result != NULL)
        {
            break;
        }
        NAOS_CHECK_INT_EQ(s.probe.live, live);
        s.probe.calls = base;
    }
    NAOS_REQUIRE_NOT_NULL(result);
    /* the result and its slot array are two fault points */
    NAOS_CHECK_UINT_EQ(n, 3);
    clopts_result_free(result);
    NAOS_CHECK_INT_EQ(s.probe.live, live);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_TEST(failing_select_leaves_the_result_unchanged)
{
    sample_t s;
    clopts_result_t *result;
    long live;

    SAMPLE_BEGIN(s, result);
    clopts_parser_slot_add_count(slot_of(result, s.verbose), 0);
    live = s.probe.live;
    s.probe.fail_at = s.probe.calls + 1;
    NAOS_CHECK_INT_EQ(clopts_parser_result_select(result, s.build), -1);
    NAOS_CHECK_NULL(result->command);
    NAOS_CHECK_NULL(result->command_slots);
    NAOS_CHECK_UINT_EQ(result->command_count, 0);
    NAOS_CHECK_INT_EQ(s.probe.live, live);
    NAOS_CHECK_UINT_EQ(clopts_result_count(result, s.verbose), 1);

    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
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
        s.probe.fail_at = s.probe.calls + n;
        rc = clopts_parser_slot_append_list(result->probe, slot_of(result, s.include), "a,b,c", ',',
                                            CLOPTS_SOURCE_EXPLICIT);
        if (rc == 0)
        {
            NAOS_CHECK_UINT_EQ(clopts_result_list_count(result, s.include), 3);
        }
        else
        {
            NAOS_CHECK_INT_EQ(rc, -1);
            NAOS_CHECK_UINT_LT(slot_of(result, s.include)->items.len, 3);
        }
        clopts_result_free(result);
        clopts_program_free(s.program);
        NAOS_CHECK_INT_EQ(s.probe.live, 0);
        if (rc == 0)
        {
            break;
        }
    }
    /* three copies and three pushes: the seventh fault point is not reached */
    NAOS_CHECK_UINT_EQ(n, 7);
}

NAOS_TEST(failing_add_positional_keeps_a_freeable_result)
{
    sample_t s;
    clopts_result_t *result;
    char token[] = "file";

    SAMPLE_BEGIN(s, result);
    s.probe.fail_at = s.probe.calls + 1;
    NAOS_CHECK_INT_EQ(clopts_parser_result_add_positional(result, token), -1);
    NAOS_CHECK_UINT_EQ(result->positionals.len, 0);
    clopts_result_free(result);
    clopts_program_free(s.program);
    NAOS_CHECK_INT_EQ(s.probe.live, 0);
}

NAOS_SUITE(result,
    NAOS_ADD_TEST(new_allocates_one_slot_per_root_option),
    NAOS_ADD_TEST(new_without_root_options_has_no_slots),
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
    NAOS_ADD_TEST(option_of_another_program_reads_as_unset_with_ndebug),
    NAOS_ADD_TEST(value_accessors_are_unset_unless_ok_or_no_command),
    NAOS_ADD_TEST(no_command_keeps_only_the_root_values),
    NAOS_ADD_TEST(error_accessors_report_only_errors),
    NAOS_ADD_TEST(fresh_result_has_no_error),
    NAOS_ADD_TEST(null_result_and_sentinel_read_like_the_sentinel),
    NAOS_ADD_TEST(run_calls_the_selected_command_handler),
    NAOS_ADD_TEST(run_calls_the_root_handler_without_subcommands),
    NAOS_ADD_TEST(free_after_the_program_releases_everything),
    NAOS_ADD_TEST(failing_result_new_leaks_nothing),
    NAOS_ADD_TEST(failing_select_leaves_the_result_unchanged),
    NAOS_ADD_TEST(failing_append_list_keeps_a_freeable_result),
    NAOS_ADD_TEST(failing_add_positional_keeps_a_freeable_result)
)
