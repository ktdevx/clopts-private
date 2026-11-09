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
 * The rows are written with clopts_help_write_list (help_internal.h), one
 * small declaration per notation; the expected bytes are derived by hand
 * from the rules of the design (HelpLayout).
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include "help/help_internal.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/*
 * A new empty program named "t" for a small declaration, after clearing the
 * failure count of the declaration helpers: a test creates and configures
 * its options, attaches them as global options and then requires
 * test_decl_failures to be 0. The rows read the options through the public
 * read functions only; the test releases the program with
 * test_decl_discard().
 */
static clopts_program_t *new_program(void)
{
    test_decl_reset();
    return test_decl_begin("t");
}

/* Writes the count rows at rows as one list into rec. */
static clopts_write_status_t write_rows(help_recorder_t *rec, const clopts_help_row_t *rows, size_t count)
{
    clopts_help_out_t out;

    help_recorder_init(rec);
    clopts_help_out_init(&out, help_recorder_write, rec);
    clopts_help_write_list(&out, rows, count);
    return clopts_help_out_status(&out);
}

/* A row of option. */
static clopts_help_row_t option_row(const clopts_option_t *option)
{
    clopts_help_row_t row;

    row.option = option;
    row.left = NULL;
    row.help = NULL;
    return row;
}

/* Writes option as a list of one row into rec. */
static clopts_write_status_t write_option(help_recorder_t *rec, const clopts_option_t *option)
{
    clopts_help_row_t row = option_row(option);

    return write_rows(rec, &row, 1);
}

/* A fixed row. */
static clopts_help_row_t fixed_row(const char *left, const char *help)
{
    clopts_help_row_t row;

    row.option = NULL;
    row.left = left;
    row.help = help;
    return row;
}

/* The one-row list of option is exactly expected. */
#define CHECK_ROW(option, expected)                                                \
    do                                                                             \
    {                                                                              \
        static help_recorder_t rec_;                                               \
        NAOS_CHECK_INT_EQ(write_option(&rec_, (option)), CLOPTS_WRITE_OK);         \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, (expected), strlen(expected));       \
    } while (0)

/* ---- fixture ----------------------------------------------------------- */

NAOS_TEST(fixture_wide_builds_ok)
{
    help_fixture_wide_t f;
    clopts_result_t *result;

    help_fixture_wide_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_TRUE(f.ok);
    NAOS_CHECK_NOT_NULL(f.naive);
    NAOS_CHECK_NOT_NULL(f.nihongo);
    NAOS_CHECK_NOT_NULL(f.raw);
    NAOS_CHECK_NOT_NULL(f.help);
    NAOS_CHECK_NOT_NULL(f.file);

    /* With no argument (the positional is optional) it parses to OK. */
    result = clopts_parse(f.program, 0, NULL);
    NAOS_CHECK_INT_EQ(clopts_result_get_status(result), CLOPTS_OK);
    clopts_result_destroy(result);

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

/* ---- left column (4.1 to 4.6) ------------------------------------------ */

NAOS_TEST(left_short_and_long_names)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, "name", 'c');

    test_decl_check(clopts_option_set_help(option, "Desc"));
    option = test_decl_global_attach(program, option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    CHECK_ROW(option, "  -c, --name  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_long_name_only_is_indented_by_four_spaces)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, "name", '\0');

    test_decl_check(clopts_option_set_help(option, "Desc"));
    option = test_decl_global_attach(program, option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    CHECK_ROW(option, "      --name  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_short_name_only)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, NULL, 'c');

    test_decl_check(clopts_option_set_help(option, "Desc"));
    option = test_decl_global_attach(program, option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    CHECK_ROW(option, "  -c  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_negatable_flag)
{
    clopts_program_t *program = new_program();
    clopts_option_t *both = test_decl_option(CLOPTS_OPTION_FLAG, "color", 'c');
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_FLAG, "cache", '\0');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_FLAG, NULL, 'q');

    test_decl_check(clopts_option_set_negatable(both, true));
    test_decl_check(clopts_option_set_help(both, "Desc"));
    test_decl_check(clopts_option_set_negatable(long_only, true));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    test_decl_check(clopts_option_set_negatable(short_only, true));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    both = test_decl_global_attach(program, both);
    long_only = test_decl_global_attach(program, long_only);
    short_only = test_decl_global_attach(program, short_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(both, "  -c, --[no-]color  Desc\n");
    CHECK_ROW(long_only, "      --[no-]cache  Desc\n");
    /* Without a long name there is no --no- form: the short name only. */
    CHECK_ROW(short_only, "  -q  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_count)
{
    clopts_program_t *program = new_program();
    clopts_option_t *both = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_COUNT, NULL, 'j');
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_COUNT, "debug", '\0');

    test_decl_check(clopts_option_set_help(both, "Desc"));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    both = test_decl_global_attach(program, both);
    short_only = test_decl_global_attach(program, short_only);
    long_only = test_decl_global_attach(program, long_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(both, "  -v, --verbose...  Desc\n");
    CHECK_ROW(short_only, "  -j...  Desc\n");
    CHECK_ROW(long_only, "      --debug...  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_value)
{
    clopts_program_t *program = new_program();
    clopts_option_t *both = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_VALUE, NULL, 'n');
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');

    test_decl_check(clopts_option_set_value_name(both, "FILE"));
    test_decl_check(clopts_option_set_help(both, "Desc"));
    test_decl_check(clopts_option_set_value_name(short_only, "N"));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    test_decl_check(clopts_option_set_value_name(long_only, "PATH"));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    both = test_decl_global_attach(program, both);
    short_only = test_decl_global_attach(program, short_only);
    long_only = test_decl_global_attach(program, long_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(both, "  -o, --output <FILE>  Desc\n");
    CHECK_ROW(short_only, "  -n <N>  Desc\n");
    CHECK_ROW(long_only, "      --config <PATH>  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_value_name_unset_or_empty_is_VALUE)
{
    clopts_program_t *program = new_program();
    clopts_option_t *unset = test_decl_option(CLOPTS_OPTION_VALUE, "unset", 'u');
    clopts_option_t *empty = test_decl_option(CLOPTS_OPTION_VALUE, "empty", 'e');
    clopts_option_t *list = test_decl_option(CLOPTS_OPTION_LIST, "list", 'l');

    test_decl_check(clopts_option_set_help(unset, "Desc"));
    test_decl_check(clopts_option_set_value_name(empty, ""));
    test_decl_check(clopts_option_set_help(empty, "Desc"));
    test_decl_check(clopts_option_set_delimiter(list, ','));
    test_decl_check(clopts_option_set_help(list, "Desc"));
    unset = test_decl_global_attach(program, unset);
    empty = test_decl_global_attach(program, empty);
    list = test_decl_global_attach(program, list);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(unset, "  -u, --unset <VALUE>  Desc\n");
    CHECK_ROW(empty, "  -e, --empty <VALUE>  Desc\n");
    CHECK_ROW(list, "  -l, --list <VALUE>[,<VALUE>...]  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_list_with_delimiter)
{
    clopts_program_t *program = new_program();
    clopts_option_t *comma = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');
    clopts_option_t *colon = test_decl_option(CLOPTS_OPTION_LIST, "path", '\0');

    test_decl_check(clopts_option_set_value_name(comma, "KEY"));
    test_decl_check(clopts_option_set_delimiter(comma, ','));
    test_decl_check(clopts_option_set_help(comma, "Desc"));
    test_decl_check(clopts_option_set_value_name(colon, "DIR"));
    test_decl_check(clopts_option_set_delimiter(colon, ':'));
    test_decl_check(clopts_option_set_help(colon, "Desc"));
    comma = test_decl_global_attach(program, comma);
    colon = test_decl_global_attach(program, colon);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(comma, "  -k, --keys <KEY>[,<KEY>...]  Desc\n");
    /* The declared delimiter is written in place of ','. */
    CHECK_ROW(colon, "      --path <DIR>[:<DIR>...]  Desc\n");
    test_decl_discard(program);
}

NAOS_TEST(left_list_without_delimiter)
{
    clopts_program_t *program = new_program();
    clopts_option_t *long_only = test_decl_option(CLOPTS_OPTION_LIST, "tag", '\0');
    clopts_option_t *short_only = test_decl_option(CLOPTS_OPTION_LIST, NULL, 't');

    test_decl_check(clopts_option_set_value_name(long_only, "TAG"));
    test_decl_check(clopts_option_set_help(long_only, "Desc"));
    test_decl_check(clopts_option_set_help(short_only, "Desc"));
    long_only = test_decl_global_attach(program, long_only);
    short_only = test_decl_global_attach(program, short_only);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(long_only, "      --tag <TAG>...  Desc\n");
    CHECK_ROW(short_only, "  -t <VALUE>...  Desc\n");
    test_decl_discard(program);
}

/* ---- notes (4.7 to 4.12) ----------------------------------------------- */

NAOS_TEST(note_required)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');

    test_decl_check(clopts_option_set_value_name(option, "FILE"));
    test_decl_check(clopts_option_set_help(option, "Write the result to FILE"));
    test_decl_check(clopts_option_set_required(option, true));
    option = test_decl_global_attach(program, option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(option, "  -o, --output <FILE>  Write the result to FILE (required)\n");
    test_decl_discard(program);
}

NAOS_TEST(note_default)
{
    clopts_program_t *program = new_program();
    clopts_option_t *value = test_decl_option(CLOPTS_OPTION_VALUE, "lines", 'n');
    clopts_option_t *list = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');

    test_decl_check(clopts_option_set_value_name(value, "N"));
    test_decl_check(clopts_option_set_help(value, "Number of lines"));
    test_decl_check(clopts_option_set_default(value, "10"));
    test_decl_check(clopts_option_set_value_name(list, "KEY"));
    test_decl_check(clopts_option_set_delimiter(list, ','));
    test_decl_check(clopts_option_set_help(list, "Keys to include"));
    test_decl_check(clopts_option_set_default(list, "a,b"));
    value = test_decl_global_attach(program, value);
    list = test_decl_global_attach(program, list);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(value, "  -n, --lines <N>  Number of lines (default: 10)\n");
    /* The default of a LIST is written as declared, not split. */
    CHECK_ROW(list, "  -k, --keys <KEY>[,<KEY>...]  Keys to include (default: a,b)\n");
    test_decl_discard(program);
}

NAOS_TEST(note_env)
{
    clopts_program_t *program = new_program();
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');

    test_decl_check(clopts_option_set_value_name(option, "FILE"));
    test_decl_check(clopts_option_set_help(option, "Configuration file"));
    test_decl_check(clopts_option_set_env(option, "TOOL_CONFIG"));
    option = test_decl_global_attach(program, option);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(option, "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n");
    test_decl_discard(program);
}

NAOS_TEST(notes_two_in_fixed_order)
{
    clopts_program_t *program = new_program();
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
    with_default = test_decl_global_attach(program, with_default);
    with_required = test_decl_global_attach(program, with_required);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(with_default, "  -n, --lines <N>  Number of lines (default: 10) [env: FILT_LINES]\n");
    CHECK_ROW(with_required, "  -k, --keys <KEY>...  Keys (required) [env: KEYS]\n");
    test_decl_discard(program);
}

NAOS_TEST(no_description_and_no_notes_writes_the_left_column_only)
{
    clopts_program_t *program = new_program();
    clopts_option_t *unset = test_decl_option(CLOPTS_OPTION_FLAG, "name", 'c');
    clopts_option_t *empty = test_decl_option(CLOPTS_OPTION_VALUE, NULL, 'n');

    /* Empty strings count as unset. */
    test_decl_check(clopts_option_set_value_name(empty, "N"));
    test_decl_check(clopts_option_set_help(empty, ""));
    test_decl_check(clopts_option_set_default(empty, ""));
    unset = test_decl_global_attach(program, unset);
    empty = test_decl_global_attach(program, empty);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(unset, "  -c, --name\n");
    CHECK_ROW(empty, "  -n <N>\n");
    test_decl_discard(program);
}

NAOS_TEST(no_description_with_notes_starts_the_notes_at_the_description)
{
    clopts_program_t *program = new_program();
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
    with_default = test_decl_global_attach(program, with_default);
    with_env = test_decl_global_attach(program, with_env);
    with_required = test_decl_global_attach(program, with_required);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);

    CHECK_ROW(with_default, "  -n, --lines <N>  (default: 10) [env: LINES]\n");
    CHECK_ROW(with_env, "      --config <FILE>  [env: TOOL_CONFIG]\n");
    CHECK_ROW(with_required, "  -o <FILE>  (required)\n");
    test_decl_discard(program);
}

/* ---- lists (10.1, 10.2) ------------------------------------------------ */

NAOS_TEST(list_of_no_rows_writes_nothing)
{
    help_recorder_t rec;

    NAOS_CHECK_INT_EQ(write_rows(&rec, NULL, 0), CLOPTS_WRITE_OK);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
}

NAOS_TEST(list_pads_every_row_to_the_widest_left_column)
{
    static const char expected[] =
        "  -v, --verbose...     Print more details\n"
        "  -c, --[no-]color     Colorize the output\n"
        "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
        "  -h, --help           Show this help\n"
        "      --version        Show version information\n";
    help_fixture_tool_t f;
    help_recorder_t rec;
    clopts_help_row_t rows[5];

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);

    rows[0] = option_row(f.verbose);
    rows[1] = option_row(f.color);
    rows[2] = option_row(f.config);
    rows[3] = option_row(f.help);
    rows[4] = option_row(f.version);
    NAOS_CHECK_INT_EQ(write_rows(&rec, rows, 5), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));

    help_fixture_tool_free(&f);
}

NAOS_TEST(list_aligns_each_list_on_its_own_widest_left_column)
{
    static const char expected[] =
        "  -o, --output <FILE>          Write the result to FILE (required)\n"
        "  -k, --keys <KEY>[,<KEY>...]  Keys to include (default: a,b)\n"
        "  -h, --help                   Show this help\n"
        "  -x\n";
    help_fixture_tool_t f;
    help_recorder_t rec;
    clopts_help_row_t rows[4];

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);

    rows[0] = option_row(f.output);
    rows[1] = option_row(f.keys);
    rows[2] = fixed_row("-h, --help", "Show this help");
    /* A fixed row without a description: no padding, no trailing space. */
    rows[3] = fixed_row("-x", NULL);
    NAOS_CHECK_INT_EQ(write_rows(&rec, rows, 4), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));

    help_fixture_tool_free(&f);
}

NAOS_TEST(list_aligns_descriptions_in_code_points_with_non_ascii_names)
{
    /*
     * Left columns (code points): "    --<naive>" 11, "-j, --<nihongo> <<atai>>" 13,
     * "    --raw <B?..E>" 19 (every invalid byte is 1), "-h, --help" 10.
     * W = 19, so every description starts after 2 + 19 + 2 = 23 code points.
     */
    static const char expected[] =
        "      --" HELP_FIXTURE_NAIVE "          Trust every input\n"
        "  -j, --" HELP_FIXTURE_NIHONGO " <" HELP_FIXTURE_ATAI ">        Japanese text\n"
        "      --raw <" HELP_FIXTURE_RAW_VALUE_NAME ">  Raw bytes\n"
        "  -h, --help           Show this help\n";
    static const char *const descriptions[] = {
        "Trust every input", "Japanese text", "Raw bytes", "Show this help"
    };
    help_fixture_wide_t f;
    help_recorder_t rec;
    clopts_help_row_t rows[4];
    const char *line;
    size_t byte_starts[4];
    size_t i;

    help_fixture_wide_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_REQUIRE_TRUE(f.ok);

    rows[0] = option_row(f.naive);
    rows[1] = option_row(f.nihongo);
    rows[2] = option_row(f.raw);
    rows[3] = option_row(f.help);
    NAOS_CHECK_INT_EQ(write_rows(&rec, rows, 4), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, expected, strlen(expected));

    /* Each description starts at the same code point column. */
    line = rec.buf;
    for (i = 0; i < 4; i++)
    {
        const char *end = strchr(line, '\n');
        const char *start = strstr(line, descriptions[i]);

        NAOS_REQUIRE_NOT_NULL(end);
        NAOS_REQUIRE_NOT_NULL(start);
        NAOS_CHECK_TRUE(start < end);
        NAOS_CHECK_UINT_EQ(clopts_help_width(line, (size_t)(start - line)), 23);
        byte_starts[i] = (size_t)(start - line);
        line = end + 1;
    }
    /* ... while the byte offsets differ for the non-ASCII rows. */
    NAOS_CHECK_UINT_EQ(byte_starts[3], 23);
    NAOS_CHECK_UINT_EQ(byte_starts[0], 24);
    NAOS_CHECK_UINT_EQ(byte_starts[1], 23 + 6 + 2);

    help_fixture_wide_free(&f);
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
    NAOS_ADD_TEST(list_of_no_rows_writes_nothing),
    NAOS_ADD_TEST(list_pads_every_row_to_the_widest_left_column),
    NAOS_ADD_TEST(list_aligns_each_list_on_its_own_widest_left_column),
    NAOS_ADD_TEST(list_aligns_descriptions_in_code_points_with_non_ascii_names)
)
