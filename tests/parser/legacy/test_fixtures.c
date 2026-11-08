/*
 * Definitions of the shared parser test fixtures (see test_fixtures.h).
 */

#include "test_fixtures.h"

#include <stdlib.h>
#include <string.h>

/* ---- tool -------------------------------------------------------------- */

const clopts_option_t fixture_tool_options[FIXTURE_TOOL_OPT_COUNT] = {
    [FIXTURE_TOOL_OPT_VERBOSE] = {
        .long_name = "verbose",
        .short_name = 'v',
        .kind = CLOPTS_COUNT,
        .help = "Increase verbosity.",
    },
    [FIXTURE_TOOL_OPT_COLOR] = {
        .long_name = "color",
        .kind = CLOPTS_FLAG,
        .help = "Colorize the output.",
        .negatable = true,
    },
    [FIXTURE_TOOL_OPT_CONFIG] = {
        .long_name = "config",
        .kind = CLOPTS_VALUE,
        .value_name = "PATH",
        .help = "Configuration file.",
        .env = "TOOL_CONFIG",
    },
};

const clopts_option_t fixture_build_options[FIXTURE_BUILD_OPT_COUNT] = {
    [FIXTURE_BUILD_OPT_OUTPUT] = {
        .long_name = "output",
        .short_name = 'o',
        .kind = CLOPTS_VALUE,
        .value_name = "FILE",
        .help = "Output file.",
        .required = true,
    },
    [FIXTURE_BUILD_OPT_KEYS] = {
        .long_name = "keys",
        .short_name = 'k',
        .kind = CLOPTS_LIST,
        .value_name = "KEY",
        .help = "Keys to build.",
        .list_delimiter = ',',
    },
    [FIXTURE_BUILD_OPT_JOBS] = {
        .short_name = 'j',
        .kind = CLOPTS_COUNT,
        .help = "Add a parallel job.",
        .count_max = 8,
    },
};

const clopts_positional_t fixture_build_positionals[FIXTURE_BUILD_POS_COUNT] = {
    [FIXTURE_BUILD_POS_SRC] = {
        .name = "SRC",
        .help = "Source to build.",
        .required = true,
    },
    [FIXTURE_BUILD_POS_EXTRA] = {
        .name = "EXTRA",
        .help = "Extra inputs.",
        .variadic = true,
    },
};

const clopts_positional_t fixture_copy_positionals[FIXTURE_COPY_POS_COUNT] = {
    [FIXTURE_COPY_POS_SRC] = {
        .name = "SRC",
        .help = "Source.",
        .required = true,
    },
    [FIXTURE_COPY_POS_DST] = {
        .name = "DST",
        .help = "Destination.",
        .required = true,
    },
};

const clopts_option_t fixture_cat_options[FIXTURE_CAT_OPT_COUNT] = {
    [FIXTURE_CAT_OPT_TAGS] = {
        .long_name = "tags",
        .kind = CLOPTS_LIST,
        .value_name = "TAG",
        .help = "Tags to show.",
        .default_value = "a,b",
        .env = "TOOL_TAGS",
        .list_delimiter = ',',
    },
};

const clopts_positional_t fixture_cat_positionals[FIXTURE_CAT_POS_COUNT] = {
    [FIXTURE_CAT_POS_FILE] = {
        .name = "FILE",
        .help = "Files to show.",
        .variadic = true,
    },
};

fixture_handler_record_t fixture_build_handler_record;

const clopts_command_t fixture_tool_commands[FIXTURE_TOOL_CMD_COUNT] = {
    [FIXTURE_TOOL_CMD_BUILD] = {
        .name = "build",
        .summary = "Build a source.",
        .options = fixture_build_options,
        .option_count = FIXTURE_BUILD_OPT_COUNT,
        .positionals = fixture_build_positionals,
        .positional_count = FIXTURE_BUILD_POS_COUNT,
        .handler = fixture_handler,
        .userdata = &fixture_build_handler_record,
    },
    [FIXTURE_TOOL_CMD_COPY] = {
        .name = "copy",
        .summary = "Copy a file.",
        .positionals = fixture_copy_positionals,
        .positional_count = FIXTURE_COPY_POS_COUNT,
    },
    [FIXTURE_TOOL_CMD_CAT] = {
        .name = "cat",
        .summary = "Show files.",
        .options = fixture_cat_options,
        .option_count = FIXTURE_CAT_OPT_COUNT,
        .positionals = fixture_cat_positionals,
        .positional_count = FIXTURE_CAT_POS_COUNT,
    },
};

const clopts_program_t fixture_tool = {
    .name = "tool",
    .description = "A fictional tool with subcommands.",
    .version = "1.2.3",
    .options = fixture_tool_options,
    .option_count = FIXTURE_TOOL_OPT_COUNT,
    .commands = fixture_tool_commands,
    .command_count = FIXTURE_TOOL_CMD_COUNT,
};

/* ---- filt -------------------------------------------------------------- */

const clopts_option_t fixture_filt_options[FIXTURE_FILT_OPT_COUNT] = {
    [FIXTURE_FILT_OPT_LINES] = {
        .long_name = "lines",
        .short_name = 'n',
        .kind = CLOPTS_VALUE,
        .value_name = "N",
        .help = "Number of lines.",
        .default_value = "10",
        .env = "FILT_LINES",
    },
};

const clopts_positional_t fixture_filt_positionals[FIXTURE_FILT_POS_COUNT] = {
    [FIXTURE_FILT_POS_FILE] = {
        .name = "FILE",
        .help = "Files to filter.",
        .variadic = true,
    },
};

fixture_handler_record_t fixture_filt_handler_record;

const clopts_program_t fixture_filt = {
    .name = "filt",
    .description = "A fictional filter without subcommands.",
    .options = fixture_filt_options,
    .option_count = FIXTURE_FILT_OPT_COUNT,
    .positionals = fixture_filt_positionals,
    .positional_count = FIXTURE_FILT_POS_COUNT,
    .handler = fixture_handler,
    .userdata = &fixture_filt_handler_record,
};

/* ---- strict ------------------------------------------------------------ */

const clopts_option_t fixture_strict_options[FIXTURE_STRICT_OPT_COUNT] = {
    [FIXTURE_STRICT_OPT_TOKEN] = {
        .long_name = "token",
        .kind = CLOPTS_VALUE,
        .value_name = "TOKEN",
        .help = "Access token.",
        .required = true,
    },
};

const clopts_command_t fixture_strict_commands[FIXTURE_STRICT_CMD_COUNT] = {
    [FIXTURE_STRICT_CMD_RUN] = {
        .name = "run",
        .summary = "Run it.",
    },
};

const clopts_program_t fixture_strict = {
    .name = "strict",
    .description = "A fictional program with a required global option.",
    .options = fixture_strict_options,
    .option_count = FIXTURE_STRICT_OPT_COUNT,
    .commands = fixture_strict_commands,
    .command_count = FIXTURE_STRICT_CMD_COUNT,
};

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

    env->vars = vars;
    env->var_count = var_count;
    env->fail_next = false;
    for (i = 0; i < FIXTURE_ENV_MAX_QUERIES; i++)
    {
        env->queried[i] = NULL;
    }
    env->query_count = 0;
}

clopts_env_status_t fixture_env_getenv(void *ctx, const char *name, char **out)
{
    fixture_env_t *env = (fixture_env_t *)ctx;
    size_t i;

    if (env->query_count < FIXTURE_ENV_MAX_QUERIES)
    {
        env->queried[env->query_count] = name;
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
            size_t size = strlen(env->vars[i].value) + 1;
            char *copy = (char *)malloc(size);

            if (copy == NULL)
            {
                return CLOPTS_ENV_FAILED;
            }
            memcpy(copy, env->vars[i].value, size);
            *out = copy;
            return CLOPTS_ENV_FOUND;
        }
    }
    return CLOPTS_ENV_UNSET;
}

bool fixture_env_was_queried(const fixture_env_t *env, const char *name)
{
    size_t stored = env->query_count;
    size_t i;

    if (stored > FIXTURE_ENV_MAX_QUERIES)
    {
        stored = FIXTURE_ENV_MAX_QUERIES;
    }
    for (i = 0; i < stored; i++)
    {
        if (strcmp(env->queried[i], name) == 0)
        {
            return true;
        }
    }
    return false;
}
