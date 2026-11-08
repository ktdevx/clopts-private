/*
 * Entry points of parsing and validation (clopts_parse,
 * clopts_parse_with_env, clopts_program_validate) and the scanner.
 *
 * Implemented: the common entry (NULL, freezing, a recorded error, the
 * allocation of the result) and clopts_program_validate(). Skeleton: the
 * parse functions still return the out-of-memory sentinel; task 7.2
 * implements them and the scanner on top of scan_enter().
 */

#include "parser/parser_internal.h"

/*
 * Common entry of parsing and validation. Freezes program, then returns the
 * sentinel when program is NULL, has a recorded OUT_OF_MEMORY or the result
 * cannot be allocated. Otherwise returns a new result; when program has a
 * recorded declaration error, the result has status DECL_ERROR with that
 * error copied (the root reported as NULL), and *done is set to true.
 */
static clopts_result_t *scan_enter(clopts_program_t *program, bool *done)
{
    clopts_result_t *result;

    *done = true;
    if (program == NULL)
    {
        return clopts_parser_result_oom();
    }
    program->frozen = true;
    if (program->error.status == CLOPTS_OUT_OF_MEMORY)
    {
        return clopts_parser_result_oom();
    }

    result = clopts_parser_result_new(program);
    if (result == NULL)
    {
        return clopts_parser_result_oom();
    }

    if (program->error.status == CLOPTS_DECL_ERROR)
    {
        result->status = CLOPTS_DECL_ERROR;
        result->error.decl = program->error.kind;
        result->error.command = (program->error.command == &program->root) ? NULL : program->error.command;
        result->error.option = program->error.option;
        result->error.positional = program->error.positional;
        return result;
    }

    *done = false;
    return result;
}

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
    clopts_result_t *result;
    bool done;

    result = scan_enter(program, &done);
    if (!done)
    {
        result->status = CLOPTS_OK;
    }
    return result;
}
