/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PROCESS_CMDLINE_H
#define CLOPTS_SRC_PROCESS_CMDLINE_H

#include "process/utf16.h"

#include <stddef.h>

/*
 * Command line splitting with the rules of the current MSVC CRT. Pure: never
 * allocates and never calls the OS, so it is built and tested on every OS.
 *
 * Splits cmdline (NUL-terminated). Always stores the element count in *argc
 * (>= 1) and the code units needed for all elements including one NUL each
 * in *units. When buf is not NULL it must hold *units code units from a
 * previous counting call, args must hold *argc pointers, and each args[i] is
 * set to the NUL-terminated element i inside buf. Never fails.
 */
void clopts_cmdline_split(const clopts_utf16_t *cmdline, clopts_utf16_t *buf, clopts_utf16_t **args,
                          size_t *argc, size_t *units);

#endif /* CLOPTS_SRC_PROCESS_CMDLINE_H */
