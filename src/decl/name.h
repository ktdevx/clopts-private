/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_NAME_H
#define CLOPTS_SRC_DECL_NAME_H

#include <stdbool.h>

/*
 * Name rules (name.c): what a name of each kind may be, checked by the
 * constructors and setters. How names are compared with each other is in
 * scope.c.
 */

/* Program and command name rule: not NULL, not empty, no leading '-', no white space. */
bool clopts_name_valid_command(const char *name);

/* Long name rule: not empty, no leading '-', no '=' and no white space. */
bool clopts_name_valid_long(const char *name);

/* Short name rule: not '-', '=' or white space. */
bool clopts_name_valid_short(char c);

/*
 * Environment variable name rule: not empty, no '=' (no system can look such
 * a name up) and valid UTF-8 (the name is converted on Windows).
 */
bool clopts_name_valid_env(const char *name);

#endif /* CLOPTS_SRC_DECL_NAME_H */
