/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Name rules: what a program, command, option or environment variable name
 * may be. How names are compared with each other is in scope.c.
 */

#include "decl/name.h"

#include "base/utf8.h"

#include <assert.h>
#include <string.h>

/* True for the white space characters of the C locale. */
static bool is_space(char c)
{
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\v') || (c == '\f') || (c == '\r'));
}

/* True when s contains a white space character. */
static bool has_space(const char *s)
{
    for (; *s != '\0'; s++)
    {
        if (is_space(*s))
        {
            return true;
        }
    }
    return false;
}

bool clopts_name_valid_command(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (!has_space(name)));
}

bool clopts_name_valid_long(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (name[0] != '-') && (strchr(name, '=') == NULL)
            && (!has_space(name)));
}

bool clopts_name_valid_short(char c)
{
    /* '\0' means "no short name" and is not a name. */
    return ((c != '\0') && (c != '-') && (c != '=') && (!is_space(c)));
}

bool clopts_name_valid_env(const char *name)
{
    return ((name != NULL) && (name[0] != '\0') && (strchr(name, '=') == NULL) && clopts_utf8_valid(name));
}
