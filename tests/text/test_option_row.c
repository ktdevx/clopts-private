/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite option_row: the left column and the notes of one option row
 * (Requirement 4), and the alignment of a list with non-ASCII names.
 *
 * It also checks that the wide fixture, with non-ASCII names and bytes that
 * are not valid UTF-8, is built without a declaration error.
 *
 * Each option is the only global option of a program of its own, so its
 * row is the only row of the Options paragraph of clopts_write_help(); the
 * expected bytes are derived by hand from the rules of the design.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include "base/utf8.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* The text the help of a one-option program "t" writes before the option row. */
#define SOLO_PREFIX "Usage: t [<options>]\n\nOptions:\n"

/* At most this many one-option programs per test. */
#define SOLO_MAX 4

/* The one-option programs of the running test, with their option. */
static struct
{
    clopts_program_t *program;
    const clopts_option_t *option;
} solos[SOLO_MAX];
static size_t solo_count;

/*
 * Starts a test: clears the failure count of the declaration helpers and
 * the one-option programs. A test creates and configures its options, gives
 * each one to solo() and then requires test_decl_failures to be 0.
 */
static void begin(void)
{
    test_decl_reset();
    solo_count = 0;
}

/*
 * Attaches option to a new program "t" of its own and returns it (NULL, and
 * one more failure, when that fails). Its row is then the only row of the
 * Options paragraph of that program's help.
 */
static const clopts_option_t *solo(clopts_option_t *option)
{
    clopts_program_t *program;

    NAOS_REQUIRE_TRUE(solo_count < SOLO_MAX);
    program = test_decl_begin("t");
    solos[solo_count].option = test_decl_global_attach(program, option);
    solos[solo_count].program = program;
    return solos[solo_count++].option;
}

/* Releases the programs of solo(). */
static void end(void)
{
    size_t i;

    for (i = 0; i < solo_count; i++)
    {
        test_decl_discard(solos[i].program);
    }
    solo_count = 0;
}

/* Writes the help of the one-option program of option into rec. */
static clopts_write_status_t write_option(text_recorder_t *rec, const clopts_option_t *option)
{
    size_t i = 0;

    text_recorder_init(rec);
    while ((i < solo_count) && (solos[i].option != option))
    {
        i++;
    }
    /* The option must have been given to solo(). */
    NAOS_REQUIRE_UINT_LT(i, solo_count);
    return clopts_write_help(solos[i].program, NULL, text_recorder_write, rec);
}

/* The row of option in its one-option help is exactly expected. */
#define CHECK_ROW(option, expected)                                                \
    do                                                                             \
    {                                                                              \
        static text_recorder_t rec_;                                               \
        static const char full_[] = SOLO_PREFIX expected;                          \
        NAOS_CHECK_INT_EQ(write_option(&rec_, (option)), CLOPTS_WRITE_OK);         \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, full_, sizeof(full_) - 1);           \
    } while (0)

/*
 * Writes the help of program into rec and checks that it holds the lines
 * of expected, in one piece.
 */
static void check_help_has(const clopts_program_t *program, const char *expected)
{
    static text_recorder_t rec;

    text_recorder_init(&rec);
    NAOS_REQUIRE_INT_EQ(clopts_write_help(program, NULL, text_recorder_write, &rec), CLOPTS_WRITE_OK);
    NAOS_REQUIRE_FALSE(rec.overflow);
    NAOS_CHECK_NOT_NULL(strstr(rec.buf, expected));
}

/* ---- fixture ----------------------------------------------------------- */

NAOS_TEST(fixture_wide_builds_ok)
{
    text_fixture_wide_t f;
    clopts_result_t *result;

    text_fixture_wide_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_TRUE(f.ok);
    NAOS_CHECK_NOT_NULL(f.naive);
    NAOS_CHECK_NOT_NULL(f.nihongo);
    NAOS_CHECK_NOT_NULL(f.raw);
    NAOS_CHECK_NOT_NULL(f.help);
    NAOS_CHECK_NOT_NULL(f.file);

    /* With no argument (the positional is optional) it parses to OK. */
    result = clopts_parse(f.program, 0, NULL);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_PARSE_OK);
    clopts_result_destroy(result);

    text_fixture_wide_free(&f);
    NAOS_CHECK_NULL(f.program);
}

NAOS_TEST(fixture_wide_names_have_the_intended_bytes)
{
    /* The byte lengths pin the spellings used by the alignment tests. */
    NAOS_CHECK_UINT_EQ(strlen(TEXT_FIXTURE_NAIVE), 6);
    NAOS_CHECK_UINT_EQ(strlen(TEXT_FIXTURE_NIHONGO), 9);
    NAOS_CHECK_UINT_EQ(strlen(TEXT_FIXTURE_ATAI), 3);
    NAOS_CHECK_UINT_EQ(strlen(TEXT_FIXTURE_FILE), 12);
    NAOS_CHECK_MEM_EQ(TEXT_FIXTURE_RAW_VALUE_NAME, strlen(TEXT_FIXTURE_RAW_VALUE_NAME),
                      "\x42\xFF\xC0\xAF\xE6\x97\x45", (size_t)7);
}

/* ---- left column (4.1 to 4.6) ------------------------------------------ */

NAOS_TEST(left_short_and_long_names)
{
    begin();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, "name", 'c');

    test_decl_check(clopts_option_set_help(option, "Desc"));
    solo(option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    CHECK_ROW(option, "  -c, --name  Desc\n");
    end();
}

NAOS_TEST(left_long_name_only_is_indented_by_four_spaces)
{
    begin();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, "name", '\0');

    test_decl_check(clopts_option_set_help(option, "Desc"));
    solo(option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    CHECK_ROW(option, "      --name  Desc\n");
    end();
}

NAOS_TEST(left_short_name_only)
{
    begin();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, NULL, 'c');

    test_decl_check(clopts_option_set_help(option, "Desc"));
    solo(option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    CHECK_ROW(option, "  -c  Desc\n");
    end();
}

NAOS_TEST(left_negatable_flag)
{
    begin();
    clopts_option_t *both = test_decl_option(CLOPTS_OPTION_FLAG, "color", 'c');
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_FLAG, "cache", '\0');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_FLAG, NULL, 'q');

    test_decl_check(clopts_option_set_negatable(both, true));
    test_decl_check(clopts_option_set_help(both, "Desc"));
    test_decl_check(clopts_option_set_negatable(long_only, true));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    test_decl_check(clopts_option_set_negatable(short_only, true));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    solo(both);
    solo(long_only);
    solo(short_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(both, "  -c, --[no-]color  Desc\n");
    CHECK_ROW(long_only, "      --[no-]cache  Desc\n");
    /* Without a long name there is no --no- form: the short name only. */
    CHECK_ROW(short_only, "  -q  Desc\n");
    end();
}

NAOS_TEST(left_count)
{
    begin();
    clopts_option_t *both = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_COUNT, NULL, 'j');
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_COUNT, "debug", '\0');

    test_decl_check(clopts_option_set_help(both, "Desc"));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    solo(both);
    solo(short_only);
    solo(long_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(both, "  -v, --verbose...  Desc\n");
    CHECK_ROW(short_only, "  -j...  Desc\n");
    CHECK_ROW(long_only, "      --debug...  Desc\n");
    end();
}

NAOS_TEST(left_value)
{
    begin();
    clopts_option_t *both = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_VALUE, NULL, 'n');
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');

    test_decl_check(clopts_option_set_value_name(both, "FILE"));
    test_decl_check(clopts_option_set_help(both, "Desc"));
    test_decl_check(clopts_option_set_value_name(short_only, "N"));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    test_decl_check(clopts_option_set_value_name(long_only, "PATH"));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    solo(both);
    solo(short_only);
    solo(long_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(both, "  -o, --output <FILE>  Desc\n");
    CHECK_ROW(short_only, "  -n <N>  Desc\n");
    CHECK_ROW(long_only, "      --config <PATH>  Desc\n");
    end();
}

NAOS_TEST(left_value_name_unset_or_empty_is_VALUE)
{
    begin();
    clopts_option_t *unset = test_decl_option(CLOPTS_OPTION_VALUE, "unset", 'u');
    clopts_option_t *empty = test_decl_option(CLOPTS_OPTION_VALUE, "empty", 'e');
    clopts_option_t *list = test_decl_option(CLOPTS_OPTION_LIST, "list", 'l');

    test_decl_check(clopts_option_set_help(unset, "Desc"));
    test_decl_check(clopts_option_set_value_name(empty, ""));
    test_decl_check(clopts_option_set_help(empty, "Desc"));
    test_decl_check(clopts_option_set_delimiter(list, ','));
    test_decl_check(clopts_option_set_help(list, "Desc"));
    solo(unset);
    solo(empty);
    solo(list);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(unset, "  -u, --unset <VALUE>  Desc\n");
    CHECK_ROW(empty, "  -e, --empty <VALUE>  Desc\n");
    CHECK_ROW(list, "  -l, --list <VALUE>[,<VALUE>...]  Desc\n");
    end();
}

NAOS_TEST(left_list_with_delimiter)
{
    begin();
    clopts_option_t *comma = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');
    clopts_option_t *colon = test_decl_option(CLOPTS_OPTION_LIST, "path", '\0');

    test_decl_check(clopts_option_set_value_name(comma, "KEY"));
    test_decl_check(clopts_option_set_delimiter(comma, ','));
    test_decl_check(clopts_option_set_help(comma, "Desc"));
    test_decl_check(clopts_option_set_value_name(colon, "DIR"));
    test_decl_check(clopts_option_set_delimiter(colon, ':'));
    test_decl_check(clopts_option_set_help(colon, "Desc"));
    solo(comma);
    solo(colon);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(comma, "  -k, --keys <KEY>[,<KEY>...]  Desc\n");
    /* The declared delimiter is written in place of ','. */
    CHECK_ROW(colon, "      --path <DIR>[:<DIR>...]  Desc\n");
    end();
}

NAOS_TEST(left_list_without_delimiter)
{
    begin();
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_LIST, "tag", '\0');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_LIST, NULL, 't');

    test_decl_check(clopts_option_set_value_name(long_only, "TAG"));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    solo(long_only);
    solo(short_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(long_only, "      --tag <TAG>...  Desc\n");
    CHECK_ROW(short_only, "  -t <VALUE>...  Desc\n");
    end();
}

/* ---- notes (4.7 to 4.12) ----------------------------------------------- */

NAOS_TEST(note_required)
{
    begin();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');

    test_decl_check(clopts_option_set_value_name(option, "FILE"));
    test_decl_check(clopts_option_set_help(option, "Write the result to FILE"));
    test_decl_check(clopts_option_set_required(option, true));
    solo(option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(option, "  -o, --output <FILE>  Write the result to FILE (required)\n");
    end();
}

NAOS_TEST(note_default)
{
    begin();
    clopts_option_t *value = test_decl_option(CLOPTS_OPTION_VALUE, "lines", 'n');
    clopts_option_t *list = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');

    test_decl_check(clopts_option_set_value_name(value, "N"));
    test_decl_check(clopts_option_set_help(value, "Number of lines"));
    test_decl_check(clopts_option_set_default(value, "10"));
    test_decl_check(clopts_option_set_value_name(list, "KEY"));
    test_decl_check(clopts_option_set_delimiter(list, ','));
    test_decl_check(clopts_option_set_help(list, "Keys to include"));
    test_decl_check(clopts_option_set_default(list, "a,b"));
    solo(value);
    solo(list);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(value, "  -n, --lines <N>  Number of lines (default: 10)\n");
    /* The default of a LIST is written as declared, not split. */
    CHECK_ROW(list, "  -k, --keys <KEY>[,<KEY>...]  Keys to include (default: a,b)\n");
    end();
}

NAOS_TEST(note_env)
{
    begin();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');

    test_decl_check(clopts_option_set_value_name(option, "FILE"));
    test_decl_check(clopts_option_set_help(option, "Configuration file"));
    test_decl_check(clopts_option_set_env(option, "TOOL_CONFIG"));
    solo(option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(option, "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n");
    end();
}

NAOS_TEST(notes_two_in_fixed_order)
{
    begin();
    clopts_option_t *with_default = test_decl_option(CLOPTS_OPTION_VALUE, "lines", 'n');
    clopts_option_t *with_required = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');

    /* Set in the reverse order: the order of the notes is fixed. */
    test_decl_check(clopts_option_set_env(with_default, "FILT_LINES"));
    test_decl_check(clopts_option_set_default(with_default, "10"));
    test_decl_check(clopts_option_set_value_name(with_default, "N"));
    test_decl_check(clopts_option_set_help(with_default, "Number of lines"));
    test_decl_check(clopts_option_set_env(with_required, "KEYS"));
    test_decl_check(clopts_option_set_required(with_required, true));
    test_decl_check(clopts_option_set_value_name(with_required, "KEY"));
    test_decl_check(clopts_option_set_help(with_required, "Keys"));
    solo(with_default);
    solo(with_required);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(with_default, "  -n, --lines <N>  Number of lines (default: 10) [env: FILT_LINES]\n");
    CHECK_ROW(with_required, "  -k, --keys <KEY>...  Keys (required) [env: KEYS]\n");
    end();
}

NAOS_TEST(no_description_and_no_notes_writes_the_left_column_only)
{
    begin();
    clopts_option_t *unset = test_decl_option(CLOPTS_OPTION_FLAG, "name", 'c');
    clopts_option_t *empty = test_decl_option(CLOPTS_OPTION_VALUE, NULL, 'n');

    /* Empty strings count as unset. */
    test_decl_check(clopts_option_set_value_name(empty, "N"));
    test_decl_check(clopts_option_set_help(empty, ""));
    test_decl_check(clopts_option_set_default(empty, ""));
    solo(unset);
    solo(empty);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(unset, "  -c, --name\n");
    CHECK_ROW(empty, "  -n <N>\n");
    end();
}

NAOS_TEST(no_description_with_notes_starts_the_notes_at_the_description)
{
    begin();
    clopts_option_t *with_default = test_decl_option(CLOPTS_OPTION_VALUE, "lines", 'n');
    clopts_option_t *with_env = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');
    clopts_option_t *with_required = test_decl_option(CLOPTS_OPTION_VALUE, NULL, 'o');

    test_decl_check(clopts_option_set_value_name(with_default, "N"));
    test_decl_check(clopts_option_set_default(with_default, "10"));
    test_decl_check(clopts_option_set_env(with_default, "LINES"));
    test_decl_check(clopts_option_set_value_name(with_env, "FILE"));
    test_decl_check(clopts_option_set_help(with_env, ""));
    test_decl_check(clopts_option_set_env(with_env, "TOOL_CONFIG"));
    test_decl_check(clopts_option_set_value_name(with_required, "FILE"));
    test_decl_check(clopts_option_set_required(with_required, true));
    solo(with_default);
    solo(with_env);
    solo(with_required);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(with_default, "  -n, --lines <N>  (default: 10) [env: LINES]\n");
    CHECK_ROW(with_env, "      --config <FILE>  [env: TOOL_CONFIG]\n");
    CHECK_ROW(with_required, "  -o <FILE>  (required)\n");
    end();
}

/* ---- lists (10.1, 10.2) ------------------------------------------------ */

NAOS_TEST(list_pads_every_row_to_the_widest_left_column)
{
    static const char expected[] =
        "Options:\n"
        "  -v, --verbose...     Print more details\n"
        "  -c, --[no-]color     Colorize the output\n"
        "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
        "  -h, --help           Show this help\n"
        "      --version        Show version information\n";
    text_fixture_tool_t f;

    text_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);
    check_help_has(f.program, expected);
    text_fixture_tool_free(&f);
}

NAOS_TEST(list_aligns_descriptions_in_code_points_with_non_ascii_names)
{
    /*
     * Left columns (code points): "    --<naive>" 11, "-j, --<nihongo> <<atai>>" 13,
     * "    --raw <B?..E>" 19 (every invalid byte is 1), "-h, --help" 10.
     * W = 19, so every description starts after 2 + 19 + 2 = 23 code points.
     */
    static const char expected[] =
        "      --" TEXT_FIXTURE_NAIVE "          Trust every input\n"
        "  -j, --" TEXT_FIXTURE_NIHONGO " <" TEXT_FIXTURE_ATAI ">        Japanese text\n"
        "      --raw <" TEXT_FIXTURE_RAW_VALUE_NAME ">  Raw bytes\n"
        "  -h, --help           Show this help\n";
    static const char *const descriptions[] = {
        "Trust every input", "Japanese text", "Raw bytes", "Show this help"
    };
    text_fixture_wide_t f;
    text_recorder_t rec;
    const char *line;
    size_t byte_starts[4];
    size_t i;

    text_fixture_wide_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);

    text_recorder_init(&rec);
    NAOS_REQUIRE_INT_EQ(clopts_write_help(f.program, NULL, text_recorder_write, &rec), CLOPTS_WRITE_OK);
    line = strstr(rec.buf, expected);
    NAOS_REQUIRE_NOT_NULL(line);

    /* Each description starts at the same code point column. */
    for (i = 0; i < 4; i++)
    {
        const char *end = strchr(line, '\n');
        const char *start = strstr(line, descriptions[i]);

        NAOS_REQUIRE_NOT_NULL(end);
        NAOS_REQUIRE_NOT_NULL(start);
        NAOS_CHECK_TRUE(start < end);
        NAOS_CHECK_UINT_EQ(clopts_utf8_count(line, (size_t)(start - line)), 23);
        byte_starts[i] = (size_t)(start - line);
        line = end + 1;
    }
    /* ... while the byte offsets differ for the non-ASCII rows. */
    NAOS_CHECK_UINT_EQ(byte_starts[3], 23);
    NAOS_CHECK_UINT_EQ(byte_starts[0], 24);
    NAOS_CHECK_UINT_EQ(byte_starts[1], 23 + 6 + 2);

    text_fixture_wide_free(&f);
}

NAOS_SUITE(option_row,
    NAOS_ADD_TEST(fixture_wide_builds_ok),
    NAOS_ADD_TEST(fixture_wide_names_have_the_intended_bytes),
    NAOS_ADD_TEST(left_short_and_long_names),
    NAOS_ADD_TEST(left_long_name_only_is_indented_by_four_spaces),
    NAOS_ADD_TEST(left_short_name_only),
    NAOS_ADD_TEST(left_negatable_flag),
    NAOS_ADD_TEST(left_count),
    NAOS_ADD_TEST(left_value),
    NAOS_ADD_TEST(left_value_name_unset_or_empty_is_VALUE),
    NAOS_ADD_TEST(left_list_with_delimiter),
    NAOS_ADD_TEST(left_list_without_delimiter),
    NAOS_ADD_TEST(note_required),
    NAOS_ADD_TEST(note_default),
    NAOS_ADD_TEST(note_env),
    NAOS_ADD_TEST(notes_two_in_fixed_order),
    NAOS_ADD_TEST(no_description_and_no_notes_writes_the_left_column_only),
    NAOS_ADD_TEST(no_description_with_notes_starts_the_notes_at_the_description),
    NAOS_ADD_TEST(list_pads_every_row_to_the_widest_left_column),
    NAOS_ADD_TEST(list_aligns_descriptions_in_code_points_with_non_ascii_names)
)
