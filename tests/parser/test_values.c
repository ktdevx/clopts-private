/*
 * Suite values: repetition rules of FLAG / COUNT / VALUE / LIST, list
 * delimiters and the count limit.
 *
 * The first cases record through the internal result store API directly; the
 * cases named parse_* go through argv with clopts_parse_with_env.
 */

#include "test_fixtures.h"

#include "parser/parser_internal.h"

#include <naos/naos.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Reference to fixture_tool_options[index] (a global option). */
static clopts_parser_ref_t tool_ref(size_t index)
{
    clopts_parser_ref_t ref = { 0 };

    ref.option = &fixture_tool_options[index];
    ref.global = true;
    ref.index = index;
    return ref;
}

/* Reference to fixture_build_options[index] (a command option). */
static clopts_parser_ref_t build_ref(size_t index)
{
    clopts_parser_ref_t ref = { 0 };

    ref.option = &fixture_build_options[index];
    ref.global = false;
    ref.index = index;
    return ref;
}

/* Starts a store for fixture_tool with the build command selected. */
#define BEGIN_TOOL_BUILD(result)                                                         \
    do                                                                                   \
    {                                                                                    \
        NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&(result), &fixture_tool), 0);     \
        NAOS_REQUIRE_INT_EQ(clopts_parser_store_select(                                  \
                                &(result), &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD]), \
                            0);                                                          \
    } while (0)

/* Slot of the option behind ref in result. */
static clopts_parser_slot_t *slot_of(clopts_result_t *result, clopts_parser_ref_t ref)
{
    return clopts_parser_store_slot(result->impl, &ref);
}

/* Item index of a LIST slot. */
static const char *item_at(const clopts_parser_slot_t *slot, size_t index)
{
    return *(char *const *)clopts_core_vec_at(&slot->items, index);
}

/* ---- fixtures ---------------------------------------------------------- */

NAOS_TEST(fixture_list_delimiters_are_as_designed)
{
    const clopts_option_t *keys = &fixture_build_options[FIXTURE_BUILD_OPT_KEYS];
    const clopts_option_t *tags = &fixture_cat_options[FIXTURE_CAT_OPT_TAGS];

    NAOS_CHECK_INT_EQ(keys->kind, CLOPTS_LIST);
    NAOS_CHECK_INT_EQ(keys->list_delimiter, ',');
    NAOS_CHECK_INT_EQ(tags->kind, CLOPTS_LIST);
    NAOS_CHECK_INT_EQ(tags->list_delimiter, ',');
}

NAOS_TEST(fixture_count_limits_are_as_designed)
{
    const clopts_option_t *jobs = &fixture_build_options[FIXTURE_BUILD_OPT_JOBS];
    const clopts_option_t *verbose = &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE];

    NAOS_CHECK_INT_EQ(jobs->kind, CLOPTS_COUNT);
    NAOS_CHECK_UINT_EQ(jobs->count_max, 8);
    /* 0 selects CLOPTS_COUNT_MAX_DEFAULT. */
    NAOS_CHECK_INT_EQ(verbose->kind, CLOPTS_COUNT);
    NAOS_CHECK_UINT_EQ(verbose->count_max, 0);
}

/* ---- FLAG -------------------------------------------------------------- */

NAOS_TEST(store_slots_start_empty)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);

    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_COLOR));
    NAOS_REQUIRE_NOT_NULL(slot);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_FALSE(slot->flag);
    NAOS_CHECK_UINT_EQ(slot->count, 0);
    NAOS_CHECK_NULL(slot->value);
    NAOS_CHECK_NULL(slot->owned);
    NAOS_CHECK_UINT_EQ(slot->items.len, 0);

    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(slot->items.len, 0);

    clopts_result_free(&result);
}

NAOS_TEST(flag_repeated_stays_true)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_COLOR));
    NAOS_REQUIRE_NOT_NULL(slot);

    clopts_parser_store_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_store_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_TRUE(slot->flag);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

NAOS_TEST(flag_negated_is_false_and_explicit)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_COLOR));
    NAOS_REQUIRE_NOT_NULL(slot);

    clopts_parser_store_set_flag(slot, false, CLOPTS_SOURCE_EXPLICIT);

    NAOS_CHECK_FALSE(slot->flag);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

NAOS_TEST(flag_last_record_wins)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_COLOR));
    NAOS_REQUIRE_NOT_NULL(slot);

    /* --color --no-color */
    clopts_parser_store_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_store_set_flag(slot, false, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_FALSE(slot->flag);

    /* ... --color */
    clopts_parser_store_set_flag(slot, true, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(slot->flag);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

/* ---- COUNT ------------------------------------------------------------- */

NAOS_TEST(count_adds_one_per_record)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    unsigned max = fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE].count_max;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_VERBOSE));
    NAOS_REQUIRE_NOT_NULL(slot);

    clopts_parser_store_add_count(slot, max);
    NAOS_CHECK_UINT_EQ(slot->count, 1);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_store_add_count(slot, max);
    clopts_parser_store_add_count(slot, max);
    NAOS_CHECK_UINT_EQ(slot->count, 3);

    clopts_result_free(&result);
}

NAOS_TEST(count_is_clamped_to_count_max)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    unsigned max = fixture_build_options[FIXTURE_BUILD_OPT_JOBS].count_max;
    int i;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_JOBS));
    NAOS_REQUIRE_NOT_NULL(slot);

    for (i = 0; i < 8; i++)
    {
        clopts_parser_store_add_count(slot, max);
    }
    NAOS_CHECK_UINT_EQ(slot->count, 8);

    /* The 9th and later occurrences are clamped, not an error. */
    clopts_parser_store_add_count(slot, max);
    clopts_parser_store_add_count(slot, max);
    NAOS_CHECK_UINT_EQ(slot->count, 8);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

NAOS_TEST(count_with_default_limit_keeps_growing)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    int i;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_VERBOSE));
    NAOS_REQUIRE_NOT_NULL(slot);

    for (i = 0; i < 1000; i++)
    {
        clopts_parser_store_add_count(slot, 0);
    }
    NAOS_CHECK_UINT_EQ(slot->count, 1000);

    clopts_result_free(&result);
}

NAOS_TEST(count_with_default_limit_clamps_at_count_max_default)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_VERBOSE));
    NAOS_REQUIRE_NOT_NULL(slot);

    /* Start just below the default limit instead of adding UINT_MAX times. */
    slot->count = CLOPTS_COUNT_MAX_DEFAULT - 1u;
    clopts_parser_store_add_count(slot, 0);
    NAOS_CHECK_UINT_EQ(slot->count, CLOPTS_COUNT_MAX_DEFAULT);
    clopts_parser_store_add_count(slot, 0);
    NAOS_CHECK_UINT_EQ(slot->count, CLOPTS_COUNT_MAX_DEFAULT);
    NAOS_CHECK_UINT_EQ(slot->count, UINT_MAX);

    clopts_result_free(&result);
}

/* ---- VALUE ------------------------------------------------------------- */

NAOS_TEST(value_last_record_wins)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    char first[] = "a";
    char second[] = "b";

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_OUTPUT));
    NAOS_REQUIRE_NOT_NULL(slot);

    clopts_parser_store_set_value(slot, first, CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_store_set_value(slot, second, CLOPTS_SOURCE_EXPLICIT);

    /* The value is not copied: it points at the recorded string itself. */
    NAOS_CHECK_PTR_EQ(slot->value, second);
    NAOS_CHECK_STR_EQ(slot->value, "b");
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_NULL(slot->owned);

    clopts_result_free(&result);
}

NAOS_TEST(value_record_overwrites_source)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    const char *default_value = fixture_filt_options[FIXTURE_FILT_OPT_LINES].default_value;
    char explicit_value[] = "5";

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_CONFIG));
    NAOS_REQUIRE_NOT_NULL(slot);

    clopts_parser_store_set_value(slot, default_value, CLOPTS_SOURCE_DEFAULT);
    NAOS_CHECK_PTR_EQ(slot->value, default_value);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_DEFAULT);

    clopts_parser_store_set_value(slot, explicit_value, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(slot->value, explicit_value);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

NAOS_TEST(value_keeps_the_string_as_is)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    char number[] = "not-a-number";
    char empty[] = "";

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_OUTPUT));
    NAOS_REQUIRE_NOT_NULL(slot);

    /* No conversion or validation: any string, including "", is kept. */
    clopts_parser_store_set_value(slot, number, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_STR_EQ(slot->value, "not-a-number");
    clopts_parser_store_set_value(slot, empty, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(slot->value, empty);
    NAOS_CHECK_STR_EQ(slot->value, "");

    clopts_result_free(&result);
}

/* ---- LIST -------------------------------------------------------------- */

NAOS_TEST(list_repeats_are_concatenated)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    char first[] = "a,b";
    char second[] = "c";

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, first, ',', CLOPTS_SOURCE_EXPLICIT),
                        0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, second, ',', CLOPTS_SOURCE_EXPLICIT),
                        0);

    NAOS_REQUIRE_UINT_EQ(slot->items.len, 3);
    NAOS_CHECK_STR_EQ(item_at(slot, 0), "a");
    NAOS_CHECK_STR_EQ(item_at(slot, 1), "b");
    NAOS_CHECK_STR_EQ(item_at(slot, 2), "c");
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

NAOS_TEST(list_items_are_copies)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    char value[] = "c";

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, value, ',', CLOPTS_SOURCE_EXPLICIT),
                        0);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 1);
    NAOS_CHECK_TRUE(item_at(slot, 0) != value);

    /* Changing the source string later does not change the item. */
    value[0] = 'x';
    NAOS_CHECK_STR_EQ(item_at(slot, 0), "c");
    NAOS_CHECK_STR_EQ(value, "x");

    clopts_result_free(&result);
}

NAOS_TEST(list_keeps_empty_items)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, "a,,b", ',', CLOPTS_SOURCE_EXPLICIT),
                        0);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 3);
    NAOS_CHECK_STR_EQ(item_at(slot, 0), "a");
    NAOS_CHECK_STR_EQ(item_at(slot, 1), "");
    NAOS_CHECK_STR_EQ(item_at(slot, 2), "b");

    clopts_result_free(&result);
}

NAOS_TEST(list_lone_delimiter_gives_two_empty_items)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, ",", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 2);
    NAOS_CHECK_STR_EQ(item_at(slot, 0), "");
    NAOS_CHECK_STR_EQ(item_at(slot, 1), "");

    clopts_result_free(&result);
}

NAOS_TEST(list_empty_value_is_one_empty_item)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, "", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 1);
    NAOS_CHECK_STR_EQ(item_at(slot, 0), "");

    clopts_result_free(&result);
}

NAOS_TEST(list_keeps_whitespace)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(
        clopts_parser_store_append_list(slot, " a , b", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 2);
    NAOS_CHECK_STR_EQ(item_at(slot, 0), " a ");
    NAOS_CHECK_STR_EQ(item_at(slot, 1), " b");

    clopts_result_free(&result);
}

NAOS_TEST(list_without_delimiter_is_one_item_per_record)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, "a,b", '\0', CLOPTS_SOURCE_EXPLICIT),
                        0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, "", '\0', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_REQUIRE_UINT_EQ(slot->items.len, 2);
    NAOS_CHECK_STR_EQ(item_at(slot, 0), "a,b");
    NAOS_CHECK_STR_EQ(item_at(slot, 1), "");

    clopts_result_free(&result);
}

NAOS_TEST(list_record_overwrites_source)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, build_ref(FIXTURE_BUILD_OPT_KEYS));
    NAOS_REQUIRE_NOT_NULL(slot);

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, "x,y", ',', CLOPTS_SOURCE_ENV), 0);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_ENV);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_append_list(slot, "z", ',', CLOPTS_SOURCE_EXPLICIT), 0);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_UINT_EQ(slot->items.len, 3);

    clopts_result_free(&result);
}

/* ---- ownership of environment copies ----------------------------------- */

/* Returns a malloc'ed copy of s, as an environment lookup would. */
static char *copy_string(const char *s)
{
    size_t len = strlen(s);
    char *copy = malloc(len + 1u);

    if (copy != NULL)
    {
        memcpy(copy, s, len + 1u);
    }
    return copy;
}

NAOS_TEST(own_moves_the_copy_into_the_slot)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    char *copy;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_CONFIG));
    NAOS_REQUIRE_NOT_NULL(slot);
    copy = copy_string("/etc/t");
    NAOS_REQUIRE_NOT_NULL(copy);

    clopts_parser_store_set_value(slot, copy, CLOPTS_SOURCE_ENV);
    clopts_parser_store_own(slot, copy);

    NAOS_CHECK_PTR_EQ(slot->owned, copy);
    NAOS_CHECK_PTR_EQ(slot->value, copy);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_ENV);

    /* Released by clopts_result_free; the sanitizer build checks for a leak. */
    clopts_result_free(&result);
}

NAOS_TEST(own_replaces_and_releases_the_previous_copy)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    char *first;
    char *second;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_CONFIG));
    NAOS_REQUIRE_NOT_NULL(slot);
    first = copy_string("one");
    second = copy_string("two");
    NAOS_REQUIRE_NOT_NULL(first);
    NAOS_REQUIRE_NOT_NULL(second);

    clopts_parser_store_own(slot, first);
    NAOS_CHECK_PTR_EQ(slot->value, first);
    clopts_parser_store_own(slot, second);
    NAOS_CHECK_PTR_EQ(slot->owned, second);
    NAOS_CHECK_PTR_EQ(slot->value, second);
    NAOS_CHECK_STR_EQ(slot->value, "two");

    clopts_result_free(&result);
}

NAOS_TEST(set_value_after_own_releases_the_copy)
{
    clopts_result_t result = { 0 };
    clopts_parser_slot_t *slot;
    char *copy;
    char explicit_value[] = "x";

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, tool_ref(FIXTURE_TOOL_OPT_CONFIG));
    NAOS_REQUIRE_NOT_NULL(slot);
    copy = copy_string("/etc/t");
    NAOS_REQUIRE_NOT_NULL(copy);

    clopts_parser_store_own(slot, copy);
    clopts_parser_store_set_value(slot, explicit_value, CLOPTS_SOURCE_EXPLICIT);

    /* value == owned whenever owned is not NULL: the copy is gone. */
    NAOS_CHECK_NULL(slot->owned);
    NAOS_CHECK_PTR_EQ(slot->value, explicit_value);
    NAOS_CHECK_INT_EQ(slot->source, CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

/* ---- through clopts_parse_with_env ------------------------------------- */

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define KEYS_OPT (&fixture_build_options[FIXTURE_BUILD_OPT_KEYS])
#define OUTPUT_OPT (&fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT])
#define JOBS_OPT (&fixture_build_options[FIXTURE_BUILD_OPT_JOBS])
#define COLOR_OPT (&fixture_tool_options[FIXTURE_TOOL_OPT_COLOR])
#define TAGS_OPT (&fixture_cat_options[FIXTURE_CAT_OPT_TAGS])

/*
 * Parses argv (argc elements, NULL-terminated) with an empty mock environment
 * and checks that the status matches result->status.
 */
static clopts_status_t parse_argv(const clopts_program_t *program, int argc, char **argv,
                                  clopts_result_t *result)
{
    fixture_env_t env;
    clopts_status_t status;

    fixture_env_init(&env, NULL, 0);
    status = clopts_parse_with_env(program, argc, argv, fixture_env_getenv, &env, result);
    NAOS_CHECK_INT_EQ(result->status, status);
    return status;
}

/* Checks the items of a LIST option against expected[0..count-1]. */
static void check_items(const clopts_result_t *result, const clopts_option_t *option,
                        const char *const *expected, size_t count)
{
    size_t i;

    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(result, option), count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_STR_EQ(clopts_result_list_at(result, option, i), expected[i]);
    }
}

NAOS_TEST(parse_short_list_repeats_are_concatenated)
{
    static const char *const expected[] = { "a", "b", "c" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", "a,b", "-k", "c", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 9, argv, &result), CLOPTS_OK);
    check_items(&result, KEYS_OPT, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, KEYS_OPT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(parse_long_list_repeats_are_concatenated)
{
    static const char *const expected[] = { "a", "b", "c" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "--keys=a", "--keys", "b,c", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 8, argv, &result), CLOPTS_OK);
    check_items(&result, KEYS_OPT, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, KEYS_OPT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(parse_list_keeps_empty_items)
{
    static const char *const expected[] = { "a", "", "b" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", "a,,b", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 7, argv, &result), CLOPTS_OK);
    check_items(&result, KEYS_OPT, expected, ARRAY_LEN(expected));
    clopts_result_free(&result);
}

NAOS_TEST(parse_list_lone_delimiter_gives_two_empty_items)
{
    static const char *const expected[] = { "", "" };
    char *argv[] = { "tool", "build", "x", "-o", "y", "-k", ",", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 7, argv, &result), CLOPTS_OK);
    check_items(&result, KEYS_OPT, expected, ARRAY_LEN(expected));
    clopts_result_free(&result);
}

NAOS_TEST(parse_value_last_occurrence_wins)
{
    char *argv[] = { "tool", "build", "x", "--output", "a", "--output", "b", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 7, argv, &result), CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, OUTPUT_OPT), argv[6]);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, OUTPUT_OPT), "b");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, OUTPUT_OPT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(parse_count_nine_times_is_clamped_to_eight)
{
    char *argv[] = { "tool", "build", "x", "-o", "y", "-j", "-j", "-j", "-j",
                     "-j", "-j", "-j", "-j", "-j", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 14, argv, &result), CLOPTS_OK);
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, JOBS_OPT), 8);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, JOBS_OPT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(parse_flag_then_negation_is_false)
{
    char *argv[] = { "tool", "--color", "--no-color", "cat", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 4, argv, &result), CLOPTS_OK);
    NAOS_CHECK_FALSE(clopts_result_flag(&result, COLOR_OPT));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, COLOR_OPT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(parse_negation_then_flag_is_true)
{
    char *argv[] = { "tool", "cat", "--no-color", "--color", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 4, argv, &result), CLOPTS_OK);
    NAOS_CHECK_TRUE(clopts_result_flag(&result, COLOR_OPT));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, COLOR_OPT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_TEST(parse_cat_tags_default_is_split)
{
    static const char *const expected[] = { "a", "b" };
    char *argv[] = { "tool", "cat", NULL };
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(parse_argv(&fixture_tool, 2, argv, &result), CLOPTS_OK);
    check_items(&result, TAGS_OPT, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TAGS_OPT), CLOPTS_SOURCE_DEFAULT);
    clopts_result_free(&result);
}

NAOS_TEST(parse_cat_tags_env_is_split)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x,,y" } };
    static const char *const expected[] = { "x", "", "y" };
    char *argv[] = { "tool", "cat", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(
        clopts_parse_with_env(&fixture_tool, 2, argv, fixture_env_getenv, &env, &result),
        CLOPTS_OK);
    check_items(&result, TAGS_OPT, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TAGS_OPT), CLOPTS_SOURCE_ENV);
    clopts_result_free(&result);
}

NAOS_TEST(parse_cat_tags_explicit_is_split_and_wins)
{
    static const fixture_env_var_t vars[] = { { "TOOL_TAGS", "x,y" } };
    static const char *const expected[] = { "p", "q", "r" };
    char *argv[] = { "tool", "cat", "--tags", "p,q", "--tags=r", NULL };
    clopts_result_t result = { 0 };
    fixture_env_t env;

    fixture_env_init(&env, vars, ARRAY_LEN(vars));
    NAOS_REQUIRE_INT_EQ(
        clopts_parse_with_env(&fixture_tool, 5, argv, fixture_env_getenv, &env, &result),
        CLOPTS_OK);
    check_items(&result, TAGS_OPT, expected, ARRAY_LEN(expected));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, TAGS_OPT), CLOPTS_SOURCE_EXPLICIT);
    clopts_result_free(&result);
}

NAOS_SUITE(values,
    NAOS_ADD_TEST(fixture_list_delimiters_are_as_designed),
    NAOS_ADD_TEST(fixture_count_limits_are_as_designed),
    NAOS_ADD_TEST(store_slots_start_empty),
    NAOS_ADD_TEST(flag_repeated_stays_true),
    NAOS_ADD_TEST(flag_negated_is_false_and_explicit),
    NAOS_ADD_TEST(flag_last_record_wins),
    NAOS_ADD_TEST(count_adds_one_per_record),
    NAOS_ADD_TEST(count_is_clamped_to_count_max),
    NAOS_ADD_TEST(count_with_default_limit_keeps_growing),
    NAOS_ADD_TEST(count_with_default_limit_clamps_at_count_max_default),
    NAOS_ADD_TEST(value_last_record_wins),
    NAOS_ADD_TEST(value_record_overwrites_source),
    NAOS_ADD_TEST(value_keeps_the_string_as_is),
    NAOS_ADD_TEST(list_repeats_are_concatenated),
    NAOS_ADD_TEST(list_items_are_copies),
    NAOS_ADD_TEST(list_keeps_empty_items),
    NAOS_ADD_TEST(list_lone_delimiter_gives_two_empty_items),
    NAOS_ADD_TEST(list_empty_value_is_one_empty_item),
    NAOS_ADD_TEST(list_keeps_whitespace),
    NAOS_ADD_TEST(list_without_delimiter_is_one_item_per_record),
    NAOS_ADD_TEST(list_record_overwrites_source),
    NAOS_ADD_TEST(own_moves_the_copy_into_the_slot),
    NAOS_ADD_TEST(own_replaces_and_releases_the_previous_copy),
    NAOS_ADD_TEST(set_value_after_own_releases_the_copy),
    NAOS_ADD_TEST(parse_short_list_repeats_are_concatenated),
    NAOS_ADD_TEST(parse_long_list_repeats_are_concatenated),
    NAOS_ADD_TEST(parse_list_keeps_empty_items),
    NAOS_ADD_TEST(parse_list_lone_delimiter_gives_two_empty_items),
    NAOS_ADD_TEST(parse_value_last_occurrence_wins),
    NAOS_ADD_TEST(parse_count_nine_times_is_clamped_to_eight),
    NAOS_ADD_TEST(parse_flag_then_negation_is_false),
    NAOS_ADD_TEST(parse_negation_then_flag_is_true),
    NAOS_ADD_TEST(parse_cat_tags_default_is_split),
    NAOS_ADD_TEST(parse_cat_tags_env_is_split),
    NAOS_ADD_TEST(parse_cat_tags_explicit_is_split_and_wins)
)
