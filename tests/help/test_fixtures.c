/*
 * Definitions of the shared help test fixtures (see test_fixtures.h).
 *
 * The help texts of tool are those of the output example in the design
 * (HelpLayout), so that the layout tests can compare against it.
 */

#include "test_fixtures.h"

#include <assert.h>
#include <string.h>

/* ---- tool -------------------------------------------------------------- */

void help_fixture_tool_init(help_fixture_tool_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("tool");
    f->root = clopts_program_root(f->program);
    clopts_command_set_description(f->root, "A sample tool for testing.");
    clopts_program_set_version(f->program, "1.2.3");

    f->verbose = clopts_command_add_count(f->root, "verbose", 'v');
    clopts_option_set_help(f->verbose, "Print more details");

    f->color = clopts_command_add_flag(f->root, "color", 'c');
    clopts_option_set_help(f->color, "Colorize the output");
    clopts_option_set_negatable(f->color, true);

    f->config = clopts_command_add_value(f->root, "config", '\0');
    clopts_option_set_value_name(f->config, "FILE");
    clopts_option_set_help(f->config, "Configuration file");
    clopts_option_set_env(f->config, "TOOL_CONFIG");

    /* build */
    f->build = clopts_program_add_command(f->program, "build");
    clopts_command_set_summary(f->build, "Build an output from a source");
    clopts_command_set_description(f->build, "Build an output from SRC.");

    f->output = clopts_command_add_value(f->build, "output", 'o');
    clopts_option_set_value_name(f->output, "FILE");
    clopts_option_set_help(f->output, "Write the result to FILE");
    clopts_option_set_required(f->output, true);

    f->keys = clopts_command_add_list(f->build, "keys", 'k');
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
}

void help_fixture_tool_free(help_fixture_tool_t *f)
{
    assert(f != NULL);

    clopts_program_free(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- filt -------------------------------------------------------------- */

void help_fixture_filt_init(help_fixture_filt_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("filt");
    f->root = clopts_program_root(f->program);
    clopts_command_set_description(f->root, "Filter the lines of files.");

    f->lines = clopts_command_add_value(f->root, "lines", 'n');
    clopts_option_set_value_name(f->lines, "N");
    clopts_option_set_help(f->lines, "Number of lines");
    clopts_option_set_default(f->lines, "10");
    clopts_option_set_env(f->lines, "FILT_LINES");

    f->file = clopts_command_add_positional(f->root, "FILE");
    clopts_positional_set_help(f->file, "Files to filter");
    clopts_positional_set_required(f->file, true);
    clopts_positional_set_variadic(f->file, true);
}

void help_fixture_filt_free(help_fixture_filt_t *f)
{
    assert(f != NULL);

    clopts_program_free(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- bare -------------------------------------------------------------- */

void help_fixture_bare_init(help_fixture_bare_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("bare");
    f->root = clopts_program_root(f->program);
    clopts_program_set_version(f->program, "0.1");

    f->run = clopts_program_add_command(f->program, "run");
    clopts_command_set_summary(f->run, "Run the task");
}

void help_fixture_bare_free(help_fixture_bare_t *f)
{
    assert(f != NULL);

    clopts_program_free(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- wide -------------------------------------------------------------- */

void help_fixture_wide_init(help_fixture_wide_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("wide");
    f->root = clopts_program_root(f->program);

    f->naive = clopts_command_add_flag(f->root, HELP_FIXTURE_NAIVE, '\0');
    clopts_option_set_help(f->naive, "Trust every input");

    f->nihongo = clopts_command_add_value(f->root, HELP_FIXTURE_NIHONGO, 'j');
    clopts_option_set_value_name(f->nihongo, HELP_FIXTURE_ATAI);
    clopts_option_set_help(f->nihongo, "Japanese text");

    f->raw = clopts_command_add_value(f->root, "raw", '\0');
    clopts_option_set_value_name(f->raw, HELP_FIXTURE_RAW_VALUE_NAME);
    clopts_option_set_help(f->raw, "Raw bytes");

    f->file = clopts_command_add_positional(f->root, HELP_FIXTURE_FILE);
    clopts_positional_set_help(f->file, "Input files");
    clopts_positional_set_variadic(f->file, true);
}

void help_fixture_wide_free(help_fixture_wide_t *f)
{
    assert(f != NULL);

    clopts_program_free(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- write callbacks --------------------------------------------------- */

void help_recorder_init(help_recorder_t *rec)
{
    assert(rec != NULL);

    memset(rec, 0, sizeof(*rec));
}

int help_recorder_write(void *ctx, const char *data, size_t len)
{
    help_recorder_t *rec = (help_recorder_t *)ctx;

    assert(rec != NULL);
    rec->calls++;

    if (len > (HELP_RECORDER_SIZE - rec->len))
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

void help_failer_init(help_failer_t *failer, size_t fail_at)
{
    assert(failer != NULL);

    memset(failer, 0, sizeof(*failer));
    failer->fail_at = fail_at;
    help_recorder_init(&failer->written);
}

int help_failer_write(void *ctx, const char *data, size_t len)
{
    help_failer_t *failer = (help_failer_t *)ctx;

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
    return help_recorder_write(&failer->written, data, len);
}
