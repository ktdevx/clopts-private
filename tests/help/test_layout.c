/*
 * Suite layout: the whole help texts, the Usage line, the version line and
 * the usage hint, compared byte for byte.
 *
 * It also checks that the fixtures it uses (tool, filt, bare) are valid
 * declarations.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

NAOS_TEST(fixture_tool_validates_ok)
{
    help_fixture_tool_t f;
    clopts_result_t *result;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_NOT_NULL(f.build);
    NAOS_CHECK_NOT_NULL(f.copy);
    NAOS_CHECK_NOT_NULL(f.verbose);
    NAOS_CHECK_NOT_NULL(f.color);
    NAOS_CHECK_NOT_NULL(f.config);
    NAOS_CHECK_NOT_NULL(f.output);
    NAOS_CHECK_NOT_NULL(f.keys);
    NAOS_CHECK_NOT_NULL(f.build_src);
    NAOS_CHECK_NOT_NULL(f.build_extra);
    NAOS_CHECK_NOT_NULL(f.copy_src);
    NAOS_CHECK_NOT_NULL(f.copy_dst);

    result = clopts_program_validate(f.program);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    clopts_result_free(result);

    help_fixture_tool_free(&f);
    NAOS_CHECK_NULL(f.program);
}

NAOS_TEST(fixture_filt_validates_ok)
{
    help_fixture_filt_t f;
    clopts_result_t *result;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_NOT_NULL(f.lines);
    NAOS_CHECK_NOT_NULL(f.file);

    result = clopts_program_validate(f.program);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    clopts_result_free(result);

    help_fixture_filt_free(&f);
    NAOS_CHECK_NULL(f.program);
}

NAOS_TEST(fixture_bare_validates_ok)
{
    help_fixture_bare_t f;
    clopts_result_t *result;

    help_fixture_bare_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_NOT_NULL(f.run);

    result = clopts_program_validate(f.program);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    clopts_result_free(result);

    help_fixture_bare_free(&f);
    NAOS_CHECK_NULL(f.program);
}

NAOS_TEST(fixture_free_accepts_a_cleared_struct)
{
    help_fixture_tool_t tool = {0};
    help_fixture_filt_t filt = {0};
    help_fixture_bare_t bare = {0};

    help_fixture_tool_free(&tool);
    help_fixture_filt_free(&filt);
    help_fixture_bare_free(&bare);
    NAOS_CHECK_NULL(tool.program);
    NAOS_CHECK_NULL(filt.program);
    NAOS_CHECK_NULL(bare.program);
}

NAOS_SUITE(layout,
    NAOS_ADD_TEST(fixture_tool_validates_ok),
    NAOS_ADD_TEST(fixture_filt_validates_ok),
    NAOS_ADD_TEST(fixture_bare_validates_ok),
    NAOS_ADD_TEST(fixture_free_accepts_a_cleared_struct)
)
