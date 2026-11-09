/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Definitions of the shared parser test fixtures (see test_fixtures.h).
 *
 * The three programs reproduce the first-version declarations (names, kinds,
 * help texts, value names, environment variables, defaults, delimiters, count
 * limit and positionals) with the builder functions, bottom up: the root and
 * the commands first, then the program (test_decl_finish()). The terminal
 * options -h/--help and --version and the help command come from the
 * helpers of clopts/help.h.
 */

#include "test_fixtures.h"
#include "test_decl.h"

#include "clopts/help.h"

#include <assert.h>
#include <string.h>

/* ---- tool -------------------------------------------------------------- */

void fixture_tool_init(fixture_tool_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    test_decl_reset();
    f->root = test_decl_root("tool");
    if (f->root != NULL)
    {
        test_decl_check(clopts_command_set_description(f->root, "A fictional tool with subcommands."));
    }

    f->verbose = test_decl_option(CLOPTS_OPTION_COUNT, "verbose", 'v');
    if (f->verbose != NULL)
    {
        test_decl_check(clopts_option_set_help(f->verbose, "Increase verbosity."));
    }
    f->verbose = test_decl_attach(f->root, f->verbose);

    f->color = test_decl_option(CLOPTS_OPTION_FLAG, "color", '\0');
    if (f->color != NULL)
    {
        test_decl_check(clopts_option_set_help(f->color, "Colorize the output."));
    }
    if (f->color != NULL)
    {
        test_decl_check(clopts_option_set_negatable(f->color, true));
    }
    f->color = test_decl_attach(f->root, f->color);

    f->config = test_decl_option(CLOPTS_OPTION_VALUE, "config", '\0');
    if (f->config != NULL)
    {
        test_decl_check(clopts_option_set_value_name(f->config, "PATH"));
    }
    if (f->config != NULL)
    {
        test_decl_check(clopts_option_set_help(f->config, "Configuration file."));
    }
    if (f->config != NULL)
    {
        test_decl_check(clopts_option_set_env(f->config, "TOOL_CONFIG"));
    }
    f->config = test_decl_attach(f->root, f->config);

    if (f->root != NULL)
    {
        test_decl_check(clopts_help_add_help_option(f->root, &f->help));
        test_decl_check(clopts_help_add_version_option(f->root, &f->version));
    }

    /* build */
    f->build = test_decl_command(f->root, "build");
    if (f->build != NULL)
    {
        test_decl_check(clopts_command_set_summary(f->build, "Build a source."));
    }

    f->output = test_decl_option(CLOPTS_OPTION_VALUE, "output", 'o');
    if (f->output != NULL)
    {
        test_decl_check(clopts_option_set_value_name(f->output, "FILE"));
    }
    if (f->output != NULL)
    {
        test_decl_check(clopts_option_set_help(f->output, "Output file."));
    }
    if (f->output != NULL)
    {
        test_decl_check(clopts_option_set_required(f->output, true));
    }
    f->output = test_decl_attach(f->build, f->output);

    f->keys = test_decl_option(CLOPTS_OPTION_LIST, "keys", 'k');
    if (f->keys != NULL)
    {
        test_decl_check(clopts_option_set_value_name(f->keys, "KEY"));
    }
    if (f->keys != NULL)
    {
        test_decl_check(clopts_option_set_help(f->keys, "Keys to build."));
    }
    if (f->keys != NULL)
    {
        test_decl_check(clopts_option_set_delimiter(f->keys, ','));
    }
    f->keys = test_decl_attach(f->build, f->keys);

    f->jobs = test_decl_option(CLOPTS_OPTION_COUNT, NULL, 'j');
    if (f->jobs != NULL)
    {
        test_decl_check(clopts_option_set_help(f->jobs, "Add a parallel job."));
    }
    if (f->jobs != NULL)
    {
        test_decl_check(clopts_option_set_count_max(f->jobs, 8));
    }
    f->jobs = test_decl_attach(f->build, f->jobs);

    f->build_src = test_decl_positional("SRC");
    if (f->build_src != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->build_src, "Source to build."));
    }
    if (f->build_src != NULL)
    {
        clopts_positional_set_required(f->build_src, true);
    }
    f->build_src = test_decl_attach_positional(f->build, f->build_src);

    f->build_extra = test_decl_positional("EXTRA");
    if (f->build_extra != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->build_extra, "Extra inputs."));
    }
    if (f->build_extra != NULL)
    {
        clopts_positional_set_variadic(f->build_extra, true);
    }
    f->build_extra = test_decl_attach_positional(f->build, f->build_extra);

    /* copy */
    f->copy = test_decl_command(f->root, "copy");
    if (f->copy != NULL)
    {
        test_decl_check(clopts_command_set_summary(f->copy, "Copy a file."));
    }

    f->copy_src = test_decl_positional("SRC");
    if (f->copy_src != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->copy_src, "Source."));
    }
    if (f->copy_src != NULL)
    {
        clopts_positional_set_required(f->copy_src, true);
    }
    f->copy_src = test_decl_attach_positional(f->copy, f->copy_src);

    f->copy_dst = test_decl_positional("DST");
    if (f->copy_dst != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->copy_dst, "Destination."));
    }
    if (f->copy_dst != NULL)
    {
        clopts_positional_set_required(f->copy_dst, true);
    }
    f->copy_dst = test_decl_attach_positional(f->copy, f->copy_dst);

    /* cat */
    f->cat = test_decl_command(f->root, "cat");
    if (f->cat != NULL)
    {
        test_decl_check(clopts_command_set_summary(f->cat, "Show files."));
    }

    f->tags = test_decl_option(CLOPTS_OPTION_LIST, "tags", '\0');
    if (f->tags != NULL)
    {
        test_decl_check(clopts_option_set_value_name(f->tags, "TAG"));
    }
    if (f->tags != NULL)
    {
        test_decl_check(clopts_option_set_help(f->tags, "Tags to show."));
    }
    if (f->tags != NULL)
    {
        test_decl_check(clopts_option_set_default(f->tags, "a,b"));
    }
    if (f->tags != NULL)
    {
        test_decl_check(clopts_option_set_env(f->tags, "TOOL_TAGS"));
    }
    if (f->tags != NULL)
    {
        test_decl_check(clopts_option_set_delimiter(f->tags, ','));
    }
    f->tags = test_decl_attach(f->cat, f->tags);

    f->cat_file = test_decl_positional("FILE");
    if (f->cat_file != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->cat_file, "Files to show."));
    }
    if (f->cat_file != NULL)
    {
        clopts_positional_set_variadic(f->cat_file, true);
    }
    f->cat_file = test_decl_attach_positional(f->cat, f->cat_file);

    /* help */
    if (f->root != NULL)
    {
        test_decl_check(clopts_help_command_create(&f->help_command));
        f->help_command = test_decl_adopt(f->root, f->help_command);
    }

    f->program = test_decl_finish(f->root);
    if (f->program == NULL)
    {
        /* Everything was released with the root. */
        memset(f, 0, sizeof(*f));
    }
    f->ok = (test_decl_failures == 0);
}

void fixture_tool_free(fixture_tool_t *f)
{
    assert(f != NULL);

    clopts_program_destroy(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- filt -------------------------------------------------------------- */

void fixture_filt_init(fixture_filt_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    test_decl_reset();
    f->root = test_decl_root("filt");
    if (f->root != NULL)
    {
        test_decl_check(clopts_command_set_description(f->root, "A fictional filter without subcommands."));
    }

    f->lines = test_decl_option(CLOPTS_OPTION_VALUE, "lines", 'n');
    if (f->lines != NULL)
    {
        test_decl_check(clopts_option_set_value_name(f->lines, "N"));
    }
    if (f->lines != NULL)
    {
        test_decl_check(clopts_option_set_help(f->lines, "Number of lines."));
    }
    if (f->lines != NULL)
    {
        test_decl_check(clopts_option_set_default(f->lines, "10"));
    }
    if (f->lines != NULL)
    {
        test_decl_check(clopts_option_set_env(f->lines, "FILT_LINES"));
    }
    f->lines = test_decl_attach(f->root, f->lines);

    f->file = test_decl_positional("FILE");
    if (f->file != NULL)
    {
        test_decl_check(clopts_positional_set_help(f->file, "Files to filter."));
    }
    if (f->file != NULL)
    {
        clopts_positional_set_variadic(f->file, true);
    }
    f->file = test_decl_attach_positional(f->root, f->file);

    if (f->root != NULL)
    {
        test_decl_check(clopts_help_add_help_option(f->root, &f->help));
    }
    f->program = test_decl_finish(f->root);
    if (f->program == NULL)
    {
        memset(f, 0, sizeof(*f));
    }
    f->ok = (test_decl_failures == 0);
}

void fixture_filt_free(fixture_filt_t *f)
{
    assert(f != NULL);

    clopts_program_destroy(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- strict ------------------------------------------------------------ */

void fixture_strict_init(fixture_strict_t *f)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    test_decl_reset();
    f->root = test_decl_root("strict");
    if (f->root != NULL)
    {
        test_decl_check(clopts_command_set_description(f->root, "A fictional program with a required global option."));
    }

    f->token = test_decl_option(CLOPTS_OPTION_VALUE, "token", '\0');
    if (f->token != NULL)
    {
        test_decl_check(clopts_option_set_value_name(f->token, "TOKEN"));
    }
    if (f->token != NULL)
    {
        test_decl_check(clopts_option_set_help(f->token, "Access token."));
    }
    if (f->token != NULL)
    {
        test_decl_check(clopts_option_set_required(f->token, true));
    }
    f->token = test_decl_attach(f->root, f->token);

    if (f->root != NULL)
    {
        test_decl_check(clopts_help_add_help_option(f->root, &f->help));
    }

    f->run = test_decl_command(f->root, "run");
    if (f->run != NULL)
    {
        test_decl_check(clopts_command_set_summary(f->run, "Run it."));
    }
    f->program = test_decl_finish(f->root);
    if (f->program == NULL)
    {
        memset(f, 0, sizeof(*f));
    }
    f->ok = (test_decl_failures == 0);
}

void fixture_strict_free(fixture_strict_t *f)
{
    assert(f != NULL);

    clopts_program_destroy(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- mock environment -------------------------------------------------- */

void fixture_env_init(fixture_env_t *env, const fixture_env_var_t *vars, size_t var_count)
{
    size_t i;

    assert(env != NULL);
    assert(var_count <= FIXTURE_ENV_MAX_VARS);
    assert((vars != NULL) || (var_count == 0));

    memset(env, 0, sizeof(*env));
    for (i = 0; i < var_count; i++)
    {
        assert((vars[i].name != NULL) && (vars[i].value != NULL));
        assert(strlen(vars[i].value) < FIXTURE_ENV_VALUE_SIZE);
    }
    env->vars = vars;
    env->var_count = var_count;
    env->fail_next = false;
    env->clobber = false;
    env->query_count = 0;
}

void fixture_env_clobber(fixture_env_t *env)
{
    size_t i;

    assert(env != NULL);

    for (i = 0; i < FIXTURE_ENV_MAX_VARS; i++)
    {
        memset(env->buffers[i], '#', FIXTURE_ENV_VALUE_SIZE - 1);
        env->buffers[i][FIXTURE_ENV_VALUE_SIZE - 1] = '\0';
    }
}

clopts_env_status_t fixture_env_getenv(void *ctx, const char *name, const char **value)
{
    fixture_env_t *env = (fixture_env_t *)ctx;
    size_t i;

    assert((env != NULL) && (name != NULL) && (value != NULL));

    if (env->clobber)
    {
        fixture_env_clobber(env);
    }

    if (env->query_count < FIXTURE_ENV_MAX_QUERIES)
    {
        char *slot = env->queried[env->query_count];

        strncpy(slot, name, FIXTURE_ENV_NAME_SIZE - 1);
        slot[FIXTURE_ENV_NAME_SIZE - 1] = '\0';
    }
    env->query_count++;

    if (env->fail_next)
    {
        env->fail_next = false;
        return CLOPTS_ENV_FAILED;
    }

    for (i = 0; i < env->var_count; i++)
    {
        if (strcmp(env->vars[i].name, name) == 0)
        {
            memcpy(env->buffers[i], env->vars[i].value, strlen(env->vars[i].value) + 1);
            *value = env->buffers[i];
            return CLOPTS_ENV_FOUND;
        }
    }
    return CLOPTS_ENV_UNSET;
}

size_t fixture_env_query_count(const fixture_env_t *env, const char *name)
{
    size_t stored = env->query_count;
    size_t n = 0;
    size_t i;

    if (stored > FIXTURE_ENV_MAX_QUERIES)
    {
        stored = FIXTURE_ENV_MAX_QUERIES;
    }
    for (i = 0; i < stored; i++)
    {
        if (strncmp(env->queried[i], name, FIXTURE_ENV_NAME_SIZE - 1) == 0)
        {
            n++;
        }
    }
    return n;
}

bool fixture_env_was_queried(const fixture_env_t *env, const char *name)
{
    return (fixture_env_query_count(env, name) > 0);
}
