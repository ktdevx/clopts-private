/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PARSE_TOKEN_H
#define CLOPTS_SRC_PARSE_TOKEN_H

#include <stddef.h>

/*
 * Token (token.c): what one argv element looks like, before anything is
 * looked up. It knows no declaration, so "--", "-" alone and "-x" read the
 * same whatever the program declares.
 */

typedef enum clopts_token_kind
{
    CLOPTS_TOKEN_DASHDASH,   /* "--" */
    CLOPTS_TOKEN_LONG,       /* "--<name>" or "--<name>=<value>" (anything starting with "--" but "--") */
    CLOPTS_TOKEN_SHORT,      /* "-<chars>": "-" and at least one more byte, the second not "-" */
    CLOPTS_TOKEN_WORD        /* anything else: no leading "-", or "-" alone */
} clopts_token_kind_t;

typedef struct clopts_token
{
    clopts_token_kind_t kind;
    /*
     * LONG: the name after "--", up to the first "=" (name_len bytes; it may
     * be empty). SHORT: the characters after "-" (name_len >= 1 bytes).
     * Otherwise NULL and 0. Points into the element.
     */
    const char *name;
    size_t name_len;
    /* LONG: the text after the first "=", or NULL without one. Otherwise NULL. */
    const char *value;
} clopts_token_t;

/* Reads the argv element text (not NULL). */
clopts_token_t clopts_token_read(const char *text);

#endif /* CLOPTS_SRC_PARSE_TOKEN_H */
