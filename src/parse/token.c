/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Token: the one place that tells "--", long options, short option groups
 * and plain words apart. The scan, the pre-command phase and the recovery
 * scan all read argv elements through it.
 */

#include "parse/token.h"

#include <assert.h>
#include <string.h>

clopts_token_t clopts_token_read(const char *text)
{
    clopts_token_t token;

    assert(text != NULL);
    token.kind = CLOPTS_TOKEN_WORD;
    token.name = NULL;
    token.name_len = 0;
    token.value = NULL;

    if ((text[0] != '-') || (text[1] == '\0'))
    {
        return token;
    }
    if (text[1] != '-')
    {
        token.kind = CLOPTS_TOKEN_SHORT;
        token.name = text + 1;
        token.name_len = strlen(text + 1);
        return token;
    }
    if (text[2] == '\0')
    {
        token.kind = CLOPTS_TOKEN_DASHDASH;
        return token;
    }
    token.kind = CLOPTS_TOKEN_LONG;
    token.name = text + 2;
    token.name_len = strcspn(token.name, "=");
    if (token.name[token.name_len] == '=')
    {
        token.value = token.name + token.name_len + 1;
    }
    return token;
}
