# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

clopts_add_module(parser SOURCES program.c command.c option.c positional.c scope.c
    names.c scan.c resolve.c result.c)
