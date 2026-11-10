# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

clopts_add_module_tests(base SOURCES base_tests_main.c test_size.c test_vec.c test_alloc.c test_string.c test_utf8.c
    ../support/test_alloc.c)
