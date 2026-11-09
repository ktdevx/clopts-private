/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite layout: the whole help texts, the Usage line, the version line and
 * the usage hint, compared byte for byte; the filter of the shadowed global
 * options; and the declarations for the help texts
 * (clopts_help_add_help_option(), clopts_help_add_version_option(),
 * clopts_help_command_create() and clopts_help_target()).
 *
 * It also checks that the fixtures it uses (tool, filt, bare) are built
 * without a declaration error. Nothing is reserved: -h/--help, --version
 * and the help command of the fixtures are ordinary declarations, listed
 * where they are declared. The expected texts are derived by hand from
 * the rules of the design (HelpLayout): 2 spaces of indentation,
 * descriptions at column 2 + W + 2 where W is the widest left column of the
 * list.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include <naos/naos.h>

#include <string.h>

NAOS_TEST(fixture_tool_builds_ok)
{
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_TRUE(f.ok);
    NAOS_CHECK_NOT_NULL(f.build);
    NAOS_CHECK_NOT_NULL(f.copy);
    NAOS_CHECK_NOT_NULL(f.help_command);
    NAOS_CHECK_NOT_NULL(f.help);
    NAOS_CHECK_NOT_NULL(f.version);
    NAOS_CHECK_NOT_NULL(f.verbose);
    NAOS_CHECK_NOT_NULL(f.color);
    NAOS_CHECK_NOT_NULL(f.config);
    NAOS_CHECK_NOT_NULL(f.output);
    NAOS_CHECK_NOT_NULL(f.keys);
    NAOS_CHECK_NOT_NULL(f.build_src);
    NAOS_CHECK_NOT_NULL(f.build_extra);
    NAOS_CHECK_NOT_NULL(f.copy_src);
    NAOS_CHECK_NOT_NULL(f.copy_dst);

    help_fixture_tool_free(&f);
    NAOS_CHECK_NULL(f.program);
}

NAOS_TEST(fixture_filt_builds_ok)
{
    help_fixture_filt_t f;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_TRUE(f.ok);
    NAOS_CHECK_NOT_NULL(f.lines);
    NAOS_CHECK_NOT_NULL(f.help);
    NAOS_CHECK_NOT_NULL(f.file);

    help_fixture_filt_free(&f);
    NAOS_CHECK_NULL(f.program);
}

NAOS_TEST(fixture_bare_builds_ok)
{
    help_fixture_bare_t f;

    help_fixture_bare_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_TRUE(f.ok);
    NAOS_CHECK_NOT_NULL(f.run);
    NAOS_CHECK_NOT_NULL(f.help_command);
    NAOS_CHECK_NOT_NULL(f.help);
    NAOS_CHECK_NOT_NULL(f.version);

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
 * nover: with one subcommand, no global option (so no --help and no
 * --version), no help command, no description.
 *   init: no summary, no description, no option, NAME optional (not variadic)
 * The handles are NULL when the program could not be made.
 */
typedef struct layout_nover
{
    clopts_program_t *program;
    clopts_command_t *init;
    clopts_positional_t *name;
} layout_nover_t;

static void layout_nover_init(layout_nover_t *f)
{
    clopts_command_t *root;

    memset(f, 0, sizeof(*f));
    root = test_decl_root("nover");
    f->init = test_decl_command(root, "init");
    f->name = test_decl_positional("NAME");
    test_decl_check(clopts_positional_set_help(f->name, "Name to use"));
    f->name = test_decl_attach_positional(f->init, f->name);
    f->program = test_decl_finish(root);
    if (f->program == NULL)
    {
        memset(f, 0, sizeof(*f));
    }
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
        "\n"
        "Global Options:\n"
        "  -v, --verbose...     Print more details\n"
        "  -c, --[no-]color     Colorize the output\n"
        "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
        "  -h, --help           Show this help\n"
        "      --version        Show version information\n";
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.build, expected);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_tool_copy_uses_the_summary)
{
    /* copy has no option of its own: the Options: paragraph is left out. */
    static const char expected[] =
        "Usage: tool copy [<options>] <SRC> <DST>\n"
        "\n"
        "Copy a file\n"
        "\n"
        "Arguments:\n"
        "  <SRC>  File to copy\n"
        "  <DST>  Destination\n"
        "\n"
        "Global Options:\n"
        "  -v, --verbose...     Print more details\n"
        "  -c, --[no-]color     Colorize the output\n"
        "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
        "  -h, --help           Show this help\n"
        "      --version        Show version information\n";
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.copy, expected);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_tool_help_command)
{
    /* The help command of clopts_help_command_create() has a help like any other subcommand. */
    static const char expected[] =
        "Usage: tool help [<options>] [<COMMAND>]\n"
        "\n"
        "Show help for a command\n"
        "\n"
        "Arguments:\n"
        "  <COMMAND>  Command to show help for\n"
        "\n"
        "Global Options:\n"
        "  -v, --verbose...     Print more details\n"
        "  -c, --[no-]color     Colorize the output\n"
        "      --config <FILE>  Configuration file [env: TOOL_CONFIG]\n"
        "  -h, --help           Show this help\n"
        "      --version        Show version information\n";
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.help_command, expected);
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

NAOS_TEST(help_bare_run_has_only_the_global_options)
{
    /* run has no option and no positional: neither Arguments: nor Options: is written. */
    static const char expected[] =
        "Usage: bare run [<options>]\n"
        "\n"
        "Run the task\n"
        "\n"
        "Global Options:\n"
        "  -h, --help     Show this help\n"
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

NAOS_TEST(help_without_root_options_has_no_options_and_no_run_line)
{
    /*
     * No fixed row: without a declared --help, --version or help command
     * none of them is listed, the empty Options: paragraph is left out and
     * the closing Run line (which points to --help) is not written.
     */
    static const char expected[] =
        "Usage: nover [<options>] <command> [<arguments>]\n"
        "\n"
        "Commands:\n"
        "  init\n";
    static help_recorder_t rec;
    layout_nover_t f;

    layout_nover_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, NULL, expected);
    NAOS_CHECK_INT_EQ(write_help(&rec, f.program, NULL), CLOPTS_WRITE_OK);
    NAOS_CHECK_NULL(strstr(rec.buf, "--version"));
    NAOS_CHECK_NULL(strstr(rec.buf, "--help"));
    clopts_program_destroy(f.program);
}

NAOS_TEST(help_subcommand_without_text_or_options)
{
    /* Neither Options: nor Global Options: is written when both lists are empty. */
    static const char expected[] =
        "Usage: nover init [<options>] [<NAME>]\n"
        "\n"
        "Arguments:\n"
        "  <NAME>  Name to use\n";
    layout_nover_t f;

    layout_nover_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.init, expected);
    clopts_program_destroy(f.program);
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
    clopts_program_destroy(g.program);
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

/* The version line of program with version is exactly expected. */
#define CHECK_VERSION(program, version, expected)                                                 \
    do                                                                                            \
    {                                                                                             \
        static help_recorder_t rec_;                                                              \
        help_recorder_init(&rec_);                                                                \
        NAOS_CHECK_INT_EQ(clopts_write_version((program), (version), help_recorder_write, &rec_), \
                          CLOPTS_WRITE_OK);                                                       \
        NAOS_CHECK_MEM_EQ(rec_.buf, rec_.len, (expected), strlen(expected));                      \
    } while (0)

NAOS_TEST(version_line)
{
    help_fixture_tool_t f;
    help_fixture_bare_t b;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_VERSION(f.program, "1.2.3", "tool 1.2.3\n");
    help_fixture_tool_free(&f);

    help_fixture_bare_init(&b);
    NAOS_REQUIRE_NOT_NULL(b.program);
    CHECK_VERSION(b.program, "0.1", "bare 0.1\n");
    help_fixture_bare_free(&b);
}

NAOS_TEST(version_line_does_not_need_a_version_option)
{
    /* The version string is the caller's: a program without --version has a version line too. */
    help_fixture_filt_t f;
    layout_nover_t g;

    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_VERSION(f.program, "2.0", "filt 2.0\n");
    help_fixture_filt_free(&f);

    /* An empty version is written as is. */
    layout_nover_init(&g);
    NAOS_REQUIRE_NOT_NULL(g.program);
    CHECK_VERSION(g.program, "", "nover \n");
    clopts_program_destroy(g.program);
}

NAOS_TEST(version_line_unavailable_without_version)
{
    static help_recorder_t rec;
    help_fixture_tool_t f;
    help_fixture_filt_t g;

    /* A NULL version gives UNAVAILABLE, even when the program declares --version. */
    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(f.program, NULL, help_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    help_fixture_tool_free(&f);

    help_fixture_filt_init(&g);
    NAOS_REQUIRE_NOT_NULL(g.program);
    help_recorder_init(&rec);
    NAOS_CHECK_INT_EQ(clopts_write_version(g.program, NULL, help_recorder_write, &rec), CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.calls, 0);
    help_fixture_filt_free(&g);
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

/* ---- shadowed global options ------------------------------------------- */

/*
 * shad: with subcommands, no description, no --help (so no Run line).
 *   global: -v/--verbose FLAG, -q/--quiet FLAG, --no-cache FLAG, -x FLAG,
 *           --dry FLAG
 *   run:    -v/--verbose FLAG (shadows -v/--verbose: both names),
 *           --quiet FLAG (shadows --quiet but not -q),
 *           --cache negatable FLAG (its --no-cache shadows the global --no-cache),
 *           -x FLAG (shadows -x, which has no long name)
 *   plain:  no option
 * The handles are NULL when the program could not be made.
 */
typedef struct layout_shad
{
    clopts_program_t *program;
    clopts_command_t *run;
    clopts_command_t *plain;
} layout_shad_t;

/* Attaches a FLAG named long_name / short_name with help to command; NULL on failure. */
static clopts_option_t *layout_add_flag(clopts_command_t *command, const char *long_name, char short_name,
                                        const char *help)
{
    clopts_option_t *option = test_decl_option(CLOPTS_OPTION_FLAG, long_name, short_name);

    test_decl_check(clopts_option_set_help(option, help));
    return test_decl_attach(command, option);
}

static void layout_shad_init(layout_shad_t *f)
{
    clopts_command_t *root;
    clopts_option_t *cache;

    memset(f, 0, sizeof(*f));
    test_decl_reset();
    root = test_decl_root("shad");
    layout_add_flag(root, "verbose", 'v', "Print more details");
    layout_add_flag(root, "quiet", 'q', "Print less");
    layout_add_flag(root, "no-cache", '\0', "Skip the cache");
    layout_add_flag(root, NULL, 'x', "Extra checks");
    layout_add_flag(root, "dry", '\0', "Change nothing");

    f->run = test_decl_command(root, "run");
    test_decl_check(clopts_command_set_summary(f->run, "Run the task"));
    layout_add_flag(f->run, "verbose", 'v', "Print the steps");
    layout_add_flag(f->run, "quiet", '\0', "Print no step");
    cache = test_decl_option(CLOPTS_OPTION_FLAG, "cache", '\0');
    test_decl_check(clopts_option_set_negatable(cache, true));
    test_decl_check(clopts_option_set_help(cache, "Use the cache"));
    test_decl_attach(f->run, cache);
    layout_add_flag(f->run, NULL, 'x', "Run the extra steps");

    f->plain = test_decl_command(root, "plain");
    f->program = test_decl_finish(root);
    if ((f->program == NULL) || (test_decl_failures != 0))
    {
        clopts_program_destroy(f->program);
        memset(f, 0, sizeof(*f));
    }
}

NAOS_TEST(help_global_options_leave_out_the_shadowed_ones)
{
    /*
     * -v/--verbose and -x are taken in run under every name, and --no-cache
     * is the negated form of the negatable --cache of run: they are left
     * out. -q/--quiet keeps -q, so it stays; --dry is not taken at all.
     */
    static const char expected[] =
        "Usage: shad run [<options>]\n"
        "\n"
        "Run the task\n"
        "\n"
        "Options:\n"
        "  -v, --verbose     Print the steps\n"
        "      --quiet       Print no step\n"
        "      --[no-]cache  Use the cache\n"
        "  -x                Run the extra steps\n"
        "\n"
        "Global Options:\n"
        "  -q, --quiet  Print less\n"
        "      --dry    Change nothing\n";
    layout_shad_t f;

    layout_shad_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.run, expected);
    clopts_program_destroy(f.program);
}

NAOS_TEST(help_negatable_global_option_stays_while_its_negation_is_free)
{
    /*
     * The root has a negatable --color; the command has a plain --color.
     * --no-color still reaches the root option there (the command has no
     * no-color and its color is not negatable), so the root option stays in
     * the Global Options. A negatable --color in the command hides it.
     */
    static const char kept[] =
        "Usage: neg plain [<options>]\n"
        "\n"
        "Options:\n"
        "      --color  Paint\n"
        "\n"
        "Global Options:\n"
        "      --[no-]color  Colorize\n";
    static const char hidden[] =
        "Usage: neg both [<options>]\n"
        "\n"
        "Options:\n"
        "      --[no-]color  Paint\n";
    clopts_command_t *root;
    clopts_command_t *plain;
    clopts_command_t *both;
    clopts_option_t *option;
    clopts_program_t *program;

    test_decl_reset();
    root = test_decl_root("neg");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    test_decl_check(clopts_option_set_negatable(option, true));
    test_decl_check(clopts_option_set_help(option, "Colorize"));
    (void)test_decl_attach(root, option);
    plain = test_decl_command(root, "plain");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    test_decl_check(clopts_option_set_help(option, "Paint"));
    (void)test_decl_attach(plain, option);
    both = test_decl_command(root, "both");
    option = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    test_decl_check(clopts_option_set_negatable(option, true));
    test_decl_check(clopts_option_set_help(option, "Paint"));
    (void)test_decl_attach(both, option);
    program = test_decl_finish(root);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    NAOS_REQUIRE_NOT_NULL(program);
    CHECK_HELP(program, plain, kept);
    CHECK_HELP(program, both, hidden);
    clopts_program_destroy(program);
}

NAOS_TEST(help_global_options_are_filtered_per_command)
{
    /* plain shadows nothing: every global option is listed, in the declared order. */
    static const char expected[] =
        "Usage: shad plain [<options>]\n"
        "\n"
        "Global Options:\n"
        "  -v, --verbose   Print more details\n"
        "  -q, --quiet     Print less\n"
        "      --no-cache  Skip the cache\n"
        "  -x              Extra checks\n"
        "      --dry       Change nothing\n";
    layout_shad_t f;

    layout_shad_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, f.plain, expected);
    clopts_program_destroy(f.program);
}

NAOS_TEST(help_program_lists_every_global_option)
{
    /* The program help lists the root options as they are: no filter, and no Run line without --help. */
    static const char expected[] =
        "Usage: shad [<options>] <command> [<arguments>]\n"
        "\n"
        "Commands:\n"
        "  run    Run the task\n"
        "  plain\n"
        "\n"
        "Options:\n"
        "  -v, --verbose   Print more details\n"
        "  -q, --quiet     Print less\n"
        "      --no-cache  Skip the cache\n"
        "  -x              Extra checks\n"
        "      --dry       Change nothing\n";
    layout_shad_t f;

    layout_shad_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    CHECK_HELP(f.program, NULL, expected);
    clopts_program_destroy(f.program);
}

NAOS_TEST(help_shadowed_help_option_is_left_out_of_the_global_options)
{
    /* A command that declares its own -h/--help hides the global one. */
    static const char expected[] =
        "Usage: hid sub [<options>]\n"
        "\n"
        "Options:\n"
        "  -h, --help  Show this help\n"
        "\n"
        "Global Options:\n"
        "      --version  Show version information\n";
    clopts_command_t *root;
    clopts_command_t *sub;
    clopts_program_t *program;

    test_decl_reset();
    root = test_decl_root("hid");
    test_decl_check(clopts_help_add_help_option(root, NULL));
    test_decl_check(clopts_help_add_version_option(root, NULL));
    sub = test_decl_command(root, "sub");
    test_decl_check(clopts_help_add_help_option(sub, NULL));
    program = test_decl_finish(root);
    NAOS_REQUIRE_NOT_NULL(program);
    NAOS_REQUIRE_UINT_EQ(test_decl_failures, 0);
    CHECK_HELP(program, sub, expected);
    clopts_program_destroy(program);
}

/* ---- the closing Run line ---------------------------------------------- */

/*
 * A program "run" with one subcommand "sub"; when root_long / root_short is
 * given, the root has a FLAG of that name, when sub_long is given, sub has
 * a FLAG of that name. NULL on failure.
 */
static clopts_program_t *layout_run_line_program(const char *root_long, char root_short, const char *sub_long)
{
    clopts_command_t *root;
    clopts_command_t *sub;

    test_decl_reset();
    root = test_decl_root("run");
    if ((root_long != NULL) || (root_short != '\0'))
    {
        test_decl_add(root, CLOPTS_OPTION_FLAG, root_long, root_short);
    }
    sub = test_decl_command(root, "sub");
    if (sub_long != NULL)
    {
        test_decl_add(sub, CLOPTS_OPTION_FLAG, sub_long, '\0');
    }
    if (test_decl_failures != 0)
    {
        test_decl_discard(root);
        return NULL;
    }
    return test_decl_finish(root);
}

/* The program help of program ends with the Run line exactly when expected. */
static void layout_check_run_line(const clopts_program_t *program, bool expected)
{
    static const char run_line[] = "\nRun 'run <command> --help' for help on a command.\n";
    static help_recorder_t rec;
    size_t len = strlen(run_line);
    bool ends_with_it;

    NAOS_REQUIRE_INT_EQ(write_help(&rec, program, NULL), CLOPTS_WRITE_OK);
    ends_with_it = (rec.len >= len) && (memcmp(rec.buf + rec.len - len, run_line, len) == 0);
    NAOS_CHECK_INT_EQ(ends_with_it, expected);
    NAOS_CHECK_INT_EQ(strstr(rec.buf, "Run '") != NULL, expected);
}

NAOS_TEST(run_line_needs_a_root_long_option_named_help)
{
    clopts_program_t *program;

    /* A plain FLAG --help (not terminal, without -h) is enough. */
    NAOS_TRACE_PUSH("root --help");
    program = layout_run_line_program("help", '\0', NULL);
    NAOS_REQUIRE_NOT_NULL(program);
    layout_check_run_line(program, true);
    clopts_program_destroy(program);
    NAOS_TRACE_POP();

    /* The short name h alone is not --help. */
    NAOS_TRACE_PUSH("root -h");
    program = layout_run_line_program(NULL, 'h', NULL);
    NAOS_REQUIRE_NOT_NULL(program);
    layout_check_run_line(program, false);
    clopts_program_destroy(program);
    NAOS_TRACE_POP();

    /* Another long name is not --help. */
    NAOS_TRACE_PUSH("root --helps");
    program = layout_run_line_program("helps", '\0', NULL);
    NAOS_REQUIRE_NOT_NULL(program);
    layout_check_run_line(program, false);
    clopts_program_destroy(program);
    NAOS_TRACE_POP();

    /* A --help of a subcommand is not a global --help. */
    NAOS_TRACE_PUSH("sub --help");
    program = layout_run_line_program(NULL, '\0', "help");
    NAOS_REQUIRE_NOT_NULL(program);
    layout_check_run_line(program, false);
    clopts_program_destroy(program);
    NAOS_TRACE_POP();
}

NAOS_TEST(run_line_is_not_in_the_help_of_a_program_without_subcommands)
{
    static help_recorder_t rec;
    help_fixture_filt_t f;

    /* filt has a root --help but no subcommand. */
    help_fixture_filt_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_INT_EQ(write_help(&rec, f.program, NULL), CLOPTS_WRITE_OK);
    NAOS_CHECK_NULL(strstr(rec.buf, "Run '"));
    help_fixture_filt_free(&f);
}

/* ---- declarations for the help texts ----------------------------------- */

/* A value no clopts function returns, to see that an out pointer is written. */
static const clopts_option_t *const layout_stale_option = (const clopts_option_t *)&layout_stale_option;

NAOS_TEST(add_help_option_attributes)
{
    clopts_command_t *command = test_decl_root("t");
    const clopts_option_t *option = NULL;

    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_REQUIRE_INT_EQ(clopts_help_add_help_option(command, &option), CLOPTS_DECL_NONE);
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(command), 1);
    NAOS_CHECK_PTR_EQ(clopts_command_option_at(command, 0), option);
    NAOS_CHECK_INT_EQ(clopts_option_kind_of(option), CLOPTS_OPTION_FLAG);
    NAOS_CHECK_STR_EQ(clopts_option_long_name(option), "help");
    NAOS_CHECK_INT_EQ(clopts_option_short_name(option), 'h');
    NAOS_CHECK_STR_EQ(clopts_option_help(option), "Show this help");
    NAOS_CHECK_TRUE(clopts_option_terminal(option));
    NAOS_CHECK_FALSE(clopts_option_negatable(option));
    NAOS_CHECK_FALSE(clopts_option_required(option));
    test_decl_discard(command);
}

NAOS_TEST(add_version_option_attributes)
{
    clopts_command_t *command = test_decl_root("t");
    const clopts_option_t *option = NULL;

    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_REQUIRE_INT_EQ(clopts_help_add_version_option(command, &option), CLOPTS_DECL_NONE);
    NAOS_REQUIRE_NOT_NULL(option);
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(command), 1);
    NAOS_CHECK_PTR_EQ(clopts_command_option_at(command, 0), option);
    NAOS_CHECK_INT_EQ(clopts_option_kind_of(option), CLOPTS_OPTION_FLAG);
    NAOS_CHECK_STR_EQ(clopts_option_long_name(option), "version");
    NAOS_CHECK_INT_EQ(clopts_option_short_name(option), '\0');
    NAOS_CHECK_STR_EQ(clopts_option_help(option), "Show version information");
    NAOS_CHECK_TRUE(clopts_option_terminal(option));
    NAOS_CHECK_FALSE(clopts_option_negatable(option));
    test_decl_discard(command);
}

NAOS_TEST(add_options_accept_a_null_out)
{
    clopts_command_t *command = test_decl_root("t");

    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(command, NULL), CLOPTS_DECL_NONE);
    NAOS_CHECK_INT_EQ(clopts_help_add_version_option(command, NULL), CLOPTS_DECL_NONE);
    NAOS_REQUIRE_UINT_EQ(clopts_command_option_count(command), 2);
    NAOS_CHECK_STR_EQ(clopts_option_long_name(clopts_command_option_at(command, 0)), "help");
    NAOS_CHECK_STR_EQ(clopts_option_long_name(clopts_command_option_at(command, 1)), "version");
    test_decl_discard(command);
}

NAOS_TEST(add_options_twice_in_a_command_fail_and_attach_nothing)
{
    clopts_command_t *command = test_decl_root("t");
    const clopts_option_t *option;

    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_REQUIRE_INT_EQ(clopts_help_add_help_option(command, NULL), CLOPTS_DECL_NONE);
    NAOS_REQUIRE_INT_EQ(clopts_help_add_version_option(command, NULL), CLOPTS_DECL_NONE);

    option = layout_stale_option;
    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(command, &option), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_NULL(option);
    option = layout_stale_option;
    NAOS_CHECK_INT_EQ(clopts_help_add_version_option(command, &option), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_NULL(option);
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(command), 2);
    test_decl_discard(command);
}

NAOS_TEST(add_help_option_fails_on_a_taken_short_name)
{
    /* -h is taken by another option of the command: nothing is attached, not even --help. */
    clopts_command_t *command = test_decl_root("t");
    const clopts_option_t *option = layout_stale_option;

    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_REQUIRE_NOT_NULL(test_decl_add(command, CLOPTS_OPTION_VALUE, "host", 'h'));
    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(command, &option), CLOPTS_DECL_DUPLICATE_OPTION);
    NAOS_CHECK_NULL(option);
    NAOS_REQUIRE_UINT_EQ(clopts_command_option_count(command), 1);
    NAOS_CHECK_STR_EQ(clopts_option_long_name(clopts_command_option_at(command, 0)), "host");
    test_decl_discard(command);
}

NAOS_TEST(add_options_in_several_commands)
{
    /* Option names are checked within a command only: root and subcommand may both have --help. */
    clopts_command_t *root = test_decl_root("t");
    clopts_command_t *sub = test_decl_command(root, "sub");

    NAOS_REQUIRE_NOT_NULL(sub);
    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(root, NULL), CLOPTS_DECL_NONE);
    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(sub, NULL), CLOPTS_DECL_NONE);
    NAOS_CHECK_INT_EQ(clopts_help_add_version_option(root, NULL), CLOPTS_DECL_NONE);
    NAOS_CHECK_INT_EQ(clopts_help_add_version_option(sub, NULL), CLOPTS_DECL_NONE);
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(root), 2);
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(sub), 2);
    test_decl_discard(root);
}

NAOS_TEST(add_options_reject_a_null_command)
{
    const clopts_option_t *option = layout_stale_option;

    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(NULL, &option), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_NULL(option);
    option = layout_stale_option;
    NAOS_CHECK_INT_EQ(clopts_help_add_version_option(NULL, &option), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_NULL(option);
    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
    NAOS_CHECK_INT_EQ(clopts_help_add_version_option(NULL, NULL), CLOPTS_DECL_INVALID_ARGUMENT);
}

NAOS_TEST(add_options_to_an_attached_command_fail)
{
    /* The commands of a program are read-only. */
    const clopts_option_t *option = layout_stale_option;
    layout_nover_t f;

    layout_nover_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    NAOS_CHECK_INT_EQ(clopts_help_add_help_option(f.init, &option), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_NULL(option);
    NAOS_CHECK_INT_EQ(clopts_help_add_version_option(f.init, NULL), CLOPTS_DECL_ALREADY_ATTACHED);
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(f.init), 0);
    clopts_program_destroy(f.program);
}

NAOS_TEST(help_command_new_declaration)
{
    clopts_command_t *command = NULL;
    const clopts_positional_t *positional;

    NAOS_REQUIRE_INT_EQ(clopts_help_command_create(&command), CLOPTS_DECL_NONE);
    NAOS_REQUIRE_NOT_NULL(command);
    NAOS_CHECK_STR_EQ(clopts_command_name(command), "help");
    NAOS_CHECK_STR_EQ(clopts_command_summary(command), "Show help for a command");
    NAOS_CHECK_NULL(clopts_command_description(command));
    NAOS_CHECK_UINT_EQ(clopts_command_option_count(command), 0);
    NAOS_REQUIRE_UINT_EQ(clopts_command_positional_count(command), 1);
    positional = clopts_command_positional_at(command, 0);
    NAOS_CHECK_STR_EQ(clopts_positional_name(positional), "COMMAND");
    NAOS_CHECK_STR_EQ(clopts_positional_help(positional), "Command to show help for");
    NAOS_CHECK_FALSE(clopts_positional_required(positional));
    NAOS_CHECK_FALSE(clopts_positional_variadic(positional));
    /* It is unattached: the caller owns it. */
    clopts_command_destroy(command);
}

NAOS_TEST(help_command_new_rejects_a_null_out)
{
    NAOS_CHECK_INT_EQ(clopts_help_command_create(NULL), CLOPTS_DECL_INVALID_ARGUMENT);
}

NAOS_TEST(help_command_new_gives_a_new_command_each_time)
{
    /* Two of them cannot be in one program (DUPLICATE_COMMAND), but each is a separate command. */
    clopts_command_t *first = NULL;
    clopts_command_t *second = NULL;
    clopts_command_t *root = test_decl_root("t");
    clopts_command_t *commands[2];
    clopts_program_t *program = NULL;

    NAOS_REQUIRE_NOT_NULL(root);
    NAOS_REQUIRE_INT_EQ(clopts_help_command_create(&first), CLOPTS_DECL_NONE);
    NAOS_REQUIRE_INT_EQ(clopts_help_command_create(&second), CLOPTS_DECL_NONE);
    NAOS_CHECK_TRUE(first != second);
    commands[0] = first;
    commands[1] = second;
    NAOS_CHECK_INT_EQ(clopts_program_create(root, commands, 2, &program), CLOPTS_DECL_DUPLICATE_COMMAND);
    NAOS_CHECK_NULL(program);
    clopts_command_destroy(second);
    clopts_command_destroy(first);
    test_decl_discard(root);
}

/* Environment lookup that finds no variable, so the parses do not depend on the environment. */
static clopts_env_status_t layout_env_unset(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_UNSET;
}

/* Number of elements of argv before its terminating NULL. */
#define LAYOUT_ARGC(argv) ((int)(sizeof(argv) / sizeof((argv)[0])) - 1)

/*
 * Parses argv against program, requires the status, and checks that
 * clopts_help_target() returns expected and sets the target to
 * expected_target (from a stale value).
 */
static void layout_check_target(const clopts_program_t *program, int argc, char *const *argv,
                                clopts_status_t status, bool expected, const clopts_command_t *expected_target)
{
    clopts_result_t *result = clopts_parse_with_env(program, argc, argv, layout_env_unset, NULL);
    const clopts_command_t *target = (const clopts_command_t *)&layout_stale_option;

    NAOS_CHECK_INT_EQ(clopts_result_status(result), status);
    NAOS_CHECK_INT_EQ(clopts_help_target(program, result, &target), expected);
    NAOS_CHECK_PTR_EQ(target, expected_target);
    clopts_result_free(result);
}

NAOS_TEST(help_target_without_a_positional_is_the_program)
{
    char *argv[] = { "tool", "help", NULL };
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    layout_check_target(f.program, LAYOUT_ARGC(argv), argv, CLOPTS_OK, true, NULL);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_target_names_a_known_command)
{
    char *argv_build[] = { "tool", "help", "build", NULL };
    char *argv_help[] = { "tool", "help", "help", NULL };
    char *argv_global[] = { "tool", "-v", "help", "copy", NULL };
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    layout_check_target(f.program, LAYOUT_ARGC(argv_build), argv_build, CLOPTS_OK, true, f.build);
    layout_check_target(f.program, LAYOUT_ARGC(argv_help), argv_help, CLOPTS_OK, true, f.help_command);
    layout_check_target(f.program, LAYOUT_ARGC(argv_global), argv_global, CLOPTS_OK, true, f.copy);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_target_of_an_unknown_command_is_false)
{
    char *argv[] = { "tool", "help", "bulid", NULL };
    /* The root is not a subcommand. */
    char *argv_root[] = { "tool", "help", "tool", NULL };
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    layout_check_target(f.program, LAYOUT_ARGC(argv), argv, CLOPTS_OK, false, NULL);
    layout_check_target(f.program, LAYOUT_ARGC(argv_root), argv_root, CLOPTS_OK, false, NULL);
    help_fixture_tool_free(&f);
}

NAOS_TEST(help_target_of_a_status_other_than_ok_is_false)
{
    char *argv_terminated[] = { "tool", "help", "build", "--help", NULL };
    char *argv_no_command[] = { "tool", NULL };
    char *argv_usage[] = { "tool", "help", "build", "copy", NULL };
    const clopts_command_t *target = (const clopts_command_t *)&layout_stale_option;
    help_fixture_tool_t f;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    layout_check_target(f.program, LAYOUT_ARGC(argv_terminated), argv_terminated, CLOPTS_TERMINATED, false, NULL);
    layout_check_target(f.program, LAYOUT_ARGC(argv_no_command), argv_no_command, CLOPTS_NO_COMMAND, false, NULL);
    layout_check_target(f.program, LAYOUT_ARGC(argv_usage), argv_usage, CLOPTS_USAGE_ERROR, false, NULL);

    /* A NULL result reads as out of memory. */
    NAOS_CHECK_FALSE(clopts_help_target(f.program, NULL, &target));
    NAOS_CHECK_NULL(target);
    help_fixture_tool_free(&f);
}

/* ---- determinism ------------------------------------------------------- */

/* Every text of command (NULL: the program) of program, concatenated into rec. */
static void write_all(help_recorder_t *rec, const clopts_program_t *program, const clopts_command_t *command)
{
    help_recorder_init(rec);
    NAOS_CHECK_INT_EQ(clopts_write_help(program, command, help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_INT_EQ(clopts_write_usage(program, command, help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_INT_EQ(clopts_write_version(program, "1.2.3", help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_INT_EQ(clopts_write_usage_hint(program, command, help_recorder_write, rec), CLOPTS_WRITE_OK);
    NAOS_CHECK_FALSE(rec->overflow);
}

NAOS_TEST(generation_is_repeatable_and_keeps_the_declaration)
{
    static help_recorder_t first;
    static help_recorder_t second;
    help_fixture_tool_t f;
    const clopts_command_t *commands[4];
    size_t i;

    help_fixture_tool_init(&f);
    NAOS_REQUIRE_NOT_NULL(f.program);
    commands[0] = NULL;
    commands[1] = f.build;
    commands[2] = f.copy;
    commands[3] = f.help_command;
    for (i = 0; i < 4; i++)
    {
        write_all(&first, f.program, commands[i]);
        write_all(&second, f.program, commands[i]);
        NAOS_CHECK_MEM_EQ(second.buf, second.len, first.buf, first.len);
    }
    /* The declaration is unchanged, and so is its help. */
    CHECK_HELP(f.program, NULL, tool_help);
    help_fixture_tool_free(&f);
}

/* ---- shape of every text ----------------------------------------------- */

/*
 * NULL when the len bytes at buf are a well-formed text (10.4, 10.5): not
 * empty, every line ended by LF, no CR, no line ending with a space or a
 * tab, no blank line at the start or the end, no two blank lines in a row.
 * Otherwise a description of the first problem.
 */
static const char *layout_shape_problem(const char *buf, size_t len)
{
    size_t i;

    if (len == 0)
    {
        return "empty";
    }
    if (buf[len - 1] != '\n')
    {
        return "does not end with LF";
    }
    if (buf[0] == '\n')
    {
        return "starts with a blank line";
    }
    if ((len >= 2) && (buf[len - 2] == '\n'))
    {
        return "ends with a blank line";
    }
    for (i = 0; i < len; i++)
    {
        if (buf[i] == '\r')
        {
            return "contains CR";
        }
        if (buf[i] != '\n')
        {
            continue;
        }
        if ((i > 0) && ((buf[i - 1] == ' ') || (buf[i - 1] == '\t')))
        {
            return "a line ends with a space";
        }
        if ((i >= 2) && (buf[i - 1] == '\n') && (buf[i - 2] == '\n'))
        {
            return "two blank lines in a row";
        }
    }
    return NULL;
}

#define SHAPE_OF(lit) layout_shape_problem((lit), sizeof(lit) - 1)

NAOS_TEST(shape_check_rejects_each_malformed_text)
{
    NAOS_CHECK_NULL(SHAPE_OF("a\n"));
    NAOS_CHECK_NULL(SHAPE_OF("a\n\nb\n  c  d\n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF(""));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a\nb"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a\r\n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a\rb\n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a \n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a\nb\t\nc\n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF(" \n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("\na\n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a\n\n"));
    NAOS_CHECK_NOT_NULL(SHAPE_OF("a\n\n\nb\n"));
}

/* The text in rec, written by a call that returned status, is well formed. */
static void layout_check_shape(const help_recorder_t *rec, clopts_write_status_t status)
{
    const char *problem;

    NAOS_CHECK_INT_EQ(status, CLOPTS_WRITE_OK);
    NAOS_CHECK_FALSE(rec->overflow);
    problem = layout_shape_problem(rec->buf, rec->len);
    if (problem != NULL)
    {
        NAOS_NOTE("%s in:\n%s", problem, rec->buf);
    }
    NAOS_CHECK_NULL(problem);
}

/*
 * The help, the Usage line, the usage hint and the version line (with a
 * version string) of command (NULL: the program) of program are well
 * formed; without a version string the version line is UNAVAILABLE and
 * empty.
 */
static void layout_check_texts(const clopts_program_t *program, const clopts_command_t *command)
{
    static help_recorder_t rec;
    clopts_write_status_t status;

    help_recorder_init(&rec);
    NAOS_TRACE_PUSH("help");
    layout_check_shape(&rec, clopts_write_help(program, command, help_recorder_write, &rec));
    NAOS_TRACE_POP();

    help_recorder_init(&rec);
    NAOS_TRACE_PUSH("usage");
    layout_check_shape(&rec, clopts_write_usage(program, command, help_recorder_write, &rec));
    NAOS_TRACE_POP();

    help_recorder_init(&rec);
    NAOS_TRACE_PUSH("usage hint");
    layout_check_shape(&rec, clopts_write_usage_hint(program, command, help_recorder_write, &rec));
    NAOS_TRACE_POP();

    help_recorder_init(&rec);
    NAOS_TRACE_PUSH("version");
    layout_check_shape(&rec, clopts_write_version(program, "1.0", help_recorder_write, &rec));
    help_recorder_init(&rec);
    status = clopts_write_version(program, NULL, help_recorder_write, &rec);
    NAOS_CHECK_INT_EQ(status, CLOPTS_WRITE_UNAVAILABLE);
    NAOS_CHECK_UINT_EQ(rec.len, 0);
    NAOS_TRACE_POP();
}

NAOS_TEST(every_text_of_the_fixtures_is_well_formed)
{
    help_fixture_tool_t tool;
    help_fixture_filt_t filt;
    help_fixture_bare_t bare;
    help_fixture_wide_t wide;
    layout_nover_t nover;
    size_t i;

    help_fixture_tool_init(&tool);
    help_fixture_filt_init(&filt);
    help_fixture_bare_init(&bare);
    help_fixture_wide_init(&wide);
    layout_nover_init(&nover);
    NAOS_REQUIRE_NOT_NULL(tool.program);
    NAOS_REQUIRE_NOT_NULL(filt.program);
    NAOS_REQUIRE_NOT_NULL(bare.program);
    NAOS_REQUIRE_NOT_NULL(wide.program);
    NAOS_REQUIRE_NOT_NULL(nover.program);
    {
        const struct
        {
            const char *name;
            const clopts_program_t *program;
            const clopts_command_t *command;
        } texts[] = {
            { "tool", tool.program, NULL },
            { "tool (root)", tool.program, tool.root },
            { "tool build", tool.program, tool.build },
            { "tool copy", tool.program, tool.copy },
            { "tool help", tool.program, tool.help_command },
            { "filt", filt.program, NULL },
            { "filt (root)", filt.program, filt.root },
            { "bare", bare.program, NULL },
            { "bare run", bare.program, bare.run },
            { "bare help", bare.program, bare.help_command },
            { "wide", wide.program, NULL },
            { "nover", nover.program, NULL },
            { "nover init", nover.program, nover.init },
        };

        for (i = 0; i < sizeof(texts) / sizeof(texts[0]); i++)
        {
            NAOS_TRACE_PUSH("%s", texts[i].name);
            layout_check_texts(texts[i].program, texts[i].command);
            NAOS_TRACE_POP();
        }
    }
    clopts_program_destroy(nover.program);
    help_fixture_wide_free(&wide);
    help_fixture_bare_free(&bare);
    help_fixture_filt_free(&filt);
    help_fixture_tool_free(&tool);
}

NAOS_SUITE(layout,
    NAOS_ADD_TEST(fixture_tool_builds_ok),
    NAOS_ADD_TEST(fixture_filt_builds_ok),
    NAOS_ADD_TEST(fixture_bare_builds_ok),
    NAOS_ADD_TEST(fixture_free_accepts_a_cleared_struct),
    NAOS_ADD_TEST(help_tool_global),
    NAOS_ADD_TEST(help_tool_root_is_the_global_help),
    NAOS_ADD_TEST(help_tool_build),
    NAOS_ADD_TEST(help_tool_copy_uses_the_summary),
    NAOS_ADD_TEST(help_tool_help_command),
    NAOS_ADD_TEST(help_filt_without_subcommands),
    NAOS_ADD_TEST(help_filt_root_is_the_program_help),
    NAOS_ADD_TEST(help_bare_run_has_only_the_global_options),
    NAOS_ADD_TEST(help_bare_global_without_description),
    NAOS_ADD_TEST(help_without_root_options_has_no_options_and_no_run_line),
    NAOS_ADD_TEST(help_subcommand_without_text_or_options),
    NAOS_ADD_TEST(usage_global),
    NAOS_ADD_TEST(usage_subcommand),
    NAOS_ADD_TEST(usage_without_subcommands),
    NAOS_ADD_TEST(usage_is_the_first_line_of_the_help),
    NAOS_ADD_TEST(version_line),
    NAOS_ADD_TEST(version_line_does_not_need_a_version_option),
    NAOS_ADD_TEST(version_line_unavailable_without_version),
    NAOS_ADD_TEST(usage_hint_without_command),
    NAOS_ADD_TEST(usage_hint_with_command),
    NAOS_ADD_TEST(help_global_options_leave_out_the_shadowed_ones),
    NAOS_ADD_TEST(help_negatable_global_option_stays_while_its_negation_is_free),
    NAOS_ADD_TEST(help_global_options_are_filtered_per_command),
    NAOS_ADD_TEST(help_program_lists_every_global_option),
    NAOS_ADD_TEST(help_shadowed_help_option_is_left_out_of_the_global_options),
    NAOS_ADD_TEST(run_line_needs_a_root_long_option_named_help),
    NAOS_ADD_TEST(run_line_is_not_in_the_help_of_a_program_without_subcommands),
    NAOS_ADD_TEST(add_help_option_attributes),
    NAOS_ADD_TEST(add_version_option_attributes),
    NAOS_ADD_TEST(add_options_accept_a_null_out),
    NAOS_ADD_TEST(add_options_twice_in_a_command_fail_and_attach_nothing),
    NAOS_ADD_TEST(add_help_option_fails_on_a_taken_short_name),
    NAOS_ADD_TEST(add_options_in_several_commands),
    NAOS_ADD_TEST(add_options_reject_a_null_command),
    NAOS_ADD_TEST(add_options_to_an_attached_command_fail),
    NAOS_ADD_TEST(help_command_new_declaration),
    NAOS_ADD_TEST(help_command_new_rejects_a_null_out),
    NAOS_ADD_TEST(help_command_new_gives_a_new_command_each_time),
    NAOS_ADD_TEST(help_target_without_a_positional_is_the_program),
    NAOS_ADD_TEST(help_target_names_a_known_command),
    NAOS_ADD_TEST(help_target_of_an_unknown_command_is_false),
    NAOS_ADD_TEST(help_target_of_a_status_other_than_ok_is_false),
    NAOS_ADD_TEST(generation_is_repeatable_and_keeps_the_declaration),
    NAOS_ADD_TEST(shape_check_rejects_each_malformed_text),
    NAOS_ADD_TEST(every_text_of_the_fixtures_is_well_formed)
)
