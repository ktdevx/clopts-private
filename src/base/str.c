/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Owned strings: copies allocated through the allocation gateway
 * (base/alloc_internal.h).
 */

#include "base/str.h"

#include "base/alloc_internal.h"
#include "base/size.h"

#include <assert.h>
#include <string.h>

char *clopts_strndup(const char *s, size_t len)
{
    size_t bytes;
    char *copy;

    assert(s != NULL);

    if (clopts_size_add(len, 1, &bytes) != 0)
    {
        return NULL;
    }

    copy = (char *)clopts_malloc(bytes);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, s, len);
    copy[len] = '\0';

    return copy;
}

char *clopts_strdup(const char *s)
{
    if (s == NULL)
    {
        return NULL;
    }

    return clopts_strndup(s, strlen(s));
}

int clopts_strrep(char **field, const char *value)
{
    char *copy;

    assert(field != NULL);

    copy = clopts_strdup(value);
    if ((value != NULL) && (copy == NULL))
    {
        return -1;
    }

    clopts_free(*field);
    *field = copy;

    return 0;
}
