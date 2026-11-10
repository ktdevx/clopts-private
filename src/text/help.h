/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_TEXT_HELP_H
#define CLOPTS_SRC_TEXT_HELP_H

#include "clopts/decl/program.h"

#include "text/out.h"

/*
 * Help (help.c): the help text and its Usage line, read from the
 * declaration through the public read functions only. The entry points of
 * write.c check the arguments and make the output.
 */

/* Writes the help of command (NULL: the program itself) of program to out. */
void clopts_help_write(clopts_out_t *out, const clopts_program_t *program, const clopts_command_t *command);

/* Writes the Usage line of command (NULL: the program itself) of program to out. */
void clopts_help_write_usage(clopts_out_t *out, const clopts_program_t *program, const clopts_command_t *command);

#endif /* CLOPTS_SRC_TEXT_HELP_H */
