/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PROCESS_ENV_H
#define CLOPTS_PROCESS_ENV_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Environment lookup callback type of clopts_parse_with_env()
 * (clopts/parse/parse.h): a source of environment values other than the process
 * environment, for tests or for values read from elsewhere.
 */

/* Return value of a clopts_getenv_fn. */
typedef enum clopts_env_status
{
    /* Found: *value holds a string borrowed from the callback. */
    CLOPTS_ENV_FOUND = 0,
    /* Not set. *value is not read. */
    CLOPTS_ENV_UNSET = 1,
    /* Lookup failed for lack of memory. The parse ends with CLOPTS_PARSE_OUT_OF_MEMORY. */
    CLOPTS_ENV_FAILED = 2,
    /*
     * Set, but its value cannot be given in UTF-8. *value is not read. The
     * parse ends with CLOPTS_PARSE_USAGE_ERROR (CLOPTS_USAGE_ERROR_INVALID_ENV_VALUE).
     */
    CLOPTS_ENV_INVALID = 3
} clopts_env_status_t;

/*
 * Environment lookup callback. The string stored in *value is owned by the
 * callback and needs to stay valid only until the next call of the callback
 * or until the parse function returns. The library copies it right after the
 * callback returns and never reads or releases it afterwards.
 */
typedef clopts_env_status_t (*clopts_getenv_fn)(void *ctx, const char *name, const char **value);

#ifdef __cplusplus
}
#endif

#endif /* CLOPTS_PROCESS_ENV_H */
