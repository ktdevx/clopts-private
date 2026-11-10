# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

clopts_add_module_tests(core SOURCES core_tests_main.c test_size.c test_vec.c test_alloc.c
    ../support/test_alloc.c)
