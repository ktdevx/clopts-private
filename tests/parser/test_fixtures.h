/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Shared fixtures for the parser tests: fictional programs assembled with the
 * builder functions, and a borrowing mock environment.
 *
 * The programs are made up for the tests and do not describe any real tool.
 * Only valid declarations live here; a suite that needs an invalid or special
 * declaration builds its own program in its own file.
 *
 *   tool         with subcommands
 *     global: -v/--verbose COUNT, --color negatable FLAG,
 *             --config VALUE (env TOOL_CONFIG),
 *             -h/--help and --version terminal FLAGs (clopts/help.h)
 *     build:  -o/--output VALUE required, -k/--keys LIST ',', -j COUNT max 8,
 *             SRC required, EXTRA variadic
 *     copy:   SRC required, DST required
 *     cat:    --tags LIST ',' (default "a,b", env TOOL_TAGS), FILE variadic
 *     help:   the help command of clopts_help_command_create() (COMMAND optional)
 *   filt         without subcommands, no --version
 *     -n/--lines VALUE (default "10", env FILT_LINES), -h/--help terminal,
 *     FILE variadic
 *   strict       with one subcommand and a required global option
 *     global: --token VALUE required, -h/--help terminal
 *     run:    no options, no positionals
 *
 * Each init function fills a struct of handles and sets ok when every
 * builder call succeeded. Every allocation of the assembly,
 * clopts_program_create() included, goes through the allocator set with
 * clopts_set_allocator(), so with the fault-injecting allocator of
 * tests/support/test_alloc.h each of them is a fault point. When an
 * allocation fails, the remaining builder calls get NULL handles and do
 * nothing; when the program could not be created at the end, everything is
 * released and the whole struct is NULL (ok false). The free function
 * releases the program (NULL is fine) and clears the struct.
 */

#ifndef CLOPTS_TESTS_PARSER_TEST_FIXTURES_H
#define CLOPTS_TESTS_PARSER_TEST_FIXTURES_H

#include "parser/parser_internal.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- tool -------------------------------------------------------------- */

typedef struct fixture_tool
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of the init function succeeded */
    clopts_command_t *root;
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_command_t *cat;
    clopts_command_t *help_command;  /* help [COMMAND] */
    const clopts_option_t *help;     /* root: -h/--help terminal FLAG */
    const clopts_option_t *version;  /* root: --version terminal FLAG */
    clopts_option_t *verbose;   /* root:  -v/--verbose COUNT */
    clopts_option_t *color;     /* root:  --color negatable FLAG */
    clopts_option_t *config;    /* root:  --config VALUE, env TOOL_CONFIG */
    clopts_option_t *output;    /* build: -o/--output VALUE required */
    clopts_option_t *keys;      /* build: -k/--keys LIST ',' */
    clopts_option_t *jobs;      /* build: -j COUNT max 8 */
    clopts_option_t *tags;      /* cat:   --tags LIST ',', default "a,b", env TOOL_TAGS */
    clopts_positional_t *build_src;    /* build: SRC required */
    clopts_positional_t *build_extra;  /* build: EXTRA variadic */
    clopts_positional_t *copy_src;     /* copy:  SRC required */
    clopts_positional_t *copy_dst;     /* copy:  DST required */
    clopts_positional_t *cat_file;     /* cat:   FILE variadic */
} fixture_tool_t;

void fixture_tool_init(fixture_tool_t *f);
void fixture_tool_free(fixture_tool_t *f);

/* ---- filt -------------------------------------------------------------- */

typedef struct fixture_filt
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of the init function succeeded */
    clopts_command_t *root;
    clopts_option_t *lines;     /* -n/--lines VALUE, default "10", env FILT_LINES */
    const clopts_option_t *help;  /* -h/--help terminal FLAG */
    clopts_positional_t *file;  /* FILE variadic */
} fixture_filt_t;

void fixture_filt_init(fixture_filt_t *f);
void fixture_filt_free(fixture_filt_t *f);

/* ---- strict ------------------------------------------------------------ */

typedef struct fixture_strict
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of the init function succeeded */
    clopts_command_t *root;
    clopts_command_t *run;
    clopts_option_t *token;     /* root: --token VALUE required */
    const clopts_option_t *help;  /* root: -h/--help terminal FLAG */
} fixture_strict_t;

void fixture_strict_init(fixture_strict_t *f);
void fixture_strict_free(fixture_strict_t *f);

/* ---- mock environment -------------------------------------------------- */

/* One variable of the mock environment. value may be "" but not NULL. */
typedef struct fixture_env_var
{
    const char *name;
    const char *value;
} fixture_env_var_t;

/* Maximum number of variables in a fixture_env_t. */
#define FIXTURE_ENV_MAX_VARS 8
/* Size of the lending buffer of one variable (value length + 1 at most). */
#define FIXTURE_ENV_VALUE_SIZE 64
/* Maximum number of looked-up names recorded in fixture_env_t.queried. */
#define FIXTURE_ENV_MAX_QUERIES 32
/* Size of one recorded name (longer names are truncated). */
#define FIXTURE_ENV_NAME_SIZE 32

/*
 * Mock environment, passed as ctx to fixture_env_getenv. Initialize it with
 * fixture_env_init().
 *
 * A found value is copied from the table into the variable's own buffer and
 * lent from there. With clobber set, every call first overwrites all buffers
 * (so a value lent by an earlier call is destroyed as soon as the next call
 * starts); fixture_env_clobber() destroys them on demand, e.g. right after a
 * parse returns. A library that keeps the borrowed pointer instead of copying
 * the string then shows "#..." instead of the value.
 */
typedef struct fixture_env
{
    const fixture_env_var_t *vars; /* name -> value table */
    size_t var_count;
    char buffers[FIXTURE_ENV_MAX_VARS][FIXTURE_ENV_VALUE_SIZE];
    /* When true, the next lookup returns CLOPTS_ENV_FAILED and clears it. */
    bool fail_next;
    /* When true, every lookup first overwrites all lending buffers. */
    bool clobber;
    /* Copies of the names looked up so far, in order (the first FIXTURE_ENV_MAX_QUERIES). */
    char queried[FIXTURE_ENV_MAX_QUERIES][FIXTURE_ENV_NAME_SIZE];
    /* Total number of lookups, including those not stored in queried. */
    size_t query_count;
} fixture_env_t;

/*
 * Sets the table (var_count <= FIXTURE_ENV_MAX_VARS, each value shorter than
 * FIXTURE_ENV_VALUE_SIZE; both asserted), clears fail_next, clobber, the
 * buffers and the lookup record.
 */
void fixture_env_init(fixture_env_t *env, const fixture_env_var_t *vars, size_t var_count);

/*
 * clopts_getenv_fn over a fixture_env_t (ctx). Overwrites the buffers when
 * clobber is set, records name, then returns CLOPTS_ENV_FAILED if fail_next
 * was set (clearing it), CLOPTS_ENV_FOUND with *value borrowed from the
 * variable's buffer if name is in the table, and CLOPTS_ENV_UNSET otherwise.
 */
clopts_env_status_t fixture_env_getenv(void *ctx, const char *name, const char **value);

/* Overwrites every lending buffer with '#' characters. */
void fixture_env_clobber(fixture_env_t *env);

/* True when name was looked up (compared by string content). */
bool fixture_env_was_queried(const fixture_env_t *env, const char *name);

/* Number of lookups of name among the recorded ones. */
size_t fixture_env_query_count(const fixture_env_t *env, const char *name);

#endif /* CLOPTS_TESTS_PARSER_TEST_FIXTURES_H */
