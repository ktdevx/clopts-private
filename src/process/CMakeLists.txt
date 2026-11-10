# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

set(clopts_process_sources argv.c env.c cmdline.c)
if(WIN32)
    list(APPEND clopts_process_sources argv_win32.c env_win32.c utf16_win32.c)
else()
    list(APPEND clopts_process_sources argv_posix.c env_posix.c)
endif()

clopts_add_module(process SOURCES ${clopts_process_sources})
