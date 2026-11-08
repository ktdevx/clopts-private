/*
 * Suite positional: positionals mixed with options, --, a lone -, required /
 * optional / variadic positionals and unexpected arguments.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

/* Checks name, required and variadic of one positional. */
static void check_positional(const clopts_positional_t *positional, const char *name,
                             bool required, bool variadic)
{
    NAOS_CHECK_STR_EQ(positional->name, name);
    NAOS_CHECK_INT_EQ(positional->required ? 1 : 0, required ? 1 : 0);
    NAOS_CHECK_INT_EQ(positional->variadic ? 1 : 0, variadic ? 1 : 0);
}

NAOS_TEST(fixture_positionals_are_as_designed)
{
    check_positional(&fixture_build_positionals[FIXTURE_BUILD_POS_SRC], "SRC", true, false);
    check_positional(&fixture_build_positionals[FIXTURE_BUILD_POS_EXTRA], "EXTRA", false, true);
    check_positional(&fixture_copy_positionals[FIXTURE_COPY_POS_SRC], "SRC", true, false);
    check_positional(&fixture_copy_positionals[FIXTURE_COPY_POS_DST], "DST", true, false);
    check_positional(&fixture_cat_positionals[FIXTURE_CAT_POS_FILE], "FILE", false, true);
    check_positional(&fixture_filt_positionals[FIXTURE_FILT_POS_FILE], "FILE", false, true);
}

NAOS_TEST(fixture_programs_with_commands_have_no_program_positionals)
{
    NAOS_CHECK_UINT_EQ(fixture_tool.positional_count, 0);
    NAOS_CHECK_NULL(fixture_tool.positionals);
    NAOS_CHECK_UINT_EQ(fixture_strict.positional_count, 0);
    NAOS_CHECK_NULL(fixture_strict.positionals);
}

NAOS_SUITE(positional,
    NAOS_ADD_TEST(fixture_positionals_are_as_designed),
    NAOS_ADD_TEST(fixture_programs_with_commands_have_no_program_positionals)
)
