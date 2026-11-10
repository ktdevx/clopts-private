# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Compiler options for CLOpts' own targets.
#
# Everything is applied per target and PRIVATE so that nothing propagates to
# consuming targets. Directory properties, CMAKE_* variables and the MSVC
# runtime library are never touched.

include_guard(GLOBAL)

# clopts_apply_compiler_options(<target>)
#
# Applies the C99 requirement, warning flags and internal compile definitions
# to <target>. Warnings are turned into errors when CLOPTS_WERROR is ON.
function(clopts_apply_compiler_options target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "clopts_apply_compiler_options: '${target}' is not a target "
            "(see cmake/CloptsCompilerOptions.cmake)")
    endif()

    target_compile_features("${target}" PUBLIC c_std_99)
    set_target_properties("${target}" PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    if(MSVC)
        # Covers both cl and clang-cl.
        target_compile_options("${target}" PRIVATE /std:c11 /W4 /utf-8 /Zc:preprocessor)
        target_compile_definitions("${target}" PRIVATE _CRT_SECURE_NO_WARNINGS)
        if(CLOPTS_WERROR)
            target_compile_options("${target}" PRIVATE /WX)
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Werror=vla)
        if(CLOPTS_WERROR)
            target_compile_options("${target}" PRIVATE -Werror)
        endif()
        if(MINGW)
            target_compile_definitions("${target}" PRIVATE __USE_MINGW_ANSI_STDIO=1)
        endif()
    endif()
endfunction()

# clopts_apply_library_options(<target>)
#
# Options for targets that compile the library sources (clopts and
# clopts_testing), on top of clopts_apply_compiler_options(). With GCC and
# Clang, -Wmissing-prototypes reports a non-static function definition that no
# header declares: a library function is either declared in a header or static.
# Tests and examples do not get it (Naos test functions have no prototypes).
# MSVC has no equivalent warning.
function(clopts_apply_library_options target)
    clopts_apply_compiler_options("${target}")
    if(NOT MSVC AND CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options("${target}" PRIVATE -Wmissing-prototypes)
    endif()
endfunction()
