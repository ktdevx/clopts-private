/*
 * Resolver: environment variable fallback, default values, required checks
 * and the default environment lookup.
 *
 * Skeleton: every function returns a fixed value until the resolver is
 * implemented.
 */

#include "parser/parser_internal.h"

clopts_env_status_t clopts_parser_getenv_default(void *ctx, const char *name, char **out)
{
    (void)ctx;
    (void)name;
    (void)out;
    return CLOPTS_ENV_UNSET;
}

int clopts_parser_resolve(clopts_result_t *result, clopts_getenv_fn getenv_fn, void *env_ctx,
                          bool include_command)
{
    (void)result;
    (void)getenv_fn;
    (void)env_ctx;
    (void)include_command;
    return 0;
}

int clopts_parser_check_required(clopts_result_t *result, bool include_command)
{
    (void)result;
    (void)include_command;
    return 0;
}
