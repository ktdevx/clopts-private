/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Shared fixtures for the help tests: fictional programs assembled with the
 * public builder functions, a recording write callback and a failing write
 * callback.
 *
 * The programs are made up for the tests and do not describe any real tool.
 * Only valid declarations live here; a suite that needs an invalid or a
 * one-row declaration builds its own program in its own file.
 *
 *   tool         with subcommands, description "A sample tool for testing."
 *     global: -v/--verbose COUNT, -c/--color negatable FLAG,
 *             --config VALUE <FILE> (env TOOL_CONFIG),
 *             -h/--help and --version (clopts/help.h)
 *     build:  summary and description,
 *             -o/--output VALUE <FILE> required,
 *             -k/--keys LIST <KEY> ',' (default "a,b"),
 *             SRC required, EXTRA optional variadic
 *     copy:   summary only, SRC required, DST required
 *     help:   the help command of clopts_help_command_create()
 *   filt         without subcommands, no --version, with a description
 *     -n/--lines VALUE <N> (default "10", env FILT_LINES), -h/--help,
 *     FILE required variadic
 *   bare         with one subcommand, no description
 *     global: -h/--help and --version only
 *     run:    summary only, no options, no positionals
 *     help:   the help command
 *   wide         without subcommands, no --version
 *     --naïve FLAG, -j/--日本語 VALUE <値>,
 *     --raw VALUE whose value name has bytes that are not valid UTF-8,
 *     -h/--help, ファイル optional variadic
 *
 * Each init function fills a struct of handles; the program is NULL only
 * when clopts_program_create() or clopts_program_add_command() failed (the
 * other handles are then NULL too).
 * The free function releases the program (NULL is fine) and clears the
 * struct.
 */

#ifndef CLOPTS_TESTS_HELP_TEST_FIXTURES_H
#define CLOPTS_TESTS_HELP_TEST_FIXTURES_H

#include "clopts/clopts.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- tool -------------------------------------------------------------- */

typedef struct help_fixture_tool
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of the init function succeeded */
    clopts_command_t *build;
    clopts_command_t *copy;
    clopts_command_t *help_command;  /* help [COMMAND] */
    const clopts_option_t *help;     /* global: -h/--help */
    const clopts_option_t *version;  /* global: --version */
    clopts_option_t *verbose;   /* global:  -v/--verbose COUNT */
    clopts_option_t *color;     /* global:  -c/--color negatable FLAG */
    clopts_option_t *config;    /* global:  --config VALUE <FILE>, env TOOL_CONFIG */
    clopts_option_t *output;    /* build: -o/--output VALUE <FILE> required */
    clopts_option_t *keys;      /* build: -k/--keys LIST <KEY> ',', default "a,b" */
    clopts_positional_t *build_src;    /* build: SRC required */
    clopts_positional_t *build_extra;  /* build: EXTRA optional variadic */
    clopts_positional_t *copy_src;     /* copy:  SRC required */
    clopts_positional_t *copy_dst;     /* copy:  DST required */
} help_fixture_tool_t;

void help_fixture_tool_init(help_fixture_tool_t *f);
void help_fixture_tool_free(help_fixture_tool_t *f);

/* ---- filt -------------------------------------------------------------- */

typedef struct help_fixture_filt
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of the init function succeeded */
    clopts_option_t *lines;     /* -n/--lines VALUE <N>, default "10", env FILT_LINES */
    const clopts_option_t *help;  /* -h/--help */
    clopts_positional_t *file;  /* FILE required variadic */
} help_fixture_filt_t;

void help_fixture_filt_init(help_fixture_filt_t *f);
void help_fixture_filt_free(help_fixture_filt_t *f);

/* ---- bare -------------------------------------------------------------- */

typedef struct help_fixture_bare
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of the init function succeeded */
    clopts_command_t *run;      /* summary only, no options, no positionals */
    clopts_command_t *help_command;  /* help [COMMAND] */
    const clopts_option_t *help;     /* -h/--help */
    const clopts_option_t *version;  /* --version */
} help_fixture_bare_t;

void help_fixture_bare_init(help_fixture_bare_t *f);
void help_fixture_bare_free(help_fixture_bare_t *f);

/* ---- wide -------------------------------------------------------------- */

/* UTF-8 spellings of the non-ASCII names of wide. */
#define HELP_FIXTURE_NAIVE "na\xC3\xAF" "ve"                     /* naïve (5 code points) */
#define HELP_FIXTURE_NIHONGO "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" /* 日本語 (3 code points) */
#define HELP_FIXTURE_ATAI "\xE5\x80\xA4"                          /* 値 (1 code point) */
#define HELP_FIXTURE_FILE "\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB" /* ファイル (4 code points) */
/*
 * Value name of --raw: 'B', a lone 0xFF, the overlong form C0 AF, the first
 * two bytes of a three-byte sequence (E6 97) and 'E'. Every invalid byte
 * counts as one column, so its display width is 7.
 */
#define HELP_FIXTURE_RAW_VALUE_NAME "B\xFF\xC0\xAF\xE6\x97" "E"

typedef struct help_fixture_wide
{
    clopts_program_t *program;
    bool ok;                    /* every builder call of the init function succeeded */
    clopts_option_t *naive;     /* --naïve FLAG */
    clopts_option_t *nihongo;   /* -j/--日本語 VALUE <値> */
    clopts_option_t *raw;       /* --raw VALUE <HELP_FIXTURE_RAW_VALUE_NAME> */
    const clopts_option_t *help;  /* -h/--help */
    clopts_positional_t *file;  /* ファイル optional variadic */
} help_fixture_wide_t;

void help_fixture_wide_init(help_fixture_wide_t *f);
void help_fixture_wide_free(help_fixture_wide_t *f);

/* ---- write callbacks --------------------------------------------------- */

/* Capacity of a help_recorder_t in bytes (excluding the terminating NUL). */
#define HELP_RECORDER_SIZE 8192

/*
 * Recording sink, passed as ctx to help_recorder_write. Initialize it with
 * help_recorder_init().
 */
typedef struct help_recorder
{
    char buf[HELP_RECORDER_SIZE + 1]; /* concatenated bytes, always NUL-terminated */
    size_t len;                       /* bytes stored in buf */
    size_t calls;                     /* number of callback invocations (len 0 included) */
    bool overflow;                    /* a write did not fit; it was rejected */
} help_recorder_t;

/* Empties the recorder and clears its counters. */
void help_recorder_init(help_recorder_t *rec);

/*
 * clopts_write_fn over a help_recorder_t (ctx). Counts the call and appends
 * the len bytes at data to buf, returning 0. When they do not fit in the
 * remaining capacity, nothing is appended, overflow is set and 1 is returned.
 */
int help_recorder_write(void *ctx, const char *data, size_t len);

/*
 * Failing sink, passed as ctx to help_failer_write. Initialize it with
 * help_failer_init().
 */
typedef struct help_failer
{
    size_t fail_at;              /* 1-based call that fails; 0 = never fail */
    size_t calls;                /* number of callback invocations, all of them */
    bool failed;                 /* the fail_at-th call has happened */
    size_t calls_after_failure;  /* invocations after the failing one */
    help_recorder_t written;     /* bytes accepted before the failure */
} help_failer_t;

/* Sets fail_at and clears the counters and the accepted bytes. */
void help_failer_init(help_failer_t *failer, size_t fail_at);

/*
 * clopts_write_fn over a help_failer_t (ctx). Counts the call. Once failed is
 * set, counts it in calls_after_failure and returns 1. The fail_at-th call
 * sets failed and returns 1 without storing anything. Any other call appends
 * the bytes to written (as help_recorder_write) and returns its result.
 */
int help_failer_write(void *ctx, const char *data, size_t len);

#endif /* CLOPTS_TESTS_HELP_TEST_FIXTURES_H */
