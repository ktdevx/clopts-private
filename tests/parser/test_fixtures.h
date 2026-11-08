/*
 * Shared fixtures for the parser tests: fictional program declarations and a
 * mock environment lookup.
 *
 * The programs are made up for the tests and do not describe any real tool.
 * Only valid declarations live here; a suite that needs an invalid or special
 * declaration defines its own static const table in its own file.
 *
 *   tool 1.2.3   with subcommands
 *     global: -v/--verbose COUNT, --color negatable FLAG,
 *             --config VALUE (env TOOL_CONFIG)
 *     build:  -o/--output VALUE required, -k/--keys LIST ',', -j COUNT max 8,
 *             SRC required, EXTRA variadic, with a handler
 *     copy:   SRC required, DST required
 *     cat:    --tags LIST ',' (default "a,b", env TOOL_TAGS), FILE variadic
 *   filt         without subcommands, no version, with a handler
 *     -n/--lines VALUE (default "10", env FILT_LINES), FILE variadic
 *   strict       with one subcommand and a required global option
 *     global: --token VALUE required
 *     run:    no options, no positionals
 */

#ifndef CLOPTS_TESTS_PARSER_TEST_FIXTURES_H
#define CLOPTS_TESTS_PARSER_TEST_FIXTURES_H

#include "clopts/clopts.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- tool -------------------------------------------------------------- */

enum
{
    FIXTURE_TOOL_OPT_VERBOSE,
    FIXTURE_TOOL_OPT_COLOR,
    FIXTURE_TOOL_OPT_CONFIG,
    FIXTURE_TOOL_OPT_COUNT
};

enum
{
    FIXTURE_TOOL_CMD_BUILD,
    FIXTURE_TOOL_CMD_COPY,
    FIXTURE_TOOL_CMD_CAT,
    FIXTURE_TOOL_CMD_COUNT
};

enum
{
    FIXTURE_BUILD_OPT_OUTPUT,
    FIXTURE_BUILD_OPT_KEYS,
    FIXTURE_BUILD_OPT_JOBS,
    FIXTURE_BUILD_OPT_COUNT
};

enum
{
    FIXTURE_BUILD_POS_SRC,
    FIXTURE_BUILD_POS_EXTRA,
    FIXTURE_BUILD_POS_COUNT
};

enum
{
    FIXTURE_COPY_POS_SRC,
    FIXTURE_COPY_POS_DST,
    FIXTURE_COPY_POS_COUNT
};

enum
{
    FIXTURE_CAT_OPT_TAGS,
    FIXTURE_CAT_OPT_COUNT
};

enum
{
    FIXTURE_CAT_POS_FILE,
    FIXTURE_CAT_POS_COUNT
};

extern const clopts_option_t fixture_tool_options[FIXTURE_TOOL_OPT_COUNT];
extern const clopts_option_t fixture_build_options[FIXTURE_BUILD_OPT_COUNT];
extern const clopts_positional_t fixture_build_positionals[FIXTURE_BUILD_POS_COUNT];
extern const clopts_positional_t fixture_copy_positionals[FIXTURE_COPY_POS_COUNT];
extern const clopts_option_t fixture_cat_options[FIXTURE_CAT_OPT_COUNT];
extern const clopts_positional_t fixture_cat_positionals[FIXTURE_CAT_POS_COUNT];
extern const clopts_command_t fixture_tool_commands[FIXTURE_TOOL_CMD_COUNT];
extern const clopts_program_t fixture_tool;

/* ---- filt -------------------------------------------------------------- */

enum
{
    FIXTURE_FILT_OPT_LINES,
    FIXTURE_FILT_OPT_COUNT
};

enum
{
    FIXTURE_FILT_POS_FILE,
    FIXTURE_FILT_POS_COUNT
};

extern const clopts_option_t fixture_filt_options[FIXTURE_FILT_OPT_COUNT];
extern const clopts_positional_t fixture_filt_positionals[FIXTURE_FILT_POS_COUNT];
extern const clopts_program_t fixture_filt;

/* ---- strict ------------------------------------------------------------ */

enum
{
    FIXTURE_STRICT_OPT_TOKEN,
    FIXTURE_STRICT_OPT_COUNT
};

enum
{
    FIXTURE_STRICT_CMD_RUN,
    FIXTURE_STRICT_CMD_COUNT
};

extern const clopts_option_t fixture_strict_options[FIXTURE_STRICT_OPT_COUNT];
extern const clopts_command_t fixture_strict_commands[FIXTURE_STRICT_CMD_COUNT];
extern const clopts_program_t fixture_strict;

/* ---- handler ----------------------------------------------------------- */

/*
 * Record kept by fixture_handler. fixture_filt passes &fixture_filt_handler_record
 * and the build command of fixture_tool passes &fixture_build_handler_record as
 * userdata. Reset a record with fixture_handler_reset() before use.
 */
typedef struct fixture_handler_record
{
    int calls;                          /* number of calls */
    const clopts_result_t *last_result; /* result of the last call */
    void *last_userdata;                /* userdata of the last call */
    int return_value;                   /* returned by every call */
} fixture_handler_record_t;

extern fixture_handler_record_t fixture_filt_handler_record;
extern fixture_handler_record_t fixture_build_handler_record;

/*
 * Handler used by the fixtures. userdata must point to a
 * fixture_handler_record_t; the call is recorded there and its return_value
 * is returned.
 */
int fixture_handler(const clopts_result_t *result, void *userdata);

/* Sets calls = 0, last_* = NULL and return_value. */
void fixture_handler_reset(fixture_handler_record_t *record, int return_value);

/* ---- mock environment -------------------------------------------------- */

/* One variable of the mock environment. value may be "" but not NULL. */
typedef struct fixture_env_var
{
    const char *name;
    const char *value;
} fixture_env_var_t;

/* Maximum number of looked-up names recorded in fixture_env_t.queried. */
#define FIXTURE_ENV_MAX_QUERIES 32

/*
 * Mock environment, passed as ctx to fixture_env_getenv. Initialize it with
 * fixture_env_init().
 */
typedef struct fixture_env
{
    const fixture_env_var_t *vars; /* name -> value table */
    size_t var_count;
    /* When true, the next lookup returns CLOPTS_ENV_FAILED and clears it. */
    bool fail_next;
    /* Names looked up so far, in order (the first FIXTURE_ENV_MAX_QUERIES). */
    const char *queried[FIXTURE_ENV_MAX_QUERIES];
    /* Total number of lookups, including those not stored in queried. */
    size_t query_count;
} fixture_env_t;

/* Sets the table, clears fail_next and the lookup record. */
void fixture_env_init(fixture_env_t *env, const fixture_env_var_t *vars, size_t var_count);

/*
 * clopts_getenv_fn over a fixture_env_t (ctx). Records name, then returns
 * CLOPTS_ENV_FAILED if fail_next was set, CLOPTS_ENV_FOUND with a malloc'ed
 * copy of the value if name is in the table (CLOPTS_ENV_FAILED if the copy
 * cannot be allocated), and CLOPTS_ENV_UNSET otherwise.
 */
clopts_env_status_t fixture_env_getenv(void *ctx, const char *name, char **out);

/* True when name was looked up (compared by string content). */
bool fixture_env_was_queried(const fixture_env_t *env, const char *name);

#endif /* CLOPTS_TESTS_PARSER_TEST_FIXTURES_H */
