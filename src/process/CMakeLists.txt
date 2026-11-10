# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

set(clopts_process_sources split.c argv.c env.c)
if(WIN32)
    list(APPEND clopts_process_sources win32.c)
else()
    list(APPEND clopts_process_sources posix.c)
endif()

clopts_add_module(process SOURCES ${clopts_process_sources})
