# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

set(clopts_platform_sources platform_split.c platform_argv.c platform_env.c)
if(WIN32)
    list(APPEND clopts_platform_sources platform_win32.c)
else()
    list(APPEND clopts_platform_sources platform_posix.c)
endif()

clopts_add_module(platform SOURCES ${clopts_platform_sources})
