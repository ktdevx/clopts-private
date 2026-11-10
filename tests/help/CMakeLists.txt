# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

clopts_add_module_tests(help SOURCES
    help_tests_main.c
    test_fixtures.c
    test_out.c
    test_layout.c
    test_option_row.c
    test_message.c
    test_file_write.c
    ../support/test_decl.c
)
