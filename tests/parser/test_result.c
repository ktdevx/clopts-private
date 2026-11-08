/*
 * Suite result: access by descriptor pointer, positionals, clopts_result_run,
 * idempotent release, release of a zero-initialized result and release after
 * CLOPTS_OUT_OF_MEMORY.
 *
 * The cases here build the result through the internal result store API
 * directly; the cases that go through argv are added by the scanner task.
 */

#include "test_fixtures.h"

#include "parser/parser_internal.h"

#include <naos/naos.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Reference to options[index] of the global (true) or command (false) table. */
static clopts_parser_ref_t make_ref(const clopts_option_t *options, bool global, size_t index)
{
    clopts_parser_ref_t ref = { 0 };

    ref.option = &options[index];
    ref.global = global;
    ref.index = index;
    return ref;
}

/* Slot of options[index] in result. */
static clopts_parser_slot_t *slot_of(clopts_result_t *result, const clopts_option_t *options,
                                     bool global, size_t index)
{
    clopts_parser_ref_t ref = make_ref(options, global, index);

    return clopts_parser_store_slot(result->impl, &ref);
}

/* Starts a store for fixture_tool with the build command selected. */
#define BEGIN_TOOL_BUILD(result)                                                         \
    do                                                                                   \
    {                                                                                    \
        NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&(result), &fixture_tool), 0);     \
        NAOS_REQUIRE_INT_EQ(clopts_parser_store_select(                                  \
                                &(result), &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD]), \
                            0);                                                          \
        (result).status = CLOPTS_OK;                                                     \
        (result).command = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];               \
    } while (0)

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

/* ---- store lifecycle --------------------------------------------------- */

NAOS_TEST(store_begin_sets_impl_with_global_slots)
{
    clopts_result_t result = { 0 };
    clopts_parser_store_t *store;

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_tool), 0);
    store = result.impl;
    NAOS_REQUIRE_NOT_NULL(store);
    NAOS_CHECK_PTR_EQ(store->program, &fixture_tool);
    NAOS_CHECK_NULL(store->command);
    NAOS_CHECK_NOT_NULL(store->global_slots);
    NAOS_CHECK_NULL(store->command_slots);
    NAOS_CHECK_UINT_EQ(store->positionals.len, 0);

    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

NAOS_TEST(store_select_allocates_command_slots)
{
    clopts_result_t result = { 0 };
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    clopts_parser_store_t *store;

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_tool), 0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_select(&result, build), 0);
    store = result.impl;
    NAOS_REQUIRE_NOT_NULL(store);
    NAOS_CHECK_PTR_EQ(store->command, build);
    NAOS_CHECK_NOT_NULL(store->command_slots);

    clopts_result_free(&result);
}

NAOS_TEST(store_select_command_without_options)
{
    clopts_result_t result = { 0 };
    const clopts_command_t *copy = &fixture_tool_commands[FIXTURE_TOOL_CMD_COPY];
    clopts_parser_store_t *store;

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_tool), 0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_select(&result, copy), 0);
    store = result.impl;
    NAOS_REQUIRE_NOT_NULL(store);
    NAOS_CHECK_PTR_EQ(store->command, copy);

    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

NAOS_TEST(store_begin_releases_the_previous_impl)
{
    clopts_result_t result = { 0 };

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_tool), 0);
    NAOS_REQUIRE_INT_EQ(
        clopts_parser_store_select(&result, &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD]), 0);
    NAOS_REQUIRE_INT_EQ(
        clopts_parser_store_append_list(
            slot_of(&result, fixture_build_options, false, FIXTURE_BUILD_OPT_KEYS), "a,b", ',',
            CLOPTS_SOURCE_EXPLICIT),
        0);

    /* The second begin releases the first store (checked by the sanitizer build). */
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_filt), 0);
    NAOS_REQUIRE_NOT_NULL(result.impl);
    NAOS_CHECK_PTR_EQ(result.impl->program, &fixture_filt);
    NAOS_CHECK_NULL(result.impl->command);

    clopts_result_free(&result);
}

/* Declarations whose slot array size overflows size_t. Never read as tables. */
static const clopts_option_t huge_option = { .long_name = "huge", .kind = CLOPTS_FLAG };
static const clopts_program_t huge_program = {
    .name = "huge",
    .options = &huge_option,
    .option_count = SIZE_MAX,
};
static const clopts_command_t huge_command = {
    .name = "huge",
    .options = &huge_option,
    .option_count = SIZE_MAX,
};

NAOS_TEST(store_begin_fails_on_size_overflow)
{
    clopts_result_t result = { 0 };

    NAOS_CHECK_INT_EQ(clopts_parser_store_begin(&result, &huge_program), -1);
    /* Still releasable. */
    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

NAOS_TEST(store_select_fails_on_size_overflow_and_keeps_the_store)
{
    clopts_result_t result = { 0 };
    clopts_parser_store_t *store;
    clopts_parser_slot_t *global_slots;

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_tool), 0);
    store = result.impl;
    NAOS_REQUIRE_NOT_NULL(store);
    global_slots = store->global_slots;
    clopts_parser_store_add_count(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_VERBOSE), 0);

    NAOS_CHECK_INT_EQ(clopts_parser_store_select(&result, &huge_command), -1);

    NAOS_CHECK_PTR_EQ(result.impl, store);
    NAOS_CHECK_NULL(store->command);
    NAOS_CHECK_NULL(store->command_slots);
    NAOS_CHECK_PTR_EQ(store->global_slots, global_slots);
    NAOS_CHECK_UINT_EQ(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_VERBOSE)->count, 1);

    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

/* ---- accessors --------------------------------------------------------- */

NAOS_TEST(accessors_return_empty_values_when_nothing_was_recorded)
{
    clopts_result_t result = { 0 };
    const clopts_option_t *color = &fixture_tool_options[FIXTURE_TOOL_OPT_COLOR];
    const clopts_option_t *verbose = &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE];
    const clopts_option_t *output = &fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT];
    const clopts_option_t *keys = &fixture_build_options[FIXTURE_BUILD_OPT_KEYS];

    BEGIN_TOOL_BUILD(result);

    NAOS_CHECK_FALSE(clopts_result_flag(&result, color));
    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, verbose), 0);
    NAOS_CHECK_NULL(clopts_result_value(&result, output));
    NAOS_CHECK_UINT_EQ(clopts_result_list_count(&result, keys), 0);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, color), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, output), CLOPTS_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(clopts_result_positional_count(&result), 0);

    clopts_result_free(&result);
}

NAOS_TEST(accessors_find_the_slot_by_descriptor_pointer)
{
    clopts_result_t result = { 0 };
    const clopts_option_t *verbose = &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE];
    const clopts_option_t *color = &fixture_tool_options[FIXTURE_TOOL_OPT_COLOR];
    const clopts_option_t *config = &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG];
    const clopts_option_t *output = &fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT];
    const clopts_option_t *keys = &fixture_build_options[FIXTURE_BUILD_OPT_KEYS];
    const clopts_option_t *jobs = &fixture_build_options[FIXTURE_BUILD_OPT_JOBS];
    char config_value[] = "/etc/t";
    char output_value[] = "out";

    BEGIN_TOOL_BUILD(result);

    /* Global and command options share indexes 0..2; each must hit its own slot. */
    clopts_parser_store_add_count(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_VERBOSE), 0);
    clopts_parser_store_add_count(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_VERBOSE), 0);
    clopts_parser_store_set_flag(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_COLOR), true,
        CLOPTS_SOURCE_EXPLICIT);
    clopts_parser_store_set_value(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_CONFIG), config_value,
        CLOPTS_SOURCE_DEFAULT);
    clopts_parser_store_set_value(
        slot_of(&result, fixture_build_options, false, FIXTURE_BUILD_OPT_OUTPUT), output_value,
        CLOPTS_SOURCE_EXPLICIT);
    NAOS_REQUIRE_INT_EQ(
        clopts_parser_store_append_list(
            slot_of(&result, fixture_build_options, false, FIXTURE_BUILD_OPT_KEYS), "a,b", ',',
            CLOPTS_SOURCE_ENV),
        0);
    clopts_parser_store_add_count(
        slot_of(&result, fixture_build_options, false, FIXTURE_BUILD_OPT_JOBS), 8);

    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, verbose), 2);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, verbose), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_TRUE(clopts_result_flag(&result, color));
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, color), CLOPTS_SOURCE_EXPLICIT);
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, config), config_value);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, config), CLOPTS_SOURCE_DEFAULT);

    /* No copy for VALUE: the accessor returns the recorded pointer. */
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, output), output_value);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, output), CLOPTS_SOURCE_EXPLICIT);

    NAOS_REQUIRE_UINT_EQ(clopts_result_list_count(&result, keys), 2);
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, keys, 0), "a");
    NAOS_CHECK_STR_EQ(clopts_result_list_at(&result, keys, 1), "b");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, keys), CLOPTS_SOURCE_ENV);

    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, jobs), 1);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, jobs), CLOPTS_SOURCE_EXPLICIT);

    clopts_result_free(&result);
}

NAOS_TEST(accessors_work_without_subcommands)
{
    clopts_result_t result = { 0 };
    const clopts_option_t *lines = &fixture_filt_options[FIXTURE_FILT_OPT_LINES];

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_filt), 0);
    result.status = CLOPTS_OK;
    clopts_parser_store_set_value(
        slot_of(&result, fixture_filt_options, true, FIXTURE_FILT_OPT_LINES), lines->default_value,
        CLOPTS_SOURCE_DEFAULT);

    /* A default value points into the declaration table. */
    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, lines), lines->default_value);
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, lines), CLOPTS_SOURCE_DEFAULT);

    clopts_result_free(&result);
}

NAOS_TEST(accessors_work_with_no_command_status)
{
    clopts_result_t result = { 0 };
    const clopts_option_t *verbose = &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE];

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_tool), 0);
    result.status = CLOPTS_NO_COMMAND;
    clopts_parser_store_add_count(
        slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_VERBOSE), 0);

    NAOS_CHECK_UINT_EQ(clopts_result_count(&result, verbose), 1);

    clopts_result_free(&result);
}

NAOS_TEST(environment_value_is_owned_by_the_result)
{
    clopts_result_t result = { 0 };
    const clopts_option_t *config = &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG];
    clopts_parser_slot_t *slot;
    char *copy;

    BEGIN_TOOL_BUILD(result);
    slot = slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_CONFIG);
    NAOS_REQUIRE_NOT_NULL(slot);
    copy = copy_string("/etc/t");
    NAOS_REQUIRE_NOT_NULL(copy);
    clopts_parser_store_set_value(slot, copy, CLOPTS_SOURCE_ENV);
    clopts_parser_store_own(slot, copy);

    NAOS_CHECK_PTR_EQ(clopts_result_value(&result, config), copy);
    NAOS_CHECK_STR_EQ(clopts_result_value(&result, config), "/etc/t");
    NAOS_CHECK_INT_EQ(clopts_result_source(&result, config), CLOPTS_SOURCE_ENV);

    /* Released here; the sanitizer build checks for a leak or double free. */
    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

/* ---- positionals ------------------------------------------------------- */

NAOS_TEST(positionals_point_into_argv)
{
    clopts_result_t result = { 0 };
    char src[] = "src";
    char extra[] = "-";

    BEGIN_TOOL_BUILD(result);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, src), 0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, extra), 0);

    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(&result), 2);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(&result, 0), src);
    NAOS_CHECK_PTR_EQ(clopts_result_positional_at(&result, 1), extra);
    NAOS_CHECK_PTR_EQ(result.command, &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD]);

    clopts_result_free(&result);

    /* Releasing the result leaves argv alone. */
    NAOS_CHECK_STR_EQ(src, "src");
    NAOS_CHECK_STR_EQ(extra, "-");
}

NAOS_TEST(many_positionals_are_kept_in_order)
{
    clopts_result_t result = { 0 };
    char tokens[20][4];
    size_t i;

    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_filt), 0);
    result.status = CLOPTS_OK;
    for (i = 0; i < 20; i++)
    {
        tokens[i][0] = (char)('a' + (int)i);
        tokens[i][1] = '\0';
        NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, tokens[i]), 0);
    }

    NAOS_REQUIRE_UINT_EQ(clopts_result_positional_count(&result), 20);
    for (i = 0; i < 20; i++)
    {
        NAOS_CHECK_PTR_EQ(clopts_result_positional_at(&result, i), tokens[i]);
    }

    clopts_result_free(&result);
}

/* ---- clopts_result_run ------------------------------------------------- */

NAOS_TEST(run_calls_the_program_handler_without_subcommands)
{
    clopts_result_t result = { 0 };

    fixture_handler_reset(&fixture_filt_handler_record, 42);
    fixture_handler_reset(&fixture_build_handler_record, -1);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_filt), 0);
    result.status = CLOPTS_OK;

    NAOS_CHECK_INT_EQ(clopts_result_run(&result), 42);
    NAOS_CHECK_INT_EQ(fixture_filt_handler_record.calls, 1);
    NAOS_CHECK_PTR_EQ(fixture_filt_handler_record.last_result, &result);
    NAOS_CHECK_PTR_EQ(fixture_filt_handler_record.last_userdata, &fixture_filt_handler_record);
    NAOS_CHECK_INT_EQ(fixture_build_handler_record.calls, 0);

    clopts_result_free(&result);
}

NAOS_TEST(run_calls_the_selected_command_handler)
{
    clopts_result_t result = { 0 };

    fixture_handler_reset(&fixture_filt_handler_record, -1);
    fixture_handler_reset(&fixture_build_handler_record, 7);
    BEGIN_TOOL_BUILD(result);

    NAOS_CHECK_INT_EQ(clopts_result_run(&result), 7);
    NAOS_CHECK_INT_EQ(fixture_build_handler_record.calls, 1);
    NAOS_CHECK_PTR_EQ(fixture_build_handler_record.last_result, &result);
    NAOS_CHECK_PTR_EQ(fixture_build_handler_record.last_userdata, &fixture_build_handler_record);
    NAOS_CHECK_INT_EQ(fixture_filt_handler_record.calls, 0);

    /* The return value is passed through unchanged, whatever it is. */
    fixture_build_handler_record.return_value = 0;
    NAOS_CHECK_INT_EQ(clopts_result_run(&result), 0);
    NAOS_CHECK_INT_EQ(fixture_build_handler_record.calls, 2);

    clopts_result_free(&result);
}

/* ---- clopts_result_free ------------------------------------------------ */

NAOS_TEST(free_zero_initialized_result_is_safe)
{
    clopts_result_t result = { 0 };

    clopts_result_free(&result);

    NAOS_CHECK_NULL(result.impl);
    NAOS_CHECK_INT_EQ(result.status, CLOPTS_OK);
    NAOS_CHECK_NULL(result.command);
}

NAOS_TEST(free_zero_initialized_result_twice_is_safe)
{
    clopts_result_t result = { 0 };

    clopts_result_free(&result);
    clopts_result_free(&result);

    NAOS_CHECK_NULL(result.impl);
}

NAOS_TEST(free_releases_everything_and_is_idempotent)
{
    clopts_result_t result = { 0 };
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    char *copy;
    char token[] = "src";

    BEGIN_TOOL_BUILD(result);
    result.error.argv_index = 3;
    copy = copy_string("/etc/t");
    NAOS_REQUIRE_NOT_NULL(copy);
    clopts_parser_store_own(slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_CONFIG),
                            copy);
    NAOS_REQUIRE_INT_EQ(
        clopts_parser_store_append_list(
            slot_of(&result, fixture_build_options, false, FIXTURE_BUILD_OPT_KEYS),
            "a,b,c,d,e,f,g,h,i,j", ',', CLOPTS_SOURCE_EXPLICIT),
        0);
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_add_positional(result.impl, token), 0);

    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
    /* status, command and error are left as they were. */
    NAOS_CHECK_INT_EQ(result.status, CLOPTS_OK);
    NAOS_CHECK_PTR_EQ(result.command, build);
    NAOS_CHECK_INT_EQ(result.error.argv_index, 3);
    NAOS_CHECK_STR_EQ(token, "src");

    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

NAOS_TEST(free_after_out_of_memory_status_is_safe)
{
    clopts_result_t result = { 0 };
    char *copy;

    /* A store left behind by a failed parse is released the same way. */
    NAOS_REQUIRE_INT_EQ(clopts_parser_store_begin(&result, &fixture_tool), 0);
    copy = copy_string("/etc/t");
    NAOS_REQUIRE_NOT_NULL(copy);
    clopts_parser_store_own(slot_of(&result, fixture_tool_options, true, FIXTURE_TOOL_OPT_CONFIG),
                            copy);
    result.status = CLOPTS_OUT_OF_MEMORY;

    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
    NAOS_CHECK_INT_EQ(result.status, CLOPTS_OUT_OF_MEMORY);
    clopts_result_free(&result);
    NAOS_CHECK_NULL(result.impl);
}

NAOS_SUITE(result,
    NAOS_ADD_TEST(store_begin_sets_impl_with_global_slots),
    NAOS_ADD_TEST(store_select_allocates_command_slots),
    NAOS_ADD_TEST(store_select_command_without_options),
    NAOS_ADD_TEST(store_begin_releases_the_previous_impl),
    NAOS_ADD_TEST(store_begin_fails_on_size_overflow),
    NAOS_ADD_TEST(store_select_fails_on_size_overflow_and_keeps_the_store),
    NAOS_ADD_TEST(accessors_return_empty_values_when_nothing_was_recorded),
    NAOS_ADD_TEST(accessors_find_the_slot_by_descriptor_pointer),
    NAOS_ADD_TEST(accessors_work_without_subcommands),
    NAOS_ADD_TEST(accessors_work_with_no_command_status),
    NAOS_ADD_TEST(environment_value_is_owned_by_the_result),
    NAOS_ADD_TEST(positionals_point_into_argv),
    NAOS_ADD_TEST(many_positionals_are_kept_in_order),
    NAOS_ADD_TEST(run_calls_the_program_handler_without_subcommands),
    NAOS_ADD_TEST(run_calls_the_selected_command_handler),
    NAOS_ADD_TEST(free_zero_initialized_result_is_safe),
    NAOS_ADD_TEST(free_zero_initialized_result_twice_is_safe),
    NAOS_ADD_TEST(free_releases_everything_and_is_idempotent),
    NAOS_ADD_TEST(free_after_out_of_memory_status_is_safe)
)
