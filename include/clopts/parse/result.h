/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSE_RESULT_H
#define CLOPTS_PARSE_RESULT_H

#include <stdbool.h>
#include <stddef.h>

#include "clopts/base/export.h"
#include "clopts/decl/program.h"
#include "clopts/parse/usage_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Result of a parse (clopts/parse/parse.h): its status, the selected
 * command, the terminal option, the usage error and the values. A result
 * never changes after the parse returns it. It is read with the accessors
 * below and released with clopts_result_destroy().
 *
 * String lifetime: a string obtained from a result (values, list items,
 * positionals, error tokens) is valid until the result is released, and only
 * while the caller keeps both the program and argv alive. Accessors may only
 * be called before the program that produced the result is released;
 * clopts_result_destroy() may be called before or after clopts_program_destroy().
 *
 * NULL handling: a NULL result is read like the out-of-memory sentinel
 * (status CLOPTS_PARSE_OUT_OF_MEMORY). A NULL option makes a value accessor return
 * the unset value. Neither is a contract violation.
 *
 * An option that is attached neither to the program nor to the selected command
 * (an option of another command or program, or an unattached one) reads as
 * unset.
 *
 * Contract violations (caught by assert): an index out of range.
 */

typedef struct clopts_result clopts_result_t;

/* Outcome of a parse. */
typedef enum clopts_parse_status
{
    /* Parsed successfully. */
    CLOPTS_PARSE_OK = 0,
    /* A terminal option was given (clopts_result_get_terminal_option()). Required ones were not checked. */
    CLOPTS_PARSE_TERMINATED = 1,
    /* The program has subcommands but none appeared. Only global option values are available. */
    CLOPTS_PARSE_NO_COMMAND = 2,
    /* A usage error in argv. */
    CLOPTS_PARSE_USAGE_ERROR = 3,
    /* An allocation failed, or the environment lookup reported CLOPTS_ENV_FAILED. */
    CLOPTS_PARSE_OUT_OF_MEMORY = 4
} clopts_parse_status_t;

/* Where an option's value came from. */
typedef enum clopts_source
{
    CLOPTS_SOURCE_NONE = 0,
    CLOPTS_SOURCE_DEFAULT = 1,
    CLOPTS_SOURCE_ENV = 2,
    CLOPTS_SOURCE_EXPLICIT = 3
} clopts_source_t;

CLOPTS_API clopts_parse_status_t clopts_result_get_status(const clopts_result_t *result);

/* OK / TERMINATED / NO_COMMAND / USAGE_ERROR: the selected command. NULL without one. */
CLOPTS_API const clopts_command_t *clopts_result_get_command(const clopts_result_t *result);

/* TERMINATED: the terminal option that ended the parse. NULL for any other status. */
CLOPTS_API const clopts_option_t *clopts_result_get_terminal_option(const clopts_result_t *result);

/*
 * USAGE_ERROR: what was wrong (clopts/parse/usage_error.h), valid until
 * result is released. NULL for any other status.
 */
CLOPTS_API const clopts_usage_error_t *clopts_result_get_usage_error(const clopts_result_t *result);

/*
 * Value accessors. Meaningful when the status is OK, TERMINATED (the values
 * recorded up to the terminal option, then the environment and defaults) or
 * NO_COMMAND (global options only); otherwise, and for an option of a
 * command that was not selected, they return the unset value (false, 0,
 * NULL, 0 items, CLOPTS_SOURCE_NONE).
 */
CLOPTS_API bool clopts_result_get_flag(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API unsigned clopts_result_get_count(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API const char *clopts_result_get_value(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API size_t clopts_result_get_list_count(const clopts_result_t *result, const clopts_option_t *option);
/* index must be less than clopts_result_get_list_count(). */
CLOPTS_API const char *clopts_result_get_list_at(const clopts_result_t *result, const clopts_option_t *option,
                                                  size_t index);
CLOPTS_API clopts_source_t clopts_result_get_source(const clopts_result_t *result, const clopts_option_t *option);
CLOPTS_API size_t clopts_result_get_positional_count(const clopts_result_t *result);
/* index must be less than clopts_result_get_positional_count(). Returns an argv element. */
CLOPTS_API const char *clopts_result_get_positional_at(const clopts_result_t *result, size_t index);

/* Releases result. NULL and the sentinel do nothing. May be called before or after clopts_program_destroy(). */
CLOPTS_API void clopts_result_destroy(clopts_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PARSE_RESULT_H */
