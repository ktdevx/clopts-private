/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Definitions of the shared help test fixtures (see test_fixtures.h).
 *
 * The help texts of tool are those of the output example in the design
 * (TextLayout), so that the layout tests can compare against it. Each
 * program is declared with the builder functions without checks and ended
 * with test_decl_finish(); -h/--help and --version are those of
 * clopts_program_add_help_option() and clopts_program_add_version_option(),
 * and the help command (test_decl_help_command()) is declared last.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include <assert.h>
#include <string.h>

/* ---- shared steps ------------------------------------------------------ */

/*
 * Ends a fixture: when program has a declaration error, it is released and
 * the fixture struct (size bytes at f) is cleared. Returns the program or
 * NULL.
 */
static clopts_program_t *text_fixture_finish(clopts_program_t *program, void *f, size_t size)
{
    program = test_decl_finish(program);

    if (program == NULL)
    {
        memset(f, 0, size);
    }
    return program;
}

/* ---- tool -------------------------------------------------------------- */

void text_fixture_tool_init(text_fixture_tool_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_create("tool");
    clopts_program_set_description(f->program, "A sample tool for testing.");

    f->verbose = clopts_program_add_option(f->program, CLOPTS_OPTION_COUNT, "verbose", 'v');
    clopts_option_set_help(f->verbose, "Print more details");

    f->color = clopts_program_add_option(f->program, CLOPTS_OPTION_FLAG, "color", 'c');
    clopts_option_set_help(f->color, "Colorize the output");
    clopts_option_set_negatable(f->color, true);

    f->config = clopts_program_add_option(f->program, CLOPTS_OPTION_VALUE, "config", '\0');
    clopts_option_set_value_name(f->config, "FILE");
    clopts_option_set_help(f->config, "Configuration file");
    clopts_option_set_env(f->config, "TOOL_CONFIG");

    f->help = clopts_program_add_help_option(f->program);
    f->version = clopts_program_add_version_option(f->program);

    /* build */
    f->build = clopts_program_add_command(f->program, "build");
    clopts_command_set_summary(f->build, "Build an output from a source");
    clopts_command_set_description(f->build, "Build an output from SRC.");

    f->output = clopts_command_add_option(f->build, CLOPTS_OPTION_VALUE, "output", 'o');
    clopts_option_set_value_name(f->output, "FILE");
    clopts_option_set_help(f->output, "Write the result to FILE");
    clopts_option_set_required(f->output, true);

    f->keys = clopts_command_add_option(f->build, CLOPTS_OPTION_LIST, "keys", 'k');
    clopts_option_set_value_name(f->keys, "KEY");
    clopts_option_set_help(f->keys, "Keys to include");
    clopts_option_set_delimiter(f->keys, ',');
    clopts_option_set_default(f->keys, "a,b");

    f->build_src = clopts_command_add_positional(f->build, "SRC");
    clopts_positional_set_help(f->build_src, "Source file");
    clopts_positional_set_required(f->build_src, true);

    f->build_extra = clopts_command_add_positional(f->build, "EXTRA");
    clopts_positional_set_help(f->build_extra, "Extra inputs");
    clopts_positional_set_variadic(f->build_extra, true);

    /* copy */
    f->copy = clopts_program_add_command(f->program, "copy");
    clopts_command_set_summary(f->copy, "Copy a file");

    f->copy_src = clopts_command_add_positional(f->copy, "SRC");
    clopts_positional_set_help(f->copy_src, "File to copy");
    clopts_positional_set_required(f->copy_src, true);

    f->copy_dst = clopts_command_add_positional(f->copy, "DST");
    clopts_positional_set_help(f->copy_dst, "Destination");
    clopts_positional_set_required(f->copy_dst, true);

    f->help_command = test_decl_help_command(f->program);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
}

void text_fixture_tool_free(text_fixture_tool_t *f)
{
    assert(f != NULL);

    clopts_program_destroy(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- filt -------------------------------------------------------------- */

void text_fixture_filt_init(text_fixture_filt_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_create("filt");
    clopts_program_set_description(f->program, "Filter the lines of files.");

    f->lines = clopts_program_add_option(f->program, CLOPTS_OPTION_VALUE, "lines", 'n');
    clopts_option_set_value_name(f->lines, "N");
    clopts_option_set_help(f->lines, "Number of lines");
    clopts_option_set_default(f->lines, "10");
    clopts_option_set_env(f->lines, "FILT_LINES");

    f->file = clopts_program_add_positional(f->program, "FILE");
    clopts_positional_set_help(f->file, "Files to filter");
    clopts_positional_set_required(f->file, true);
    clopts_positional_set_variadic(f->file, true);

    f->help = clopts_program_add_help_option(f->program);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
}

void text_fixture_filt_free(text_fixture_filt_t *f)
{
    assert(f != NULL);

    clopts_program_destroy(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- bare -------------------------------------------------------------- */

void text_fixture_bare_init(text_fixture_bare_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_create("bare");
    f->help = clopts_program_add_help_option(f->program);
    f->version = clopts_program_add_version_option(f->program);

    f->run = clopts_program_add_command(f->program, "run");
    clopts_command_set_summary(f->run, "Run the task");

    f->help_command = test_decl_help_command(f->program);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
}

void text_fixture_bare_free(text_fixture_bare_t *f)
{
    assert(f != NULL);

    clopts_program_destroy(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- wide -------------------------------------------------------------- */

void text_fixture_wide_init(text_fixture_wide_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_create("wide");

    f->naive = clopts_program_add_option(f->program, CLOPTS_OPTION_FLAG, TEXT_FIXTURE_NAIVE, '\0');
    clopts_option_set_help(f->naive, "Trust every input");

    f->nihongo = clopts_program_add_option(f->program, CLOPTS_OPTION_VALUE, TEXT_FIXTURE_NIHONGO, 'j');
    clopts_option_set_value_name(f->nihongo, TEXT_FIXTURE_ATAI);
    clopts_option_set_help(f->nihongo, "Japanese text");

    f->raw = clopts_program_add_option(f->program, CLOPTS_OPTION_VALUE, "raw", '\0');
    clopts_option_set_value_name(f->raw, TEXT_FIXTURE_RAW_VALUE_NAME);
    clopts_option_set_help(f->raw, "Raw bytes");

    f->file = clopts_program_add_positional(f->program, TEXT_FIXTURE_FILE);
    clopts_positional_set_help(f->file, "Input files");
    clopts_positional_set_variadic(f->file, true);

    f->help = clopts_program_add_help_option(f->program);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
}

void text_fixture_wide_free(text_fixture_wide_t *f)
{
    assert(f != NULL);

    clopts_program_destroy(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- write callbacks --------------------------------------------------- */

void text_recorder_init(text_recorder_t *rec)
{
    assert(rec != NULL);

    memset(rec, 0, sizeof(*rec));
}

int text_recorder_write(void *ctx, const char *data, size_t len)
{
    text_recorder_t *rec = (text_recorder_t *)ctx;

    assert(rec != NULL);
    rec->calls++;

    if (len > (TEXT_RECORDER_SIZE - rec->len))
    {
        rec->overflow = true;
        return 1;
    }
    if (len > 0)
    {
        assert(data != NULL);
        memcpy(rec->buf + rec->len, data, len);
        rec->len += len;
        rec->buf[rec->len] = '\0';
    }
    return 0;
}

void text_failer_init(text_failer_t *failer, size_t fail_at)
{
    assert(failer != NULL);

    memset(failer, 0, sizeof(*failer));
    failer->fail_at = fail_at;
    text_recorder_init(&failer->written);
}

int text_failer_write(void *ctx, const char *data, size_t len)
{
    text_failer_t *failer = (text_failer_t *)ctx;

    assert(failer != NULL);
    failer->calls++;

    if (failer->failed)
    {
        failer->calls_after_failure++;
        return 1;
    }
    if ((failer->fail_at != 0) && (failer->calls == failer->fail_at))
    {
        failer->failed = true;
        return 1;
    }
    return text_recorder_write(&failer->written, data, len);
}
