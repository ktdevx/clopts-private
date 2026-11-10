/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_DECL_OPTION_H
#define CLOPTS_DECL_OPTION_H

#include <stdbool.h>

#include "clopts/base/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Option: its kinds and attributes. An option is created by adding it to the
 * program (clopts_program_add_option(), clopts/decl/program.h) or to a
 * command (clopts_command_add_option(), clopts/decl/command.h), and is then
 * configured with the setters below. A setter that fails changes nothing and
 * leaves its error in the program; a NULL option does nothing (the rules are
 * in clopts/decl/program.h). Strings are copied; NULL resets.
 */

typedef struct clopts_option clopts_option_t;

/* Kind of an option. */
typedef enum clopts_option_kind
{
    /* Takes no value. True when given; false via --no-<name> when negatable. */
    CLOPTS_OPTION_FLAG = 0,
    /* Takes no value. Counts occurrences, clamped to the count limit. */
    CLOPTS_OPTION_COUNT = 1,
    /* Takes one value. When repeated, the last one wins. */
    CLOPTS_OPTION_VALUE = 2,
    /* Takes a list value. Repeats are concatenated and split by the delimiter. */
    CLOPTS_OPTION_LIST = 3
} clopts_option_kind_t;

/* One-line description in help. */
CLOPTS_API void clopts_option_set_help(clopts_option_t *option, const char *help);

/* VALUE / LIST: value name shown in help. */
CLOPTS_API void clopts_option_set_value_name(clopts_option_t *option, const char *value_name);

/*
 * VALUE / LIST: environment variable used when argv gives no value. NOT_APPLICABLE, INVALID_NAME (empty,
 * containing '=', or not valid UTF-8).
 */
CLOPTS_API void clopts_option_set_env(clopts_option_t *option, const char *env);

/* VALUE / LIST: default value. For LIST it is split by the delimiter. NOT_APPLICABLE, CONFLICT (required). */
CLOPTS_API void clopts_option_set_default(clopts_option_t *option, const char *default_value);

/* VALUE / LIST: the option must get a value from argv or the environment. NOT_APPLICABLE, CONFLICT (default). */
CLOPTS_API void clopts_option_set_required(clopts_option_t *option, bool required);

/*
 * FLAG: --no-<long_name> is accepted and sets the flag to false. NOT_APPLICABLE, CONFLICT (an option named
 * no-<long_name> in the same program or command).
 */
CLOPTS_API void clopts_option_set_negatable(clopts_option_t *option, bool negatable);

/* LIST: item delimiter. '\0' resets it: each occurrence is a single item. NOT_APPLICABLE. */
CLOPTS_API void clopts_option_set_delimiter(clopts_option_t *option, char delimiter);

/* COUNT: upper limit of the count. 0 resets it to the default (UINT_MAX). NOT_APPLICABLE. */
CLOPTS_API void clopts_option_set_count_max(clopts_option_t *option, unsigned count_max);

/*
 * FLAG: a terminal option. When argv gives it (not as --no-<name>), the
 * parse stops right there with CLOPTS_PARSE_TERMINATED: the remaining tokens are
 * not read and the required options and positionals are not checked
 * (clopts/parse/parse.h). Meant for options such as --help and --version
 * (clopts_program_add_help_option(), clopts/text/write.h). NOT_APPLICABLE.
 */
CLOPTS_API void clopts_option_set_terminal(clopts_option_t *option, bool terminal);

/* ---- read functions: a NULL option is a contract violation (assert) ---- */

/* The kind. */
CLOPTS_API clopts_option_kind_t clopts_option_get_kind(const clopts_option_t *option);
/* Without the leading "--"; NULL for none. */
CLOPTS_API const char *clopts_option_get_long_name(const clopts_option_t *option);
/* '\0' for none. */
CLOPTS_API char clopts_option_get_short_name(const clopts_option_t *option);
/* The string attributes; NULL when not set. */
CLOPTS_API const char *clopts_option_get_help(const clopts_option_t *option);
CLOPTS_API const char *clopts_option_get_value_name(const clopts_option_t *option);
CLOPTS_API const char *clopts_option_get_env(const clopts_option_t *option);
CLOPTS_API const char *clopts_option_get_default(const clopts_option_t *option);
/* '\0' when not set. */
CLOPTS_API char clopts_option_get_delimiter(const clopts_option_t *option);
/* 0 when not set (the default, UINT_MAX). */
CLOPTS_API unsigned clopts_option_get_count_max(const clopts_option_t *option);
CLOPTS_API bool clopts_option_is_required(const clopts_option_t *option);
CLOPTS_API bool clopts_option_is_negatable(const clopts_option_t *option);
CLOPTS_API bool clopts_option_is_terminal(const clopts_option_t *option);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_DECL_OPTION_H */
