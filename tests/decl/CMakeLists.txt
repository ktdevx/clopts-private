# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

clopts_add_module_tests(decl SOURCES
    decl_tests_main.c
    test_build.c
    test_decl_check.c
    test_decl_option.c
    test_decl_positional.c
    ../support/test_alloc.c
    ../support/test_decl.c
)
