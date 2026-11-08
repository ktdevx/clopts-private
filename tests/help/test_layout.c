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

/* ---- Usage line -------------------------------------------------------- */

/* The Usage line of command (NULL: the program) of program is exactly expected. */
#define CHECK_USAGE(program, command, expected)                                                   \
    do                                                                                            \
    {                                                                                             \
        static help_recorder_t rec_;                                                              \
        help_recorder_init(&rec_);                                                                \
        NAOS_CHECK_INT_EQ(clopts_write_usage((program), (command), help_recorder_write, &rec_),   \
                          CLOPTS_WRITE_OK);                                                       \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, (expected), strlen(expected));                      \
    } while (0)

NAOS_TEST(usage_global)
{
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_USAGE(f.program, NULL, "Usage: tool [<options>] <command> [<arguments>]\n");
    CHECK_USAGE(f.program, f.root, "Usage: tool [<options>] <command> [<arguments>]\n");
    help_fixture_tool_free(&f);
}

NAOS_TEST(usage_subcommand)
{
    help_fixture_tool_t f;
    layout_nover_t g;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_USAGE(f.program, f.build, "Usage: tool build [<options>] <SRC> [<EXTRA>...]\n");
    CHECK_USAGE(f.program, f.copy, "Usage: tool copy [<options>] <SRC> <DST>\n");
    help_fixture_tool_free(&f);

    layout_nover_init(&g);
    NAOS_REQUIRE_NOT_NULL(g.program);
    CHECK_USAGE(g.program, g.init, "Usage: nover init [<options>] [<NAME>]\n");
    clopts_program_free(g.program);
}

NAOS_TEST(usage_without_subcommands)
{
    help_fixture_filt_t f;
    help_fixture_bare_t b;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_USAGE(f.program, NULL, "Usage: filt [<options>] <FILE>...\n");
    CHECK_USAGE(f.program, f.root, "Usage: filt [<options>] <FILE>...\n");
    help_fixture_filt_free(&f);

    help_fixture_bare_init(&b);
    NAOS_REQUIRE_NOT_NULL(b.program);
    CHECK_USAGE(b.program, b.run, "Usage: bare run [<options>]\n");
    help_fixture_bare_free(&b);
}

NAOS_TEST(usage_is_the_first_line_of_the_help)
{
    static help_recorder_t help;
    static help_recorder_t usage;
    help_fixture_tool_t f;
    const clopts_command_t *commands[3];
    size_t i;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    commands[0] = NULL;
    commands[1] = f.build;
    commands[2] = f.copy;
    for (i = 0; i < 3; i++)
    {
        const char *newline;

        NAOS_REQUIRE_INT_EQ(write_help(&help, f.program, commands[i]), CLOPTS_WRITE_OK);
        help_recorder_init(&usage);
        NAOS_REQUIRE_INT_EQ(clopts_write_usage(f.program, commands[i], help_recorder_write, &usage),
                            CLOPTS_WRITE_OK);
        newline = strchr(help.buf, '\n');
        NAOS_REQUIRE_NOT_NULL(newline);
        NAOS_CHECK_MEM_EQ(usage.buf, usage.len, help.buf, (size_t)(newline - help.buf) + 1);
    }
    help_fixture_tool_free(&f);
}

/* ---- version line and usage hint --------------------------------------- */

NAOS_TEST(version_line)
{
    static help_recorder_t rec;
    help_fixture_tool_t f;
    help_fixture_bare_t b;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(f.program, help_recorder_write, &rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "tool 1.2.3\n", strlen("tool 1.2.3\n"));
    help_fixture_tool_free(&f);

    help_fixture_bare_init(&b);
    NAOS_REQUIRE_NOT_NULL(b.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(b.program, help_recorder_write, &rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_MEM_EQ(rec.buf, rec.len, "bare 0.1\n", strlen("bare 0.1\n"));
    help_fixture_bare_free(&b);
}

NAOS_TEST(version_line_unavailable_without_version)
{
    static help_recorder_t rec;
    help_fixture_filt_t f;
    layout_nover_t g;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(f.program, help_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    help_fixture_filt_free(&f);

    layout_nover_init(&g);
    NAOS_REQUIRE_NOT_NULL(g.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(g.program, help_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    clopts_program_free(g.program);
}

/* The usage hint of command (NULL: the program) of program is exactly expected. */
#define CHECK_HINT(program, command, expected)                                                       \
    do                                                                                               \
    {                                                                                                \
        static help_recorder_t rec_;                                                                 \
        help_recorder_init(&rec_);                                                                   \
        NAOS_CHECK_INT_EQ(clopts_write_usage_hint((program), (command), help_recorder_write, &rec_), \
                          CLOPTS_WRITE_OK);                                                          \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, (expected), strlen(expected));                         \
    } while (0)

NAOS_TEST(usage_hint_without_command)
{
    help_fixture_tool_t f;
    help_fixture_filt_t g;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HINT(f.program, NULL, "Run 'tool --help' for usage.\n");
    CHECK_HINT(f.program, f.root, "Run 'tool --help' for usage.\n");
    help_fixture_tool_free(&f);

    help_fixture_filt_init(&g);
    NAOS_REQUIRE_NOT_NULL(g.program);
    CHECK_HINT(g.program, NULL, "Run 'filt --help' for usage.\n");
    help_fixture_filt_free(&g);
}

NAOS_TEST(usage_hint_with_command)
{
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HINT(f.program, f.build, "Run 'tool build --help' for usage.\n");
    CHECK_HINT(f.program, f.copy, "Run 'tool copy --help' for usage.\n");
    help_fixture_tool_free(&f);
}

/* ---- programs with a recorded error ------------------------------------ */

/*
 * The 4 help functions return UNAVAILABLE for program (command: a handle of
 * it, or NULL) without calling the callback.
 */
static void check_all_unavailable(const clopts_program_t *program, const clopts_command_t *command)
{
    static help_recorder_t rec;

    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_help(program, command, help_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_INT_EQ(clopts_write_usage(program, command, help_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_INT_EQ(clopts_write_version(program, help_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_INT_EQ(clopts_write_usage_hint(program, command, help_recorder_write, &rec),
                      CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
}

/* program has a declaration error recorded. */
static void require_decl_error(clopts_program_t *program)
{
    clopts_result_t *result = clopts_program_validate(program);

    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_DECL_ERROR);
    clopts_result_free(result);
}

NAOS_TEST(error_program_with_an_invalid_name_is_unavailable)
{
    clopts_program_t *program = clopts_program_new("bad name");

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_set_version(program, "1.0");
    check_all_unavailable(program, NULL);
    check_all_unavailable(program, clopts_program_root(program));
    require_decl_error(program);
    clopts_program_free(program);
}

NAOS_TEST(error_program_without_a_name_is_unavailable)
{
    clopts_program_t *program = clopts_program_new(NULL);

    NAOS_REQUIRE_NOT_NULL(program);
    check_all_unavailable(program, NULL);
    require_decl_error(program);
    clopts_program_free(program);
}

NAOS_TEST(error_program_with_a_later_error_is_unavailable)
{
    clopts_program_t *program = clopts_program_new("tool");
    clopts_command_t *run;

    NAOS_REQUIRE_NOT_NULL(program);
    clopts_program_set_version(program, "1.0");
    run = clopts_program_add_command(program, "run");
    NAOS_REQUIRE_NOT_NULL(run);
    (void)clopts_command_add_flag(run, "bad name", '\0');
    check_all_unavailable(program, NULL);
    check_all_unavailable(program, run);
    require_decl_error(program);
    clopts_program_free(program);
}

/* ---- determinism ------------------------------------------------------- */

/* Every text of command (NULL: the program) of program, concatenated into rec. */
static void write_all(help_recorder_t *rec, const clopts_program_t *program, const clopts_command_t *command)
{
    help_recorder_init(rec);
    NAOS_CHECK_INT_EQ(clopts_write_help(program, command, help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_INT_EQ(clopts_write_usage(program, command, help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_INT_EQ(clopts_write_version(program, help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_INT_EQ(clopts_write_usage_hint(program, command, help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_FALSE(rec->overflow);
}

NAOS_TEST(generation_is_repeatable_and_keeps_the_declaration)
{
    static help_recorder_t first;
    static help_recorder_t second;
    help_fixture_tool_t f;
    const clopts_command_t *commands[3];
    clopts_result_t *result;
    size_t i;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    commands[0] = NULL;
    commands[1] = f.build;
    commands[2] = f.copy;
    for (i = 0; i < 3; i++)
    {
        write_all(&first, f.program, commands[i]);
        write_all(&second, f.program, commands[i]);
        NAOS_CHECK_MEM_EQ(second.buf, second.len, first.buf, first.len);
    }
    /* The declaration is still the same valid one, and its help unchanged. */
    result = clopts_program_validate(f.program);
    NAOS_CHECK_INT_EQ(clopts_result_status(result), CLOPTS_OK);
    clopts_result_free(result);
    CHECK_HELP(f.program, NULL, tool_help);
    help_fixture_tool_free(&f);
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
    NAOS_ADD_TEST(help_subcommand_without_text_or_globals),
    NAOS_ADD_TEST(usage_global),
    NAOS_ADD_TEST(usage_subcommand),
    NAOS_ADD_TEST(usage_without_subcommands),
    NAOS_ADD_TEST(usage_is_the_first_line_of_the_help),
    NAOS_ADD_TEST(version_line),
    NAOS_ADD_TEST(version_line_unavailable_without_version),
    NAOS_ADD_TEST(usage_hint_without_command),
    NAOS_ADD_TEST(usage_hint_with_command),
    NAOS_ADD_TEST(error_program_with_an_invalid_name_is_unavailable),
    NAOS_ADD_TEST(error_program_without_a_name_is_unavailable),
    NAOS_ADD_TEST(error_program_with_a_later_error_is_unavailable),
    NAOS_ADD_TEST(generation_is_repeatable_and_keeps_the_declaration)
)
