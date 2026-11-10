/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_ENV_H
#define CLOPTS_ENV_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Environment lookup callback type, shared by the parser
 * (clopts_parse_with_env(), clopts/parse.h) and the platform layer
 * (clopts_env_lookup(), clopts/platform.h). Neither header depends on the
 * other: both include this one.
 */

/* Return value of a clopts_getenv_fn. */
typedef enum clopts_env_status
{
    /* Found: *value holds a string borrowed from the callback. */
    CLOPTS_ENV_FOUND = 0,
    /* Not set. *value is not read. */
    CLOPTS_ENV_UNSET = 1,
    /* Lookup failed. The parse ends with CLOPTS_PARSE_OUT_OF_MEMORY. */
    CLOPTS_ENV_FAILED = 2
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

#endif /* CLOPTS_ENV_H */
