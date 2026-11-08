/*
 * Suite decl: declaration validation (clopts_decl_validate and the
 * declaration check run by clopts_parse*) and the lookups of commands and
 * options.
 *
 * Every invalid declaration is a fictional static const table defined in this
 * file; the shared fixtures hold valid declarations only.
 */

#include "test_fixtures.h"

#include "parser/parser_internal.h"

#include <naos/naos.h>

#include <string.h>

/* ---- helpers ----------------------------------------------------------- */

/* Number of elements of a static array. */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Returns an error filled with values that a declaration check never stores,
 * so that a test can see which fields were written.
 */
static clopts_error_t poisoned_error(void)
{
    static const clopts_option_t poison_option = { .long_name = "poison" };
    static const clopts_positional_t poison_positional = { .name = "POISON" };
    static const clopts_command_t poison_command = { .name = "poison" };
    clopts_error_t error;

    memset(&error, 0, sizeof(error));
    error.decl = CLOPTS_DECL_VARIADIC_POSITION;
    error.command = &poison_command;
    error.option = &poison_option;
    error.positional = &poison_positional;
    return error;
}

/*
 * Runs clopts_decl_validate and clopts_parser_decl_check on program and checks
 * that both report the violation kind at command / option / positional.
 */
#define CHECK_DECL_ERROR(program, kind, cmd, opt, pos)                                 \
    do                                                                                 \
    {                                                                                  \
        clopts_error_t check_error_ = poisoned_error();                                \
        clopts_error_t check_internal_ = poisoned_error();                             \
        NAOS_CHECK_INT_EQ(clopts_decl_validate(&(program), &check_error_),             \
                          CLOPTS_DECL_ERROR);                                          \
        NAOS_CHECK_INT_EQ(check_error_.decl, (kind));                                  \
        NAOS_CHECK_PTR_EQ(check_error_.command, (cmd));                                \
        NAOS_CHECK_PTR_EQ(check_error_.option, (opt));                                 \
        NAOS_CHECK_PTR_EQ(check_error_.positional, (pos));                             \
        NAOS_CHECK_INT_EQ(clopts_parser_decl_check(&(program), &check_internal_), -1); \
        NAOS_CHECK_INT_EQ(check_internal_.decl, (kind));                               \
        NAOS_CHECK_PTR_EQ(check_internal_.command, (cmd));                             \
        NAOS_CHECK_PTR_EQ(check_internal_.option, (opt));                              \
        NAOS_CHECK_PTR_EQ(check_internal_.positional, (pos));                          \
    } while (0)

/* Checks that program is valid for both entry points. */
#define CHECK_DECL_OK(program)                                                        \
    do                                                                                \
    {                                                                                 \
        clopts_error_t check_error_ = poisoned_error();                               \
        NAOS_CHECK_INT_EQ(clopts_decl_validate(&(program), &check_error_), CLOPTS_OK); \
        NAOS_CHECK_INT_EQ(clopts_parser_decl_check(&(program), NULL), 0);             \
    } while (0)

/* A program without subcommands whose only option is the given table. */
#define PROGRAM_WITH_OPTIONS(opts)                                                    \
    {                                                                                 \
        .name = "prog", .options = (opts), .option_count = ARRAY_LEN(opts),           \
    }

/* ---- fixtures ---------------------------------------------------------- */

NAOS_TEST(fixture_programs_are_shaped_as_designed)
{
    NAOS_CHECK_STR_EQ(fixture_tool.name, "tool");
    NAOS_CHECK_STR_EQ(fixture_tool.version, "1.2.3");
    NAOS_CHECK_PTR_EQ(fixture_tool.options, fixture_tool_options);
    NAOS_CHECK_UINT_EQ(fixture_tool.option_count, 3);
    NAOS_CHECK_PTR_EQ(fixture_tool.commands, fixture_tool_commands);
    NAOS_CHECK_UINT_EQ(fixture_tool.command_count, 3);

    NAOS_CHECK_STR_EQ(fixture_filt.name, "filt");
    NAOS_CHECK_NULL(fixture_filt.version);
    NAOS_CHECK_UINT_EQ(fixture_filt.option_count, 1);
    NAOS_CHECK_UINT_EQ(fixture_filt.positional_count, 1);
    NAOS_CHECK_UINT_EQ(fixture_filt.command_count, 0);
    NAOS_CHECK_NULL(fixture_filt.commands);

    NAOS_CHECK_STR_EQ(fixture_strict.name, "strict");
    NAOS_CHECK_UINT_EQ(fixture_strict.option_count, 1);
    NAOS_CHECK_UINT_EQ(fixture_strict.command_count, 1);
    NAOS_CHECK_STR_EQ(fixture_strict_commands[FIXTURE_STRICT_CMD_RUN].name, "run");
    NAOS_CHECK_UINT_EQ(fixture_strict_commands[FIXTURE_STRICT_CMD_RUN].option_count, 0);
    NAOS_CHECK_UINT_EQ(fixture_strict_commands[FIXTURE_STRICT_CMD_RUN].positional_count, 0);
}

/* ---- valid declarations (2.2) ------------------------------------------ */

NAOS_TEST(valid_fixture_declarations_are_ok)
{
    CHECK_DECL_OK(fixture_tool);
    CHECK_DECL_OK(fixture_filt);
    CHECK_DECL_OK(fixture_strict);
}

NAOS_TEST(validate_accepts_null_error)
{
    static const clopts_program_t empty_name = { .name = "" };

    NAOS_CHECK_INT_EQ(clopts_decl_validate(&fixture_tool, NULL), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(clopts_decl_validate(&empty_name, NULL), CLOPTS_DECL_ERROR);
    NAOS_CHECK_INT_EQ(clopts_parser_decl_check(&empty_name, NULL), -1);
}

NAOS_TEST(validate_leaves_error_untouched_when_valid)
{
    clopts_error_t error = poisoned_error();
    clopts_error_t before = poisoned_error();

    NAOS_CHECK_INT_EQ(clopts_decl_validate(&fixture_tool, &error), CLOPTS_OK);
    NAOS_CHECK_INT_EQ(error.decl, before.decl);
    NAOS_CHECK_PTR_EQ(error.command, before.command);
    NAOS_CHECK_PTR_EQ(error.option, before.option);
    NAOS_CHECK_PTR_EQ(error.positional, before.positional);
}

NAOS_TEST(error_fields_for_usage_errors_are_reset)
{
    static const clopts_program_t empty_name = { .name = "" };
    clopts_error_t error = poisoned_error();

    error.token = "stale";
    error.argv_index = 7;
    error.name_len = 3;
    error.short_name = 'q';
    NAOS_CHECK_INT_EQ(clopts_decl_validate(&empty_name, &error), CLOPTS_DECL_ERROR);
    NAOS_CHECK_NULL(error.token);
    NAOS_CHECK_INT_EQ(error.argv_index, -1);
    NAOS_CHECK_UINT_EQ(error.name_len, 0);
    NAOS_CHECK_INT_EQ(error.short_name, '\0');
}

/* ---- INVALID_NAME: program and command names (2.16) -------------------- */

NAOS_TEST(program_name_null_or_empty_is_invalid)
{
    static const clopts_program_t null_name = { .name = NULL };
    static const clopts_program_t empty_name = { .name = "" };

    CHECK_DECL_ERROR(null_name, CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
    CHECK_DECL_ERROR(empty_name, CLOPTS_DECL_INVALID_NAME, NULL, NULL, NULL);
}

static const clopts_command_t bad_command_names[] = {
    { .name = "ok" },
    { .name = "" },
};
static const clopts_program_t prog_bad_command_empty = {
    .name = "prog", .commands = bad_command_names, .command_count = ARRAY_LEN(bad_command_names),
};

static const clopts_command_t null_command_name[] = { { .name = NULL } };
static const clopts_program_t prog_bad_command_null = {
    .name = "prog", .commands = null_command_name, .command_count = 1,
};

static const clopts_command_t dash_command_name[] = { { .name = "-run" } };
static const clopts_program_t prog_bad_command_dash = {
    .name = "prog", .commands = dash_command_name, .command_count = 1,
};

static const clopts_command_t space_command_name[] = { { .name = "run it" } };
static const clopts_program_t prog_bad_command_space = {
    .name = "prog", .commands = space_command_name, .command_count = 1,
};

static const clopts_command_t tab_command_name[] = { { .name = "run\tit" } };
static const clopts_program_t prog_bad_command_tab = {
    .name = "prog", .commands = tab_command_name, .command_count = 1,
};

NAOS_TEST(command_name_malformed_is_invalid)
{
    CHECK_DECL_ERROR(prog_bad_command_empty, CLOPTS_DECL_INVALID_NAME, &bad_command_names[1], NULL,
                     NULL);
    CHECK_DECL_ERROR(prog_bad_command_null, CLOPTS_DECL_INVALID_NAME, &null_command_name[0], NULL,
                     NULL);
    CHECK_DECL_ERROR(prog_bad_command_dash, CLOPTS_DECL_INVALID_NAME, &dash_command_name[0], NULL,
                     NULL);
    CHECK_DECL_ERROR(prog_bad_command_space, CLOPTS_DECL_INVALID_NAME, &space_command_name[0], NULL,
                     NULL);
    CHECK_DECL_ERROR(prog_bad_command_tab, CLOPTS_DECL_INVALID_NAME, &tab_command_name[0], NULL,
                     NULL);
}

/* ---- NO_NAME (2.6) ----------------------------------------------------- */

static const clopts_option_t no_name_options[] = {
    { .long_name = "fine", .kind = CLOPTS_FLAG },
    { .long_name = NULL, .short_name = '\0', .kind = CLOPTS_FLAG },
};
static const clopts_program_t prog_no_name = PROGRAM_WITH_OPTIONS(no_name_options);

NAOS_TEST(option_without_any_name_is_no_name)
{
    CHECK_DECL_ERROR(prog_no_name, CLOPTS_DECL_NO_NAME, NULL, &no_name_options[1], NULL);
}

/* ---- INVALID_NAME: option names and env (2.7) -------------------------- */

static const clopts_option_t long_empty[] = { { .long_name = "", .kind = CLOPTS_FLAG } };
static const clopts_option_t long_dash[] = { { .long_name = "-x", .kind = CLOPTS_FLAG } };
static const clopts_option_t long_equals[] = { { .long_name = "a=b", .kind = CLOPTS_FLAG } };
static const clopts_option_t long_space[] = { { .long_name = "a b", .kind = CLOPTS_FLAG } };
static const clopts_option_t short_dash[] = { { .short_name = '-', .kind = CLOPTS_FLAG } };
static const clopts_option_t short_equals[] = { { .short_name = '=', .kind = CLOPTS_FLAG } };
static const clopts_option_t short_space[] = { { .short_name = ' ', .kind = CLOPTS_FLAG } };
static const clopts_option_t env_empty[] = {
    { .long_name = "path", .kind = CLOPTS_VALUE, .env = "" },
};

static const clopts_program_t prog_long_empty = PROGRAM_WITH_OPTIONS(long_empty);
static const clopts_program_t prog_long_dash = PROGRAM_WITH_OPTIONS(long_dash);
static const clopts_program_t prog_long_equals = PROGRAM_WITH_OPTIONS(long_equals);
static const clopts_program_t prog_long_space = PROGRAM_WITH_OPTIONS(long_space);
static const clopts_program_t prog_short_dash = PROGRAM_WITH_OPTIONS(short_dash);
static const clopts_program_t prog_short_equals = PROGRAM_WITH_OPTIONS(short_equals);
static const clopts_program_t prog_short_space = PROGRAM_WITH_OPTIONS(short_space);
static const clopts_program_t prog_env_empty = PROGRAM_WITH_OPTIONS(env_empty);

NAOS_TEST(option_name_malformed_is_invalid)
{
    CHECK_DECL_ERROR(prog_long_empty, CLOPTS_DECL_INVALID_NAME, NULL, &long_empty[0], NULL);
    CHECK_DECL_ERROR(prog_long_dash, CLOPTS_DECL_INVALID_NAME, NULL, &long_dash[0], NULL);
    CHECK_DECL_ERROR(prog_long_equals, CLOPTS_DECL_INVALID_NAME, NULL, &long_equals[0], NULL);
    CHECK_DECL_ERROR(prog_long_space, CLOPTS_DECL_INVALID_NAME, NULL, &long_space[0], NULL);
    CHECK_DECL_ERROR(prog_short_dash, CLOPTS_DECL_INVALID_NAME, NULL, &short_dash[0], NULL);
    CHECK_DECL_ERROR(prog_short_equals, CLOPTS_DECL_INVALID_NAME, NULL, &short_equals[0], NULL);
    CHECK_DECL_ERROR(prog_short_space, CLOPTS_DECL_INVALID_NAME, NULL, &short_space[0], NULL);
}

NAOS_TEST(empty_env_name_is_invalid)
{
    CHECK_DECL_ERROR(prog_env_empty, CLOPTS_DECL_INVALID_NAME, NULL, &env_empty[0], NULL);
}

/* ---- DUPLICATE_OPTION (2.3) -------------------------------------------- */

static const clopts_option_t dup_long_global[] = {
    { .long_name = "name", .kind = CLOPTS_FLAG },
    { .long_name = "other", .kind = CLOPTS_FLAG },
    { .long_name = "name", .kind = CLOPTS_COUNT },
};
static const clopts_program_t prog_dup_long_global = PROGRAM_WITH_OPTIONS(dup_long_global);

static const clopts_option_t dup_short_global[] = {
    { .long_name = "alpha", .short_name = 'a', .kind = CLOPTS_FLAG },
    { .long_name = "beta", .short_name = 'a', .kind = CLOPTS_FLAG },
};
static const clopts_program_t prog_dup_short_global = PROGRAM_WITH_OPTIONS(dup_short_global);

static const clopts_option_t dup_cmd_scope_options[] = {
    { .long_name = "fast", .short_name = 'f', .kind = CLOPTS_FLAG },
    { .long_name = "slow", .short_name = 'f', .kind = CLOPTS_FLAG },
};
static const clopts_command_t dup_cmd_scope_commands[] = {
    { .name = "run", .options = dup_cmd_scope_options, .option_count = 2 },
};
static const clopts_program_t prog_dup_cmd_scope = {
    .name = "prog", .commands = dup_cmd_scope_commands, .command_count = 1,
};

static const clopts_option_t dup_cross_global[] = {
    { .long_name = "verbose", .short_name = 'v', .kind = CLOPTS_COUNT },
};
static const clopts_option_t dup_cross_long_cmd[] = {
    { .long_name = "quiet", .short_name = 'q', .kind = CLOPTS_FLAG },
    { .long_name = "verbose", .kind = CLOPTS_FLAG },
};
static const clopts_option_t dup_cross_short_cmd[] = {
    { .long_name = "version-file", .short_name = 'v', .kind = CLOPTS_VALUE },
};
static const clopts_command_t dup_cross_long_commands[] = {
    { .name = "run", .options = dup_cross_long_cmd, .option_count = 2 },
};
static const clopts_command_t dup_cross_short_commands[] = {
    { .name = "run", .options = dup_cross_short_cmd, .option_count = 1 },
};
static const clopts_program_t prog_dup_cross_long = {
    .name = "prog",
    .options = dup_cross_global, .option_count = 1,
    .commands = dup_cross_long_commands, .command_count = 1,
};
static const clopts_program_t prog_dup_cross_short = {
    .name = "prog",
    .options = dup_cross_global, .option_count = 1,
    .commands = dup_cross_short_commands, .command_count = 1,
};

NAOS_TEST(duplicate_option_in_global_scope)
{
    CHECK_DECL_ERROR(prog_dup_long_global, CLOPTS_DECL_DUPLICATE_OPTION, NULL,
                     &dup_long_global[2], NULL);
    CHECK_DECL_ERROR(prog_dup_short_global, CLOPTS_DECL_DUPLICATE_OPTION, NULL,
                     &dup_short_global[1], NULL);
}

NAOS_TEST(duplicate_option_in_command_scope)
{
    CHECK_DECL_ERROR(prog_dup_cmd_scope, CLOPTS_DECL_DUPLICATE_OPTION, &dup_cmd_scope_commands[0],
                     &dup_cmd_scope_options[1], NULL);
}

NAOS_TEST(duplicate_option_between_global_and_command)
{
    CHECK_DECL_ERROR(prog_dup_cross_long, CLOPTS_DECL_DUPLICATE_OPTION,
                     &dup_cross_long_commands[0], &dup_cross_long_cmd[1], NULL);
    CHECK_DECL_ERROR(prog_dup_cross_short, CLOPTS_DECL_DUPLICATE_OPTION,
                     &dup_cross_short_commands[0], &dup_cross_short_cmd[0], NULL);
}

static const clopts_option_t shared_opt_a[] = { { .long_name = "force", .short_name = 'f' } };
static const clopts_option_t shared_opt_b[] = { { .long_name = "force", .short_name = 'f' } };
static const clopts_command_t shared_commands[] = {
    { .name = "add", .options = shared_opt_a, .option_count = 1 },
    { .name = "remove", .options = shared_opt_b, .option_count = 1 },
};
static const clopts_program_t prog_shared_between_commands = {
    .name = "prog", .commands = shared_commands, .command_count = 2,
};

/* Names that differ only in case are different names. */
static const clopts_option_t case_options[] = {
    { .long_name = "name", .short_name = 'n', .kind = CLOPTS_FLAG },
    { .long_name = "Name", .short_name = 'N', .kind = CLOPTS_FLAG },
};
static const clopts_program_t prog_case = PROGRAM_WITH_OPTIONS(case_options);

NAOS_TEST(same_option_in_different_commands_is_allowed)
{
    CHECK_DECL_OK(prog_shared_between_commands);
    CHECK_DECL_OK(prog_case);
}

/* ---- DUPLICATE_COMMAND (2.4) ------------------------------------------- */

static const clopts_command_t dup_commands[] = {
    { .name = "push" },
    { .name = "pull" },
    { .name = "push" },
};
static const clopts_program_t prog_dup_commands = {
    .name = "prog", .commands = dup_commands, .command_count = 3,
};

static const clopts_command_t case_commands[] = { { .name = "push" }, { .name = "Push" } };
static const clopts_program_t prog_case_commands = {
    .name = "prog", .commands = case_commands, .command_count = 2,
};

NAOS_TEST(duplicate_command_name)
{
    CHECK_DECL_ERROR(prog_dup_commands, CLOPTS_DECL_DUPLICATE_COMMAND, &dup_commands[2], NULL,
                     NULL);
    CHECK_DECL_OK(prog_case_commands);
}

/* ---- RESERVED_NAME (2.5) ----------------------------------------------- */

static const clopts_option_t reserved_long_help[] = { { .long_name = "help", .kind = CLOPTS_FLAG } };
static const clopts_option_t reserved_short_h[] = {
    { .long_name = "host", .short_name = 'h', .kind = CLOPTS_VALUE },
};
static const clopts_option_t own_version[] = {
    { .long_name = "version", .kind = CLOPTS_FLAG, .help = "Our own version flag." },
};
static const clopts_program_t prog_reserved_long_help = PROGRAM_WITH_OPTIONS(reserved_long_help);
static const clopts_program_t prog_reserved_short_h = PROGRAM_WITH_OPTIONS(reserved_short_h);
static const clopts_program_t prog_version_reserved = {
    .name = "prog", .version = "0.1.0", .options = own_version, .option_count = 1,
};
static const clopts_program_t prog_version_free = {
    .name = "prog", .version = NULL, .options = own_version, .option_count = 1,
};

static const clopts_command_t help_command[] = { { .name = "build" }, { .name = "help" } };
static const clopts_program_t prog_help_command = {
    .name = "prog", .commands = help_command, .command_count = 2,
};

static const clopts_option_t cmd_help_option[] = { { .long_name = "help", .kind = CLOPTS_FLAG } };
static const clopts_command_t cmd_help_commands[] = {
    { .name = "run", .options = cmd_help_option, .option_count = 1 },
};
static const clopts_program_t prog_cmd_help_option = {
    .name = "prog", .commands = cmd_help_commands, .command_count = 1,
};

NAOS_TEST(reserved_option_names)
{
    CHECK_DECL_ERROR(prog_reserved_long_help, CLOPTS_DECL_RESERVED_NAME, NULL,
                     &reserved_long_help[0], NULL);
    CHECK_DECL_ERROR(prog_reserved_short_h, CLOPTS_DECL_RESERVED_NAME, NULL, &reserved_short_h[0],
                     NULL);
    CHECK_DECL_ERROR(prog_cmd_help_option, CLOPTS_DECL_RESERVED_NAME, &cmd_help_commands[0],
                     &cmd_help_option[0], NULL);
}

NAOS_TEST(version_is_reserved_only_with_a_version_string)
{
    CHECK_DECL_ERROR(prog_version_reserved, CLOPTS_DECL_RESERVED_NAME, NULL, &own_version[0], NULL);
    CHECK_DECL_OK(prog_version_free);
}

NAOS_TEST(help_command_is_reserved)
{
    CHECK_DECL_ERROR(prog_help_command, CLOPTS_DECL_RESERVED_NAME, &help_command[1], NULL, NULL);
}

/* ---- NOT_NEGATABLE (2.8) ----------------------------------------------- */

static const clopts_option_t negatable_count[] = {
    { .long_name = "level", .kind = CLOPTS_COUNT, .negatable = true },
};
static const clopts_option_t negatable_value[] = {
    { .long_name = "path", .kind = CLOPTS_VALUE, .negatable = true },
};
static const clopts_program_t prog_negatable_count = PROGRAM_WITH_OPTIONS(negatable_count);
static const clopts_program_t prog_negatable_value = PROGRAM_WITH_OPTIONS(negatable_value);

NAOS_TEST(negatable_on_non_flag)
{
    CHECK_DECL_ERROR(prog_negatable_count, CLOPTS_DECL_NOT_NEGATABLE, NULL, &negatable_count[0],
                     NULL);
    CHECK_DECL_ERROR(prog_negatable_value, CLOPTS_DECL_NOT_NEGATABLE, NULL, &negatable_value[0],
                     NULL);
}

/* ---- NEGATION_CONFLICT (2.9) ------------------------------------------- */

static const clopts_option_t negation_after_flag[] = {
    { .long_name = "cache", .kind = CLOPTS_FLAG, .negatable = true },
    { .long_name = "no-cache", .kind = CLOPTS_FLAG },
};
static const clopts_option_t negation_before_flag[] = {
    { .long_name = "no-cache", .kind = CLOPTS_VALUE },
    { .long_name = "cache", .kind = CLOPTS_FLAG, .negatable = true },
};
static const clopts_option_t no_negation_without_negatable[] = {
    { .long_name = "cache", .kind = CLOPTS_FLAG },
    { .long_name = "no-cache", .kind = CLOPTS_FLAG },
};
static const clopts_program_t prog_negation_after = PROGRAM_WITH_OPTIONS(negation_after_flag);
static const clopts_program_t prog_negation_before = PROGRAM_WITH_OPTIONS(negation_before_flag);
static const clopts_program_t prog_no_negation = PROGRAM_WITH_OPTIONS(no_negation_without_negatable);

static const clopts_option_t negation_cross_global[] = {
    { .long_name = "color", .kind = CLOPTS_FLAG, .negatable = true },
};
static const clopts_option_t negation_cross_cmd[] = { { .long_name = "no-color" } };
static const clopts_command_t negation_cross_commands[] = {
    { .name = "show", .options = negation_cross_cmd, .option_count = 1 },
};
static const clopts_program_t prog_negation_cross = {
    .name = "prog",
    .options = negation_cross_global, .option_count = 1,
    .commands = negation_cross_commands, .command_count = 1,
};

NAOS_TEST(negation_conflict_in_same_scope)
{
    CHECK_DECL_ERROR(prog_negation_after, CLOPTS_DECL_NEGATION_CONFLICT, NULL,
                     &negation_after_flag[1], NULL);
    CHECK_DECL_ERROR(prog_negation_before, CLOPTS_DECL_NEGATION_CONFLICT, NULL,
                     &negation_before_flag[1], NULL);
    CHECK_DECL_OK(prog_no_negation);
}

NAOS_TEST(negation_conflict_between_global_and_command)
{
    CHECK_DECL_ERROR(prog_negation_cross, CLOPTS_DECL_NEGATION_CONFLICT,
                     &negation_cross_commands[0], &negation_cross_cmd[0], NULL);
}

/* ---- DEFAULT_NOT_ALLOWED (2.10) ---------------------------------------- */

static const clopts_option_t default_flag[] = {
    { .long_name = "dry", .kind = CLOPTS_FLAG, .default_value = "1" },
};
static const clopts_option_t default_count[] = {
    { .short_name = 'v', .kind = CLOPTS_COUNT, .default_value = "2" },
};
static const clopts_program_t prog_default_flag = PROGRAM_WITH_OPTIONS(default_flag);
static const clopts_program_t prog_default_count = PROGRAM_WITH_OPTIONS(default_count);

NAOS_TEST(default_on_flag_or_count)
{
    CHECK_DECL_ERROR(prog_default_flag, CLOPTS_DECL_DEFAULT_NOT_ALLOWED, NULL, &default_flag[0],
                     NULL);
    CHECK_DECL_ERROR(prog_default_count, CLOPTS_DECL_DEFAULT_NOT_ALLOWED, NULL, &default_count[0],
                     NULL);
}

/* ---- ENV_NOT_ALLOWED (2.11) -------------------------------------------- */

static const clopts_option_t env_flag[] = {
    { .long_name = "dry", .kind = CLOPTS_FLAG, .env = "PROG_DRY" },
};
static const clopts_option_t env_count[] = {
    { .short_name = 'v', .kind = CLOPTS_COUNT, .env = "PROG_VERBOSE" },
};
static const clopts_program_t prog_env_flag = PROGRAM_WITH_OPTIONS(env_flag);
static const clopts_program_t prog_env_count = PROGRAM_WITH_OPTIONS(env_count);

NAOS_TEST(env_on_flag_or_count)
{
    CHECK_DECL_ERROR(prog_env_flag, CLOPTS_DECL_ENV_NOT_ALLOWED, NULL, &env_flag[0], NULL);
    CHECK_DECL_ERROR(prog_env_count, CLOPTS_DECL_ENV_NOT_ALLOWED, NULL, &env_count[0], NULL);
}

/* ---- REQUIRED_WITH_DEFAULT (2.12) -------------------------------------- */

static const clopts_option_t required_default_value[] = {
    { .long_name = "mode", .kind = CLOPTS_VALUE, .required = true, .default_value = "fast" },
};
static const clopts_option_t required_default_list[] = {
    { .long_name = "tags", .kind = CLOPTS_LIST, .required = true, .default_value = "a" },
};
static const clopts_program_t prog_required_default_value =
    PROGRAM_WITH_OPTIONS(required_default_value);
static const clopts_program_t prog_required_default_list =
    PROGRAM_WITH_OPTIONS(required_default_list);

NAOS_TEST(required_with_default)
{
    CHECK_DECL_ERROR(prog_required_default_value, CLOPTS_DECL_REQUIRED_WITH_DEFAULT, NULL,
                     &required_default_value[0], NULL);
    CHECK_DECL_ERROR(prog_required_default_list, CLOPTS_DECL_REQUIRED_WITH_DEFAULT, NULL,
                     &required_default_list[0], NULL);
}

/* ---- REQUIRED_NOT_ALLOWED (2.13) --------------------------------------- */

static const clopts_option_t required_flag[] = {
    { .long_name = "yes", .kind = CLOPTS_FLAG, .required = true },
};
static const clopts_option_t required_count[] = {
    { .short_name = 'v', .kind = CLOPTS_COUNT, .required = true },
};
static const clopts_program_t prog_required_flag = PROGRAM_WITH_OPTIONS(required_flag);
static const clopts_program_t prog_required_count = PROGRAM_WITH_OPTIONS(required_count);

NAOS_TEST(required_on_flag_or_count)
{
    CHECK_DECL_ERROR(prog_required_flag, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL, &required_flag[0],
                     NULL);
    CHECK_DECL_ERROR(prog_required_count, CLOPTS_DECL_REQUIRED_NOT_ALLOWED, NULL,
                     &required_count[0], NULL);
}

/* ---- POSITIONAL_ORDER (2.14) ------------------------------------------- */

static const clopts_positional_t order_positionals[] = {
    { .name = "A", .required = true },
    { .name = "B", .required = false },
    { .name = "C", .required = true },
};
static const clopts_program_t prog_order = {
    .name = "prog", .positionals = order_positionals, .positional_count = 3,
};
static const clopts_command_t order_commands[] = {
    { .name = "run", .positionals = order_positionals, .positional_count = 3 },
};
static const clopts_program_t prog_order_in_command = {
    .name = "prog", .commands = order_commands, .command_count = 1,
};

NAOS_TEST(required_positional_after_optional)
{
    CHECK_DECL_ERROR(prog_order, CLOPTS_DECL_POSITIONAL_ORDER, NULL, NULL, &order_positionals[2]);
    CHECK_DECL_ERROR(prog_order_in_command, CLOPTS_DECL_POSITIONAL_ORDER, &order_commands[0], NULL,
                     &order_positionals[2]);
}

/* ---- VARIADIC_POSITION (2.15) ------------------------------------------ */

static const clopts_positional_t variadic_not_last[] = {
    { .name = "FILES", .variadic = true },
    { .name = "DEST" },
};
static const clopts_positional_t two_variadics[] = {
    { .name = "SRC", .required = true },
    { .name = "A", .variadic = true },
    { .name = "B", .variadic = true },
};
static const clopts_program_t prog_variadic_not_last = {
    .name = "prog", .positionals = variadic_not_last, .positional_count = 2,
};
static const clopts_command_t two_variadic_commands[] = {
    { .name = "run", .positionals = two_variadics, .positional_count = 3 },
};
static const clopts_program_t prog_two_variadics = {
    .name = "prog", .commands = two_variadic_commands, .command_count = 1,
};

NAOS_TEST(variadic_not_last_or_repeated)
{
    CHECK_DECL_ERROR(prog_variadic_not_last, CLOPTS_DECL_VARIADIC_POSITION, NULL, NULL,
                     &variadic_not_last[0]);
    CHECK_DECL_ERROR(prog_two_variadics, CLOPTS_DECL_VARIADIC_POSITION, &two_variadic_commands[0],
                     NULL, &two_variadics[1]);
}

/* ---- program positionals with subcommands are ignored (1.4) ------------ */

static const clopts_command_t ignored_positionals_commands[] = { { .name = "run" } };
static const clopts_program_t prog_ignored_positionals = {
    .name = "prog",
    .positionals = order_positionals, .positional_count = 3,
    .commands = ignored_positionals_commands, .command_count = 1,
};

NAOS_TEST(program_positionals_are_not_checked_with_subcommands)
{
    CHECK_DECL_OK(prog_ignored_positionals);
}

/* ---- first violation only (2.18) --------------------------------------- */

static const clopts_option_t first_global[] = {
    { .long_name = "dry", .kind = CLOPTS_FLAG, .env = "PROG_DRY" },
};
static const clopts_option_t first_cmd[] = { { .long_name = "", .kind = CLOPTS_FLAG } };
static const clopts_command_t first_commands[] = {
    { .name = "run", .options = first_cmd, .option_count = 1 },
};
static const clopts_program_t prog_two_violations = {
    .name = "prog",
    .options = first_global, .option_count = 1,
    .commands = first_commands, .command_count = 1,
};

/* One option breaks two rules: the earlier row of the rule table wins. */
static const clopts_option_t two_rules[] = {
    { .long_name = "help", .kind = CLOPTS_COUNT, .default_value = "1" },
};
static const clopts_program_t prog_two_rules = PROGRAM_WITH_OPTIONS(two_rules);

/* Each command is checked in full (name, options, positionals) before the next one. */
static const clopts_command_t name_before_positionals[] = {
    { .name = "ok", .positionals = variadic_not_last, .positional_count = 2 },
    { .name = "" },
};
static const clopts_program_t prog_name_before_positionals = {
    .name = "prog", .commands = name_before_positionals, .command_count = 2,
};

NAOS_TEST(only_the_first_violation_is_reported)
{
    CHECK_DECL_ERROR(prog_two_violations, CLOPTS_DECL_ENV_NOT_ALLOWED, NULL, &first_global[0],
                     NULL);
    CHECK_DECL_ERROR(prog_two_rules, CLOPTS_DECL_RESERVED_NAME, NULL, &two_rules[0], NULL);
    CHECK_DECL_ERROR(prog_name_before_positionals, CLOPTS_DECL_VARIADIC_POSITION,
                     &name_before_positionals[0], NULL, &variadic_not_last[0]);
}

/* ---- lookups (4.5, 5.7) ------------------------------------------------ */

NAOS_TEST(find_command_is_exact_and_case_sensitive)
{
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(&fixture_tool, "build"),
                      &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD]);
    NAOS_CHECK_PTR_EQ(clopts_parser_find_command(&fixture_tool, "cat"),
                      &fixture_tool_commands[FIXTURE_TOOL_CMD_CAT]);
    NAOS_CHECK_NULL(clopts_parser_find_command(&fixture_tool, "Build"));
    NAOS_CHECK_NULL(clopts_parser_find_command(&fixture_tool, "bui"));
    NAOS_CHECK_NULL(clopts_parser_find_command(&fixture_tool, "builder"));
    NAOS_CHECK_NULL(clopts_parser_find_command(&fixture_tool, ""));
    NAOS_CHECK_NULL(clopts_parser_find_command(&fixture_filt, "build"));
}

NAOS_TEST(find_long_searches_command_then_global)
{
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    clopts_parser_ref_t ref;

    ref = clopts_parser_find_long(&fixture_tool, build, "output", 6);
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT]);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, FIXTURE_BUILD_OPT_OUTPUT);
    NAOS_CHECK_FALSE(ref.negated);

    ref = clopts_parser_find_long(&fixture_tool, build, "config", 6);
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_tool_options[FIXTURE_TOOL_OPT_CONFIG]);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, FIXTURE_TOOL_OPT_CONFIG);
    NAOS_CHECK_FALSE(ref.negated);

    /* Without a command only the global options are searched. */
    ref = clopts_parser_find_long(&fixture_tool, NULL, "output", 6);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_long(&fixture_tool, NULL, "verbose", 7);
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE]);
    NAOS_CHECK_TRUE(ref.global);

    /* A program without subcommands. */
    ref = clopts_parser_find_long(&fixture_filt, NULL, "lines", 5);
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_filt_options[FIXTURE_FILT_OPT_LINES]);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, FIXTURE_FILT_OPT_LINES);
}

NAOS_TEST(find_long_uses_name_len_exactly)
{
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    clopts_parser_ref_t ref;

    /* The name is the first name_len bytes, e.g. the part before '='. */
    ref = clopts_parser_find_long(&fixture_tool, build, "output=x", 6);
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_build_options[FIXTURE_BUILD_OPT_OUTPUT]);

    /* No prefix match, no case folding, no longer names. */
    ref = clopts_parser_find_long(&fixture_tool, build, "out", 3);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_long(&fixture_tool, build, "output", 3);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_long(&fixture_tool, build, "Output", 6);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_long(&fixture_tool, build, "outputs", 7);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_long(&fixture_tool, build, "", 0);
    NAOS_CHECK_NULL(ref.option);
    NAOS_CHECK_FALSE(ref.negated);
}

NAOS_TEST(find_negation_finds_negatable_flags_only)
{
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    clopts_parser_ref_t ref;

    ref = clopts_parser_find_negation(&fixture_tool, build, "no-color", 8);
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_tool_options[FIXTURE_TOOL_OPT_COLOR]);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, FIXTURE_TOOL_OPT_COLOR);
    NAOS_CHECK_TRUE(ref.negated);

    ref = clopts_parser_find_negation(&fixture_tool, NULL, "no-color=1", 8);
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_tool_options[FIXTURE_TOOL_OPT_COLOR]);
    NAOS_CHECK_TRUE(ref.negated);

    /* Not negatable, not a FLAG, unknown, malformed. */
    ref = clopts_parser_find_negation(&fixture_tool, build, "no-output", 9);
    NAOS_CHECK_NULL(ref.option);
    NAOS_CHECK_FALSE(ref.negated);
    ref = clopts_parser_find_negation(&fixture_tool, build, "no-verbose", 10);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_negation(&fixture_tool, build, "no-colour", 9);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_negation(&fixture_tool, build, "no-Color", 8);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_negation(&fixture_tool, build, "no-", 3);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_negation(&fixture_tool, build, "color", 5);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_negation(&fixture_tool, build, "no-color", 7);
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_negation(&fixture_tool, build, "no", 2);
    NAOS_CHECK_NULL(ref.option);
}

static const clopts_option_t negatable_cmd_options[] = {
    { .long_name = "cache", .short_name = 'c', .kind = CLOPTS_FLAG, .negatable = true },
};
static const clopts_command_t negatable_cmd_commands[] = {
    { .name = "run", .options = negatable_cmd_options, .option_count = 1 },
};
static const clopts_program_t prog_negatable_cmd = {
    .name = "prog", .commands = negatable_cmd_commands, .command_count = 1,
};

NAOS_TEST(find_negation_and_short_in_command_scope)
{
    clopts_parser_ref_t ref;

    CHECK_DECL_OK(prog_negatable_cmd);

    ref = clopts_parser_find_negation(&prog_negatable_cmd, &negatable_cmd_commands[0], "no-cache",
                                      8);
    NAOS_CHECK_PTR_EQ(ref.option, &negatable_cmd_options[0]);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, 0);
    NAOS_CHECK_TRUE(ref.negated);

    ref = clopts_parser_find_negation(&prog_negatable_cmd, NULL, "no-cache", 8);
    NAOS_CHECK_NULL(ref.option);

    ref = clopts_parser_find_short(&prog_negatable_cmd, &negatable_cmd_commands[0], 'c');
    NAOS_CHECK_PTR_EQ(ref.option, &negatable_cmd_options[0]);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_FALSE(ref.negated);
}

NAOS_TEST(find_short_searches_command_then_global)
{
    const clopts_command_t *build = &fixture_tool_commands[FIXTURE_TOOL_CMD_BUILD];
    clopts_parser_ref_t ref;

    ref = clopts_parser_find_short(&fixture_tool, build, 'j');
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_build_options[FIXTURE_BUILD_OPT_JOBS]);
    NAOS_CHECK_FALSE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, FIXTURE_BUILD_OPT_JOBS);
    NAOS_CHECK_FALSE(ref.negated);

    ref = clopts_parser_find_short(&fixture_tool, build, 'v');
    NAOS_CHECK_PTR_EQ(ref.option, &fixture_tool_options[FIXTURE_TOOL_OPT_VERBOSE]);
    NAOS_CHECK_TRUE(ref.global);
    NAOS_CHECK_UINT_EQ(ref.index, FIXTURE_TOOL_OPT_VERBOSE);

    ref = clopts_parser_find_short(&fixture_tool, NULL, 'o');
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_short(&fixture_tool, build, 'O');
    NAOS_CHECK_NULL(ref.option);
    ref = clopts_parser_find_short(&fixture_tool, build, 'x');
    NAOS_CHECK_NULL(ref.option);
    /* '\0' (an option without a short name) never matches. */
    ref = clopts_parser_find_short(&fixture_tool, build, '\0');
    NAOS_CHECK_NULL(ref.option);
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(fixture_programs_are_shaped_as_designed),
    NAOS_ADD_TEST(valid_fixture_declarations_are_ok),
    NAOS_ADD_TEST(validate_accepts_null_error),
    NAOS_ADD_TEST(validate_leaves_error_untouched_when_valid),
    NAOS_ADD_TEST(error_fields_for_usage_errors_are_reset),
    NAOS_ADD_TEST(program_name_null_or_empty_is_invalid),
    NAOS_ADD_TEST(command_name_malformed_is_invalid),
    NAOS_ADD_TEST(option_without_any_name_is_no_name),
    NAOS_ADD_TEST(option_name_malformed_is_invalid),
    NAOS_ADD_TEST(empty_env_name_is_invalid),
    NAOS_ADD_TEST(duplicate_option_in_global_scope),
    NAOS_ADD_TEST(duplicate_option_in_command_scope),
    NAOS_ADD_TEST(duplicate_option_between_global_and_command),
    NAOS_ADD_TEST(same_option_in_different_commands_is_allowed),
    NAOS_ADD_TEST(duplicate_command_name),
    NAOS_ADD_TEST(reserved_option_names),
    NAOS_ADD_TEST(version_is_reserved_only_with_a_version_string),
    NAOS_ADD_TEST(help_command_is_reserved),
    NAOS_ADD_TEST(negatable_on_non_flag),
    NAOS_ADD_TEST(negation_conflict_in_same_scope),
    NAOS_ADD_TEST(negation_conflict_between_global_and_command),
    NAOS_ADD_TEST(default_on_flag_or_count),
    NAOS_ADD_TEST(env_on_flag_or_count),
    NAOS_ADD_TEST(required_with_default),
    NAOS_ADD_TEST(required_on_flag_or_count),
    NAOS_ADD_TEST(required_positional_after_optional),
    NAOS_ADD_TEST(variadic_not_last_or_repeated),
    NAOS_ADD_TEST(program_positionals_are_not_checked_with_subcommands),
    NAOS_ADD_TEST(only_the_first_violation_is_reported),
    NAOS_ADD_TEST(find_command_is_exact_and_case_sensitive),
    NAOS_ADD_TEST(find_long_searches_command_then_global),
    NAOS_ADD_TEST(find_long_uses_name_len_exactly),
    NAOS_ADD_TEST(find_negation_finds_negatable_flags_only),
    NAOS_ADD_TEST(find_negation_and_short_in_command_scope),
    NAOS_ADD_TEST(find_short_searches_command_then_global)
)
