/*
 * Definitions of the shared parser test fixtures (see test_fixtures.h).
 *
 * The three programs reproduce the first-version declarations (names, kinds,
 * help texts, value names, environment variables, defaults, delimiters, count
 * limit, positionals, handlers and version) with the builder functions.
 */

#include "test_fixtures.h"

#include <assert.h>
#include <string.h>

/* ---- tool -------------------------------------------------------------- */

fixture_handler_record_t fixture_build_handler_record;

void fixture_tool_init(fixture_tool_t *f, clopts_parser_probe_t *probe)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("tool");
    if ((f->program != NULL) && (probe != NULL))
    {
        clopts_parser_set_probe(f->program, probe);
    }
    f->root = clopts_program_root(f->program);
    clopts_command_set_description(f->root, "A fictional tool with subcommands.");
    clopts_program_set_version(f->program, "1.2.3");

    f->verbose = clopts_command_add_count(f->root, "verbose", 'v');
    clopts_option_set_help(f->verbose, "Increase verbosity.");

    f->color = clopts_command_add_flag(f->root, "color", '\0');
    clopts_option_set_help(f->color, "Colorize the output.");
    clopts_option_set_negatable(f->color, true);

    f->config = clopts_command_add_value(f->root, "config", '\0');
    clopts_option_set_value_name(f->config, "PATH");
    clopts_option_set_help(f->config, "Configuration file.");
    clopts_option_set_env(f->config, "TOOL_CONFIG");

    /* build */
    f->build = clopts_program_add_command(f->program, "build");
    clopts_command_set_summary(f->build, "Build a source.");
    clopts_command_set_handler(f->build, fixture_handler, &fixture_build_handler_record);

    f->output = clopts_command_add_value(f->build, "output", 'o');
    clopts_option_set_value_name(f->output, "FILE");
    clopts_option_set_help(f->output, "Output file.");
    clopts_option_set_required(f->output, true);

    f->keys = clopts_command_add_list(f->build, "keys", 'k');
    clopts_option_set_value_name(f->keys, "KEY");
    clopts_option_set_help(f->keys, "Keys to build.");
    clopts_option_set_delimiter(f->keys, ',');

    f->jobs = clopts_command_add_count(f->build, NULL, 'j');
    clopts_option_set_help(f->jobs, "Add a parallel job.");
    clopts_option_set_count_max(f->jobs, 8);

    f->build_src = clopts_command_add_positional(f->build, "SRC");
    clopts_positional_set_help(f->build_src, "Source to build.");
    clopts_positional_set_required(f->build_src, true);

    f->build_extra = clopts_command_add_positional(f->build, "EXTRA");
    clopts_positional_set_help(f->build_extra, "Extra inputs.");
    clopts_positional_set_variadic(f->build_extra, true);

    /* copy */
    f->copy = clopts_program_add_command(f->program, "copy");
    clopts_command_set_summary(f->copy, "Copy a file.");

    f->copy_src = clopts_command_add_positional(f->copy, "SRC");
    clopts_positional_set_help(f->copy_src, "Source.");
    clopts_positional_set_required(f->copy_src, true);

    f->copy_dst = clopts_command_add_positional(f->copy, "DST");
    clopts_positional_set_help(f->copy_dst, "Destination.");
    clopts_positional_set_required(f->copy_dst, true);

    /* cat */
    f->cat = clopts_program_add_command(f->program, "cat");
    clopts_command_set_summary(f->cat, "Show files.");

    f->tags = clopts_command_add_list(f->cat, "tags", '\0');
    clopts_option_set_value_name(f->tags, "TAG");
    clopts_option_set_help(f->tags, "Tags to show.");
    clopts_option_set_default(f->tags, "a,b");
    clopts_option_set_env(f->tags, "TOOL_TAGS");
    clopts_option_set_delimiter(f->tags, ',');

    f->cat_file = clopts_command_add_positional(f->cat, "FILE");
    clopts_positional_set_help(f->cat_file, "Files to show.");
    clopts_positional_set_variadic(f->cat_file, true);
}

void fixture_tool_free(fixture_tool_t *f)
{
    assert(f != NULL);

    clopts_program_free(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- filt -------------------------------------------------------------- */

fixture_handler_record_t fixture_filt_handler_record;

void fixture_filt_init(fixture_filt_t *f, clopts_parser_probe_t *probe)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("filt");
    if ((f->program != NULL) && (probe != NULL))
    {
        clopts_parser_set_probe(f->program, probe);
    }
    f->root = clopts_program_root(f->program);
    clopts_command_set_description(f->root, "A fictional filter without subcommands.");
    clopts_command_set_handler(f->root, fixture_handler, &fixture_filt_handler_record);

    f->lines = clopts_command_add_value(f->root, "lines", 'n');
    clopts_option_set_value_name(f->lines, "N");
    clopts_option_set_help(f->lines, "Number of lines.");
    clopts_option_set_default(f->lines, "10");
    clopts_option_set_env(f->lines, "FILT_LINES");

    f->file = clopts_command_add_positional(f->root, "FILE");
    clopts_positional_set_help(f->file, "Files to filter.");
    clopts_positional_set_variadic(f->file, true);
}

void fixture_filt_free(fixture_filt_t *f)
{
    assert(f != NULL);

    clopts_program_free(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- strict ------------------------------------------------------------ */

void fixture_strict_init(fixture_strict_t *f, clopts_parser_probe_t *probe)
{
    assert(f != NULL);

    memset(f, 0, sizeof(*f));
    f->program = clopts_program_new("strict");
    if ((f->program != NULL) && (probe != NULL))
    {
        clopts_parser_set_probe(f->program, probe);
    }
    f->root = clopts_program_root(f->program);
    clopts_command_set_description(f->root, "A fictional program with a required global option.");

    f->token = clopts_command_add_value(f->root, "token", '\0');
    clopts_option_set_value_name(f->token, "TOKEN");
    clopts_option_set_help(f->token, "Access token.");
    clopts_option_set_required(f->token, true);

    f->run = clopts_program_add_command(f->program, "run");
    clopts_command_set_summary(f->run, "Run it.");
}

void fixture_strict_free(fixture_strict_t *f)
{
    assert(f != NULL);

    clopts_program_free(f->program);
    memset(f, 0, sizeof(*f));
}

/* ---- handler ----------------------------------------------------------- */

int fixture_handler(const clopts_result_t *result, void *userdata)
{
    fixture_handler_record_t *record = (fixture_handler_record_t *)userdata;

    record->calls++;
    record->last_result = result;
    record->last_userdata = userdata;
    return record->return_value;
}

void fixture_handler_reset(fixture_handler_record_t *record, int return_value)
{
    record->calls = 0;
    record->last_result = NULL;
    record->last_userdata = NULL;
    record->return_value = return_value;
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
