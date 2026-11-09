# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

clopts_add_module_tests(parser SOURCES
    parser_tests_main.c
    test_fixtures.c
    test_build.c
    test_decl_check.c
    test_decl_option.c
    test_decl_positional.c
    test_dispatch.c
    test_long.c
    test_short.c
    test_values.c
    test_positional.c
    test_resolve.c
    test_errors.c
    test_result.c
    test_oom.c
    ../support/test_alloc.c
    ../support/test_decl.c
)
