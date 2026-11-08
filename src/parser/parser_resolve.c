/*
 * Resolver: the default environment lookup, resolution from the environment
 * and the defaults, and the required checks.
 *
 * Skeleton: every function is a placeholder that returns a fixed value
 * (nothing set, nothing missing). Task 6.3 implements them.
 */

#include "parser/parser_internal.h"

clopts_env_status_t clopts_parser_getenv_default(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_UNSET;
}

int clopts_parser_resolve(clopts_result_t *result, const clopts_program_t *program,
                          clopts_getenv_fn getenv_fn, void *env_ctx, bool include_command)
{
    (void)result;
    (void)program;
    (void)getenv_fn;
    (void)env_ctx;
    (void)include_command;
    return 0;
}

int clopts_parser_check_required(clopts_result_t *result, const clopts_program_t *program, bool include_command)
{
    (void)result;
    (void)program;
    (void)include_command;
    return 0;
}
