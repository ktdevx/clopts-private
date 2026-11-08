/*
 * Entry points of parsing and validation (clopts_parse,
 * clopts_parse_with_env, clopts_program_validate) and the scanner.
 *
 * Skeleton: every entry point returns the out-of-memory sentinel. Task 5.3
 * implements the entry points and 7.2 the scanner.
 */

#include "parser/parser_internal.h"

clopts_result_t *clopts_parse(clopts_program_t *program, int argc, char *const *argv)
{
    (void)program;
    (void)argc;
    (void)argv;
    return clopts_parser_result_oom();
}

clopts_result_t *clopts_parse_with_env(clopts_program_t *program, int argc, char *const *argv,
                                       clopts_getenv_fn getenv_fn, void *env_ctx)
{
    (void)program;
    (void)argc;
    (void)argv;
    (void)getenv_fn;
    (void)env_ctx;
    return clopts_parser_result_oom();
}

clopts_result_t *clopts_program_validate(clopts_program_t *program)
{
    (void)program;
    return clopts_parser_result_oom();
}
