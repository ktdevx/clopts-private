/*
 * Suite layout: the whole help texts, the Usage line, the version line and
 * the usage hint, compared byte for byte.
 *
 * It also checks that the fixtures it uses (tool, filt, bare) are valid
 * declarations. The expected texts are derived by hand from the rules of the
 * design (HelpLayout): 2 spaces of indentation, descriptions at column
 * 2 + W + 2 where W is the widest left column of the list.
 */

#include "test_fixtures.h"

#include <naos/naos.h>

#include <string.h>

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

/* ---- helpers ----------------------------------------------------------- */

/* Writes the help of command (NULL: the program) of program into rec. */
static clopts_write_status_t write_help(help_recorder_t *rec, const clopts_program_t *program,
                                        const clopts_command_t *command)
{
    help_recorder_init(rec);
    return clopts_write_help(program, command, help_recorder_write, rec);
}

/* The help of command (NULL: the program) of program is exactly expected. */
#define CHECK_HELP(program, command, expected)                                       \
    do                                                                               \
    {                                                                                \
        static help_recorder_t rec_;                                                 \
        NAOS_CHECK_INT_EQ(write_help(&rec_, (program), (command)), CLOPTS_WRITE_OK); \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, (expected), strlen(expected));         \
    } while (0)

/*
 * nover: with subcommands, no version, no global option, no description.
 *   init: no summary, no description, NAME optional (not variadic)
 */
typedef struct layout_nover
{
    clopts_program_t *program;
    clopts_command_t *init;
    clopts_positional_t *name;
} layout_nover_t;

static void layout_nover_init(layout_nover_t *f)
{
    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("nover");
    if (f->program == NULL)
    {
        return;
    }
    f->init = clopts_program_add_command(f->program, "init");
    f->name = clopts_command_add_positional(f->init, "NAME");
    clopts_positional_set_help(f->name, "Name to use");
}

/* ---- help -------------------------------------------------------------- */

static const char tool_help[] =
    "Usage: tool [<options>] <command> [<arguments>]\n"
    "\n"
    "A sample tool for testing.\n"
    "\n"
    "Commands:\n"
    "  build  Build an output from a source\n"
    "  copy   Copy a file\n"
    "  help   Show help for a command\n"
    "\n"
    "Options:\n"
    "  -v, --verbose...     Print more details\n"
    "  -c, --[no-]color     Colorize the output\n"
    "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
    "  -h, --help           Show this help\n"
    "      --version        Show version information\n"
    "\n"
    "Run 'tool <command> --help' for help on a command.\n";

NAOS_TEST(help_tool_global)
{
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, NULL, tool_help);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_tool_root_is_the_global_help)
{
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.root, tool_help);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_tool_build)
{
    static const char expected[] =
        "Usage: tool build [<options>] <SRC> [<EXTRA>...]\n"
        "\n"
        "Build an output from SRC.\n"
        "\n"
        "Arguments:\n"
        "  <SRC>    Source file\n"
        "  <EXTRA>  Extra inputs\n"
        "\n"
        "Options:\n"
        "  -o, --output <FILE>          Write the result to FILE (required)\n"
        "  -k, --keys <KEY>[,<KEY>...]  Keys to include (default: a,b)\n"
        "  -h, --help                   Show this help\n"
        "\n"
        "Global Options:\n"
        "  -v, --verbose...     Print more details\n"
        "  -c, --[no-]color     Colorize the output\n"
        "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
        "      --version        Show version information\n";
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.build, expected);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_tool_copy_uses_the_summary)
{
    static const char expected[] =
        "Usage: tool copy [<options>] <SRC> <DST>\n"
        "\n"
        "Copy a file\n"
        "\n"
        "Arguments:\n"
        "  <SRC>  File to copy\n"
        "  <DST>  Destination\n"
        "\n"
        "Options:\n"
        "  -h, --help  Show this help\n"
        "\n"
        "Global Options:\n"
        "  -v, --verbose...     Print more details\n"
        "  -c, --[no-]color     Colorize the output\n"
        "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
        "      --version        Show version information\n";
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.copy, expected);
    help_fixture_tool_free(&f);
}

static const char filt_help[] =
    "Usage: filt [<options>] <FILE>...\n"
    "\n"
    "Filter the lines of files.\n"
    "\n"
    "Arguments:\n"
    "  <FILE>  Files to filter\n"
    "\n"
    "Options:\n"
    "  -n, --lines <N>  Number of lines (default: 10) [env: FILT_LINES]\n"
    "  -h, --help       Show this help\n";

NAOS_TEST(help_filt_without_subcommands)
{
    help_fixture_filt_t f;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, NULL, filt_help);
    help_fixture_filt_free(&f);
}

NAOS_TEST(help_filt_root_is_the_program_help)
{
    help_fixture_filt_t f;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.root, filt_help);
    help_fixture_filt_free(&f);
}

NAOS_TEST(help_bare_run_has_only_version_as_global)
{
    static const char expected[] =
        "Usage: bare run [<options>]\n"
        "\n"
        "Run the task\n"
        "\n"
        "Options:\n"
        "  -h, --help  Show this help\n"
        "\n"
        "Global Options:\n"
        "      --version  Show version information\n";
    help_fixture_bare_t f;

    help_fixture_bare_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.run, expected);
    help_fixture_bare_free(&f);
}

NAOS_TEST(help_bare_global_without_description)
{
    static const char expected[] =
        "Usage: bare [<options>] <command> [<arguments>]\n"
        "\n"
        "Commands:\n"
        "  run   Run the task\n"
        "  help  Show help for a command\n"
        "\n"
        "Options:\n"
        "  -h, --help     Show this help\n"
        "      --version  Show version information\n"
        "\n"
        "Run 'bare <command> --help' for help on a command.\n";
    help_fixture_bare_t f;

    help_fixture_bare_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, NULL, expected);
    help_fixture_bare_free(&f);
}

NAOS_TEST(help_without_version_has_no_version_row)
{
    static const char expected[] =
        "Usage: nover [<options>] <command> [<arguments>]\n"
        "\n"
        "Commands:\n"
        "  init\n"
        "  help  Show help for a command\n"
        "\n"
        "Options:\n"
        "  -h, --help  Show this help\n"
        "\n"
        "Run 'nover <command> --help' for help on a command.\n";
    static help_recorder_t rec;
    layout_nover_t f;

    layout_nover_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, NULL, expected);
    NAOS_CHECK_INT_EQ(write_help(&rec, f.program, NULL), CLOPTS_WRITE_OK);
    NAOS_CHECK_NULL(strstr(rec.buf, "--version"));
    clopts_program_free(f.program);
}

NAOS_TEST(help_subcommand_without_text_or_globals)
{
    static const char expected[] =
        "Usage: nover init [<options>] [<NAME>]\n"
        "\n"
        "Arguments:\n"
        "  <NAME>  Name to use\n"
        "\n"
        "Options:\n"
        "  -h, --help  Show this help\n";
    layout_nover_t f;

    layout_nover_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.init, expected);
    clopts_program_free(f.program);
}

NAOS_SUITE(layout,
    NAOS_ADD_TEST(fixture_tool_validates_ok),
    NAOS_ADD_TEST(fixture_filt_validates_ok),
    NAOS_ADD_TEST(fixture_bare_validates_ok),
    NAOS_ADD_TEST(fixture_free_accepts_a_cleared_struct),
    NAOS_ADD_TEST(help_tool_global),
    NAOS_ADD_TEST(help_tool_root_is_the_global_help),
    NAOS_ADD_TEST(help_tool_build),
    NAOS_ADD_TEST(help_tool_copy_uses_the_summary),
    NAOS_ADD_TEST(help_filt_without_subcommands),
    NAOS_ADD_TEST(help_filt_root_is_the_program_help),
    NAOS_ADD_TEST(help_bare_run_has_only_version_as_global),
    NAOS_ADD_TEST(help_bare_global_without_description),
    NAOS_ADD_TEST(help_without_version_has_no_version_row),
    NAOS_ADD_TEST(help_subcommand_without_text_or_globals)
)
