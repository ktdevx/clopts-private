/*
 * Definitions of the shared help test fixtures (see test_fixtures.h).
 *
 * The help texts of tool are those of the output example in the design
 * (HelpLayout), so that the layout tests can compare against it.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include <assert.h>
#include <string.h>

/* ---- tool -------------------------------------------------------------- */

void help_fixture_tool_init(help_fixture_tool_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    test_decl_reset();
    f->program = test_decl_program("tool");
    f->root = clopts_program_root(f->program);
    test_decl_check(clopts_command_set_description(f->root, "A sample tool for testing."));
    test_decl_check(clopts_program_set_version(f->program, "1.2.3"));

    f->verbose = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    test_decl_check(clopts_option_set_help(f->verbose, "Print more details"));
    f->verbose = test_decl_attach(f->root, f->verbose);

    f->color = test_decl_option(CLOPTS_OPTION_FLAG, "color", 'c');
    test_decl_check(clopts_option_set_help(f->color, "Colorize the output"));
    test_decl_check(clopts_option_set_negatable(f->color, true));
    f->color = test_decl_attach(f->root, f->color);

    f->config = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');
    test_decl_check(clopts_option_set_value_name(f->config, "FILE"));
    test_decl_check(clopts_option_set_help(f->config, "Configuration file"));
    test_decl_check(clopts_option_set_env(f->config, "TOOL_CONFIG"));
    f->config = test_decl_attach(f->root, f->config);

    /* build */
    f->build = test_decl_command(f->program, "build");
    test_decl_check(clopts_command_set_summary(f->build, "Build an output from a source"));
    test_decl_check(clopts_command_set_description(f->build, "Build an output from SRC."));

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
    test_decl_check(clopts_positional_set_help(f->build_src, "Source file"));
    test_decl_check(clopts_positional_set_required(f->build_src, true));
    f->build_src = test_decl_attach_positional(f->build, f->build_src);

    f->build_extra = test_decl_positional("EXTRA");
    test_decl_check(clopts_positional_set_help(f->build_extra, "Extra inputs"));
    test_decl_check(clopts_positional_set_variadic(f->build_extra, true));
    f->build_extra = test_decl_attach_positional(f->build, f->build_extra);

    /* copy */
    f->copy = test_decl_command(f->program, "copy");
    test_decl_check(clopts_command_set_summary(f->copy, "Copy a file"));

    f->copy_src = test_decl_positional("SRC");
    test_decl_check(clopts_positional_set_help(f->copy_src, "File to copy"));
    test_decl_check(clopts_positional_set_required(f->copy_src, true));
    f->copy_src = test_decl_attach_positional(f->copy, f->copy_src);

    f->copy_dst = test_decl_positional("DST");
    test_decl_check(clopts_positional_set_help(f->copy_dst, "Destination"));
    test_decl_check(clopts_positional_set_required(f->copy_dst, true));
    f->copy_dst = test_decl_attach_positional(f->copy, f->copy_dst);
    f->ok = (test_decl_failures == 0);
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
    test_decl_reset();
    f->program = test_decl_program("filt");
    f->root = clopts_program_root(f->program);
    test_decl_check(clopts_command_set_description(f->root, "Filter the lines of files."));

    f->lines = test_decl_option(CLOPTS_OPTION_VALUE, "lines", 'n');
    test_decl_check(clopts_option_set_value_name(f->lines, "N"));
    test_decl_check(clopts_option_set_help(f->lines, "Number of lines"));
    test_decl_check(clopts_option_set_default(f->lines, "10"));
    test_decl_check(clopts_option_set_env(f->lines, "FILT_LINES"));
    f->lines = test_decl_attach(f->root, f->lines);

    f->file = test_decl_positional("FILE");
    test_decl_check(clopts_positional_set_help(f->file, "Files to filter"));
    test_decl_check(clopts_positional_set_required(f->file, true));
    test_decl_check(clopts_positional_set_variadic(f->file, true));
    f->file = test_decl_attach_positional(f->root, f->file);
    f->ok = (test_decl_failures == 0);
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
    test_decl_reset();
    f->program = test_decl_program("bare");
    f->root = clopts_program_root(f->program);
    test_decl_check(clopts_program_set_version(f->program, "0.1"));

    f->run = test_decl_command(f->program, "run");
    test_decl_check(clopts_command_set_summary(f->run, "Run the task"));
    f->ok = (test_decl_failures == 0);
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
    test_decl_reset();
    f->program = test_decl_program("wide");
    f->root = clopts_program_root(f->program);

    f->naive = test_decl_option(CLOPTS_OPTION_FLAG, HELP_FIXTURE_NAIVE, '\0');
    test_decl_check(clopts_option_set_help(f->naive, "Trust every input"));
    f->naive = test_decl_attach(f->root, f->naive);

    f->nihongo = test_decl_option(CLOPTS_OPTION_VALUE, HELP_FIXTURE_NIHONGO, 'j');
    test_decl_check(clopts_option_set_value_name(f->nihongo, HELP_FIXTURE_ATAI));
    test_decl_check(clopts_option_set_help(f->nihongo, "Japanese text"));
    f->nihongo = test_decl_attach(f->root, f->nihongo);

    f->raw = test_decl_option(CLOPTS_OPTION_VALUE, "raw", '\0');
    test_decl_check(clopts_option_set_value_name(f->raw, HELP_FIXTURE_RAW_VALUE_NAME));
    test_decl_check(clopts_option_set_help(f->raw, "Raw bytes"));
    f->raw = test_decl_attach(f->root, f->raw);

    f->file = test_decl_positional(HELP_FIXTURE_FILE);
    test_decl_check(clopts_positional_set_help(f->file, "Input files"));
    test_decl_check(clopts_positional_set_variadic(f->file, true));
    f->file = test_decl_attach_positional(f->root, f->file);
    f->ok = (test_decl_failures == 0);
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
