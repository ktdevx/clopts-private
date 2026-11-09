/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PARSER_MODEL_H
#define CLOPTS_PARSER_MODEL_H

#include "clopts/command.h"
#include "clopts/parse.h"
#include "clopts/program.h"
#include "core/core_internal.h"
#include "parser/command_internal.h"
#include "parser/option_internal.h"
#include "parser/parser_positional.h"

/*
 * Internal declaration model: the only representation of a declaration, built
 * by the builder functions (option.c, parser_positional.c,
 * command.c and parser_program.c). This header holds the program;
 * commands, options and positionals have their own headers. Everything
 * outside the parser module (help-and-messages included) reads it through the
 * public read functions only.
 *
 * Invariants, guaranteed by the builder:
 *   - Everything attached to a program satisfies every declaration rule:
 *     a builder call that would break one fails and changes nothing. This
 *     holds as long as the caller keeps the ownership contract (no setter or
 *     attach after the attach, no handle attached twice), which is not
 *     checked.
 *   - The address of every handle stays the same until it is released:
 *     children are allocated one by one and the vecs hold pointers only.
 *   - Nothing points to its owner: an option or positional does not point to
 *     its command, and a command does not point to its program. The index of
 *     an option is its position in the vec of the command it is attached to.
 *   - A program never changes after clopts_program_create(): its root and
 *     commands are attached (read-only) from then on.
 */

struct clopts_program
{
    clopts_command_t *root;         /* the root scope; its name is the program name */
    clopts_command_t **commands;    /* command_count elements, in the order given; NULL when there are none */
    size_t command_count;
};

#endif /* CLOPTS_PARSER_MODEL_H */
