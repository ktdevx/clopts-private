/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_DECL_COMMAND_INTERNAL_H
#define CLOPTS_SRC_DECL_COMMAND_INTERNAL_H

#include "clopts/decl/command.h"

#include "clopts/base/error.h"

/*
 * Command (command.c): the creation and release used by the program that
 * holds the command (program.c). The public functions are in
 * clopts/decl/command.h.
 */

/*
 * Creates a command named name into *out, reporting to *error (the error of
 * its program): INVALID_NAME, then OUT_OF_MEMORY. *out is NULL on failure.
 * The caller adds it to the program.
 */
clopts_error_t clopts_command_create(const char *name, clopts_error_t *error, clopts_command_t **out);

/* Releases command with its options and positionals. NULL does nothing. */
void clopts_command_destroy(clopts_command_t *command);

#endif /* CLOPTS_SRC_DECL_COMMAND_INTERNAL_H */
