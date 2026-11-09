# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

clopts_add_module(parser SOURCES parser_program.c command.c option.c parser_positional.c
    parser_decl.c parser_scan.c parser_resolve.c parser_result.c)
