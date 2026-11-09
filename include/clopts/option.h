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
 * depend on the option alone. Strings are copied; NULL resets.
 */

/* Kind of an option. The values are part of the ABI. */
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
CLOPTS_API CLOPTS_NODISCARD clopts_decl_error_t clopts_option_new(clopts_option_kind_t kind, const char *long_name,
                                                                  char short_name, clopts_option_t **out);

/*
 * Releases an unattached option. NULL does nothing. Passing an attached
 * option is a contract violation caught by assert (without assert it does
 * nothing).
 */
CLOPTS_API void clopts_option_free(clopts_option_t *option);

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

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_OPTION_H */
