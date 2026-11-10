/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_OPTION_H
#define CLOPTS_OPTION_H

#include <stdbool.h>

#include "clopts/decl_types.h"
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Option: its kinds, creation, release and attributes. An option is created
 * unattached, configured, and then attached with clopts_command_add_option()
 * (clopts/command.h); it does not know its command. The checks made here
 * depend on the option alone. Strings are copied; NULL resets. A NULL option
 * passed to a setter is a contract violation (assert).
 */

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

/*
 * Creates an unattached option of kind into *out. long_name excludes the
 * leading "--" (NULL for none); short_name is '\0' for none. At least one of
 * them is required (NO_NAME). INVALID_KIND, INVALID_NAME.
 */
CLOPTS_API clopts_decl_error_t clopts_option_create(clopts_option_kind_t kind, const char *long_name,
                                                    char short_name, clopts_option_t **out);

/*
 * Releases an unattached option. NULL does nothing. An attached option is
 * owned by its command and must not be passed (the behavior is undefined).
 */
CLOPTS_API void clopts_option_destroy(clopts_option_t *option);

/* One-line description in help. */
CLOPTS_API clopts_decl_error_t clopts_option_set_help(clopts_option_t *option, const char *help);

/* VALUE / LIST: value name shown in help. */
CLOPTS_API clopts_decl_error_t clopts_option_set_value_name(clopts_option_t *option, const char *value_name);

/* VALUE / LIST: environment variable used when argv gives no value. ENV_NOT_ALLOWED, INVALID_NAME (empty). */
CLOPTS_API clopts_decl_error_t clopts_option_set_env(clopts_option_t *option, const char *env);

/* VALUE / LIST: default value. For LIST it is split by the delimiter. DEFAULT_NOT_ALLOWED, REQUIRED_WITH_DEFAULT. */
CLOPTS_API clopts_decl_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value);

/* VALUE / LIST: the option must get a value from argv or the environment. REQUIRED_NOT_ALLOWED, REQUIRED_WITH_DEFAULT. */
CLOPTS_API clopts_decl_error_t clopts_option_set_required(clopts_option_t *option, bool required);

/* FLAG: --no-<long_name> is accepted and sets the flag to false. NOT_NEGATABLE. */
CLOPTS_API clopts_decl_error_t clopts_option_set_negatable(clopts_option_t *option, bool negatable);

/* LIST: item delimiter. '\0' resets it: each occurrence is a single item. ATTRIBUTE_KIND. */
CLOPTS_API clopts_decl_error_t clopts_option_set_delimiter(clopts_option_t *option, char delimiter);

/* COUNT: upper limit of the count. 0 resets it to the default (UINT_MAX). ATTRIBUTE_KIND. */
CLOPTS_API clopts_decl_error_t clopts_option_set_count_max(clopts_option_t *option, unsigned count_max);

/*
 * FLAG: a terminal option. When argv gives it (not as --no-<name>), the
 * parse stops right there with CLOPTS_PARSE_TERMINATED: the remaining tokens are
 * not read and the required options and positionals are not checked
 * (clopts/parse.h). Meant for options such as --help and --version
 * (clopts/help.h). ATTRIBUTE_KIND.
 */
CLOPTS_API clopts_decl_error_t clopts_option_set_terminal(clopts_option_t *option, bool terminal);

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

#endif /* CLOPTS_OPTION_H */
