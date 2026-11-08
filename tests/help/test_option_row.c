/*
 * Suite option_row: the left column and the notes of one option row
 * (Requirement 4), and the alignment of a list with non-ASCII names.
 *
 * It also checks that the wide fixture, with non-ASCII names and bytes that
 * are not valid UTF-8, is a valid declaration.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <string.h>

NAOS_TEST(fixture_wide_validates_ok)
{
    help_fixture_wide_t f;
    clopts_result_t *result;

    help_fixture_wide_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_NOT_NULL(f.naive);
    NAOS_CHECK_NOT_NULL(f.nihongo);
    NAOS_CHECK_NOT_NULL(f.raw);
    NAOS_CHECK_NOT_NULL(f.file);

    result = clopts_program_validate(f.program);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    clopts_result_free(result);

    help_fixture_wide_free(&f);
    NAOS_CHECK_NULL(f.program);
}

NAOS_TEST(fixture_wide_names_have_the_intended_bytes)
{
    /* The byte lengths pin the spellings used by the alignment tests. */
    NAOS_CHECK_UINT_EQ(strlen(HELP_FIXTURE_NAIVE), 6);
    NAOS_CHECK_UINT_EQ(strlen(HELP_FIXTURE_NIHONGO), 9);
    NAOS_CHECK_UINT_EQ(strlen(HELP_FIXTURE_ATAI), 3);
    NAOS_CHECK_UINT_EQ(strlen(HELP_FIXTURE_FILE), 12);
    NAOS_CHECK_MEM_EQ(HELP_FIXTURE_RAW_VALUE_NAME, strlen(HELP_FIXTURE_RAW_VALUE_NAME),
                      "\x42\xFF\xC0\xAF\xE6\x97\x45", (size_t)7);
}

NAOS_SUITE(option_row,
    NAOS_ADD_TEST(fixture_wide_validates_ok),
    NAOS_ADD_TEST(fixture_wide_names_have_the_intended_bytes)
)
