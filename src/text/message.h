/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_TEXT_MESSAGE_H
#define CLOPTS_SRC_TEXT_MESSAGE_H

#include "clopts/decl/program.h"
#include "clopts/parse/result.h"

#include "text/out.h"

/*
 * Messages (message.c): the one-line texts. The entry points of write.c
 * check the arguments, decide whether there is anything to write and make
 * the output.
 */

/* Writes "<program> <version>\n". version must not be NULL. */
void clopts_message_write_version(clopts_out_t *out, const clopts_program_t *program, const char *version);

/* Writes "Run '<program> [<command>] --help' for usage.\n". */
void clopts_message_write_usage_hint(clopts_out_t *out, const clopts_program_t *program,
                                     const clopts_command_t *command);

/*
 * Writes the error line of result: [prefix] + body + "\n". result is a
 * USAGE_ERROR result whose kind has a body text, or an OUT_OF_MEMORY one.
 */
void clopts_message_write_error(clopts_out_t *out, const clopts_program_t *program, const clopts_result_t *result,
                                unsigned flags);

/* Body template of a usage error kind; NULL for CLOPTS_USAGE_ERROR_NONE and values outside the enum. */
const char *clopts_message_usage_text(clopts_usage_error_kind_t kind);

#endif /* CLOPTS_SRC_TEXT_MESSAGE_H */
