/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Definitions of the shared help test fixtures (see test_fixtures.h).
 *
 * The help texts of tool are those of the output example in the design
 * (TextLayout), so that the layout tests can compare against it. Each
 * program is created first (test_decl_begin()), then filled
 * (test_decl_command(), then test_decl_finish()); -h/--help and --version
 * come from test_decl.h and the help command from clopts/text/write.h, declared
 * last.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include <assert.h>
#include <string.h>

/* ---- shared steps ------------------------------------------------------ */

/*
 * Adds -h/--help and, when version is not NULL, --version to program as
 * global options (NULL program does nothing).
 */
static void text_fixture_add_terminals(clopts_program_t *program, const clopts_option_t **help,
                                       const clopts_option_t **version)
{
    if (program == NULL)
    {
        return;
    }
    *help = test_decl_global_attach(program, test_decl_help_option());
    if (version != NULL)
    {
        *version = test_decl_global_attach(program, test_decl_version_option());
    }
}

/*
 * Adds the remembered commands to program; on failure everything was
 * released, so the fixture struct (size bytes at f) is cleared.
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
    test_decl_reset();
    f->program = test_decl_begin("tool");
    if (f->program != NULL)
    {
        test_decl_check(clopts_program_set_description(f->program, "A sample tool for testing."));
    }

    f->verbose = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    test_decl_check(clopts_option_set_help(f->verbose, "Print more details"));
    f->verbose = test_decl_global_attach(f->program, f->verbose);

    f->color = test_decl_option(CLOPTS_OPTION_FLAG, "color", 'c');
    test_decl_check(clopts_option_set_help(f->color, "Colorize the output"));
    test_decl_check(clopts_option_set_negatable(f->color, true));
    f->color = test_decl_global_attach(f->program, f->color);

    f->config = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');
    test_decl_check(clopts_option_set_value_name(f->config, "FILE"));
    test_decl_check(clopts_option_set_help(f->config, "Configuration file"));
    test_decl_check(clopts_option_set_env(f->config, "TOOL_CONFIG"));
    f->config = test_decl_global_attach(f->program, f->config);
    text_fixture_add_terminals(f->program, &f->help, &f->version);

    /* build */
    f->build = test_decl_command(f->program, "build");
    if (f->build != NULL)
    {
        test_decl_check(clopts_command_set_summary(f->build, "Build an output from a source"));
    }
    if (f->build != NULL)
    {
        test_decl_check(clopts_command_set_description(f->build, "Build an output from SRC."));
    }

    f->output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    test_decl_check(clopts_option_set_value_name(f->output, "FILE"));
    test_decl_check(clopts_option_set_help(f->output, "Write the result to FILE"));
    test_decl_check(clopts_option_set_required(f->output, true));
    f->output = test_decl_attach(f->build, f->output);

    f->keys = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');
    test_decl_check(clopts_option_set_value_name(f->keys, "KEY"));
    test_decl_check(clopts_option_set_help(f->keys, "Keys to include"));
    test_decl_check(clopts_option_set_delimiter(f->keys, ','));
    test_decl_check(clopts_option_set_default(f->keys, "a,b"));
    f->keys = test_decl_attach(f->build, f->keys);

    f->build_src = test_decl_positional("SRC");
    if (f->build_src != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->build_src, "Source file"));
    }
    if (f->build_src != NULL)
    {
        clopts_positional_set_required(f->build_src, true);
    }
    f->build_src = test_decl_attach_positional(f->build, f->build_src);

    f->build_extra = test_decl_positional("EXTRA");
    if (f->build_extra != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->build_extra, "Extra inputs"));
    }
    if (f->build_extra != NULL)
    {
        clopts_positional_set_variadic(f->build_extra, true);
    }
    f->build_extra = test_decl_attach_positional(f->build, f->build_extra);

    /* copy */
    f->copy = test_decl_command(f->program, "copy");
    if (f->copy != NULL)
    {
        test_decl_check(clopts_command_set_summary(f->copy, "Copy a file"));
    }

    f->copy_src = test_decl_positional("SRC");
    if (f->copy_src != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->copy_src, "File to copy"));
    }
    if (f->copy_src != NULL)
    {
        clopts_positional_set_required(f->copy_src, true);
    }
    f->copy_src = test_decl_attach_positional(f->copy, f->copy_src);

    f->copy_dst = test_decl_positional("DST");
    if (f->copy_dst != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->copy_dst, "Destination"));
    }
    if (f->copy_dst != NULL)
    {
        clopts_positional_set_required(f->copy_dst, true);
    }
    f->copy_dst = test_decl_attach_positional(f->copy, f->copy_dst);

    f->help_command = test_decl_help_command(f->program);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
    f->ok = (test_decl_failures == 0);
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
    test_decl_reset();
    f->program = test_decl_begin("filt");
    if (f->program != NULL)
    {
        test_decl_check(clopts_program_set_description(f->program, "Filter the lines of files."));
    }

    f->lines = test_decl_option(CLOPTS_OPTION_VALUE, "lines", 'n');
    test_decl_check(clopts_option_set_value_name(f->lines, "N"));
    test_decl_check(clopts_option_set_help(f->lines, "Number of lines"));
    test_decl_check(clopts_option_set_default(f->lines, "10"));
    test_decl_check(clopts_option_set_env(f->lines, "FILT_LINES"));
    f->lines = test_decl_global_attach(f->program, f->lines);

    f->file = test_decl_positional("FILE");
    if (f->file != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->file, "Files to filter"));
    }
    if (f->file != NULL)
    {
        clopts_positional_set_required(f->file, true);
    }
    if (f->file != NULL)
    {
        clopts_positional_set_variadic(f->file, true);
    }
    f->file = test_decl_global_attach_positional(f->program, f->file);
    text_fixture_add_terminals(f->program, &f->help, NULL);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
    f->ok = (test_decl_failures == 0);
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
    test_decl_reset();
    f->program = test_decl_begin("bare");
    text_fixture_add_terminals(f->program, &f->help, &f->version);

    f->run = test_decl_command(f->program, "run");
    if (f->run != NULL)
    {
        test_decl_check(clopts_command_set_summary(f->run, "Run the task"));
    }
    f->help_command = test_decl_help_command(f->program);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
    f->ok = (test_decl_failures == 0);
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
    test_decl_reset();
    f->program = test_decl_begin("wide");

    f->naive = test_decl_option(CLOPTS_OPTION_FLAG, TEXT_FIXTURE_NAIVE, '\0');
    test_decl_check(clopts_option_set_help(f->naive, "Trust every input"));
    f->naive = test_decl_global_attach(f->program, f->naive);

    f->nihongo = test_decl_option(CLOPTS_OPTION_VALUE, TEXT_FIXTURE_NIHONGO, 'j');
    test_decl_check(clopts_option_set_value_name(f->nihongo, TEXT_FIXTURE_ATAI));
    test_decl_check(clopts_option_set_help(f->nihongo, "Japanese text"));
    f->nihongo = test_decl_global_attach(f->program, f->nihongo);

    f->raw = test_decl_option(CLOPTS_OPTION_VALUE, "raw", '\0');
    test_decl_check(clopts_option_set_value_name(f->raw, TEXT_FIXTURE_RAW_VALUE_NAME));
    test_decl_check(clopts_option_set_help(f->raw, "Raw bytes"));
    f->raw = test_decl_global_attach(f->program, f->raw);

    f->file = test_decl_positional(TEXT_FIXTURE_FILE);
    if (f->file != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->file, "Input files"));
    }
    if (f->file != NULL)
    {
        clopts_positional_set_variadic(f->file, true);
    }
    f->file = test_decl_global_attach_positional(f->program, f->file);
    text_fixture_add_terminals(f->program, &f->help, NULL);
    f->program = text_fixture_finish(f->program, f, sizeof(*f));
    f->ok = (test_decl_failures == 0);
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
