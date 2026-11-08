/*
 * Scanner: clopts_parse / clopts_parse_with_env, the scan before the
 * subcommand, the main scan, the recovery scan and the final status.
 *
 * Skeleton: both functions return a fixed value until the scanner is
 * implemented.
 */

#include "parser/parser_internal.h"

clopts_status_t clopts_parse(const clopts_program_t *program, int argc, char *const *argv,
                             clopts_result_t *result)
{
    (void)program;
    (void)argc;
    (void)argv;
    (void)result;
    return CLOPTS_OUT_OF_MEMORY;
}

clopts_status_t clopts_parse_with_env(const clopts_program_t *program, int argc, char *const *argv,
                                      clopts_getenv_fn getenv_fn, void *env_ctx,
                                      clopts_result_t *result)
{
    (void)program;
    (void)argc;
    (void)argv;
    (void)getenv_fn;
    (void)env_ctx;
    (void)result;
    return CLOPTS_OUT_OF_MEMORY;
}
