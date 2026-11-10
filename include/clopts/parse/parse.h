/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSE_PARSE_H
#define CLOPTS_PARSE_PARSE_H

#include "clopts/base/export.h"
#include "clopts/decl/program.h"
#include "clopts/parse/result.h"
#include "clopts/process/env.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse API. clopts_parse*() return a result handle that is read with the
 * accessors of clopts/parse/result.h and released with
 * clopts_result_destroy(). A program built with the builder functions of
 * clopts/decl/program.h is always valid, so a parse never reports a
 * declaration error.
 *
 * Lookup: an option token is looked up in the selected command first and
 * then in the program (the global options). In each of them the exact long
 * name comes first and --no-<name> of a negatable FLAG second, so a name of
 * a command shadows the same global option (clopts_scope_shadows_option()).
 * The parser knows no built-in option or command: --help, --version and a
 * help command are ordinary declarations (clopts/text/write.h).
 *
 * Terminal options (clopts_option_set_terminal()): when argv gives one, the
 * scan stops right after it, the parse ends with CLOPTS_PARSE_TERMINATED and
 * clopts_result_get_terminal_option() returns it. The tokens after it are
 * not read and the required options and positionals are not checked. A usage error found
 * before it is still replaced: after the first usage error, the following
 * tokens up to "--" are searched for an exact "--<long>" or "-<c>" of a
 * terminal option of the scope, and the first one found gives
 * CLOPTS_PARSE_TERMINATED instead.
 *
 * Threads: parsing never writes to any stream, never calls exit() and never
 * writes to the program, so the same program may be parsed from several
 * threads at the same time.
 */

/*
 * Parses argv[1..argc-1] (argv[0] is skipped) against program. argc must be
 * >= 0 and argv[0..argc-1] NUL-terminated (argv is not read when argc == 0).
 * Neither program's declaration nor argv is modified. Environment variables
 * are read from the process environment in UTF-8: on Windows with
 * GetEnvironmentVariableW() and converted, elsewhere with getenv() and
 * passed through as is. Never returns NULL: when an allocation fails (or
 * program is NULL) the shared out-of-memory sentinel is returned. A program
 * with a declaration error (clopts_program_get_error()) is a contract
 * violation (caught by assert).
 */
CLOPTS_API clopts_result_t *clopts_parse(const clopts_program_t *program, int argc, char *const *argv);

/*
 * Same as clopts_parse(), with env_fn(env_ctx, ...) as the source of the
 * environment values instead of the process environment. NULL selects the
 * process environment, as clopts_parse() does.
 */
CLOPTS_API clopts_result_t *clopts_parse_with_env(const clopts_program_t *program, int argc, char *const *argv,
                                                  clopts_env_fn env_fn, void *env_ctx);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PARSE_PARSE_H */
