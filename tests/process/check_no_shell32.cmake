# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Checks that nothing under src/ uses shell32. Run with
# `cmake -P`; registered as process.no_shell32 (label: process).
#
# Required variables (-D):
#   SOURCE_DIR  Directory to scan (the src/ directory of CLOpts).
#
# Only forms of use are matched, after comments are removed, so that a comment
# that mentions shell32 is not reported:
#   .c / .h                      an #include of shellapi.h, a call of
#                                CommandLineToArgv* (the name followed by "("),
#                                a #pragma comment(lib, ...) naming shell32
#   CMakeLists.txt / *.cmake     shell32 as a link item (shell32, -lshell32,
#                                shell32.lib), e.g. in target_link_libraries()
# Matching ignores case (Windows file names and link items are not
# case-sensitive). Exits with an error that lists every offending
# <file>:<line> when something is found.

if("${SOURCE_DIR}" STREQUAL "" OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "check_no_shell32: SOURCE_DIR is not a directory ('${SOURCE_DIR}')")
endif()

# Characters that cannot appear in the scanned text stand in for ";", "[" and
# "]" while the file is handled as a CMake list of lines.
string(ASCII 1 _semicolon)
string(ASCII 2 _open_bracket)
string(ASCII 3 _close_bracket)

# _check_no_shell32_lines(<out_var> <file>)
#
# Sets <out_var> to the lines of <file> as a list (empty lines kept, so that
# the position in the list is the line number minus one), with ";", "[" and
# "]" replaced by the stand-ins above.
function(_check_no_shell32_lines out_var file)
    file(READ "${file}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE ";" "${_semicolon}" content "${content}")
    string(REPLACE "[" "${_open_bracket}" content "${content}")
    string(REPLACE "]" "${_close_bracket}" content "${content}")
    string(REPLACE "\n" ";" content "${content}")
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

set(_offending "")

# ---- C sources: strip /* */ (also across lines) and // comments -----------
file(GLOB_RECURSE _c_files LIST_DIRECTORIES false "${SOURCE_DIR}/*.c" "${SOURCE_DIR}/*.h")
list(SORT _c_files)
foreach(_file IN LISTS _c_files)
    _check_no_shell32_lines(_lines "${_file}")
    set(_line_number 0)
    set(_in_block_comment FALSE)
    foreach(_line IN LISTS _lines)
        math(EXPR _line_number "${_line_number} + 1")
        set(_code "")
        set(_rest "${_line}")
        while(NOT "${_rest}" STREQUAL "")
            if(_in_block_comment)
                string(FIND "${_rest}" "*/" _end)
                if(_end EQUAL -1)
                    set(_rest "")
                else()
                    math(EXPR _end "${_end} + 2")
                    string(SUBSTRING "${_rest}" ${_end} -1 _rest)
                    set(_in_block_comment FALSE)
                endif()
            else()
                string(FIND "${_rest}" "/*" _block)
                string(FIND "${_rest}" "//" _line_comment)
                if(NOT _line_comment EQUAL -1 AND (_block EQUAL -1 OR _line_comment LESS _block))
                    string(SUBSTRING "${_rest}" 0 ${_line_comment} _head)
                    string(APPEND _code "${_head}")
                    set(_rest "")
                elseif(NOT _block EQUAL -1)
                    string(SUBSTRING "${_rest}" 0 ${_block} _head)
                    string(APPEND _code "${_head} ")
                    math(EXPR _block "${_block} + 2")
                    string(SUBSTRING "${_rest}" ${_block} -1 _rest)
                    set(_in_block_comment TRUE)
                else()
                    string(APPEND _code "${_rest}")
                    set(_rest "")
                endif()
            endif()
        endwhile()
        string(TOLOWER "${_code}" _code)
        set(_reason "")
        if(_code MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]*[/\\\\])?shellapi\\.h[ \t]*[>\"]")
            set(_reason "#include of shellapi.h")
        elseif(_code MATCHES "(^|[^a-z0-9_])commandlinetoargv[a-z0-9_]*[ \t]*\\(")
            set(_reason "call of CommandLineToArgv")
        elseif(_code MATCHES "^[ \t]*#[ \t]*pragma[ \t]+comment[ \t]*\\(.*shell32")
            set(_reason "#pragma comment linking shell32")
        endif()
        if(NOT _reason STREQUAL "")
            list(APPEND _offending "${_file}:${_line_number}: ${_reason}")
        endif()
    endforeach()
endforeach()

# ---- CMake files: strip # comments ----------------------------------------
file(GLOB_RECURSE _cmake_files LIST_DIRECTORIES false
    "${SOURCE_DIR}/CMakeLists.txt" "${SOURCE_DIR}/*.cmake")
list(SORT _cmake_files)
foreach(_file IN LISTS _cmake_files)
    _check_no_shell32_lines(_lines "${_file}")
    set(_line_number 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _line_number "${_line_number} + 1")
        string(FIND "${_line}" "#" _comment)
        if(NOT _comment EQUAL -1)
            string(SUBSTRING "${_line}" 0 ${_comment} _line)
        endif()
        string(TOLOWER "${_line}" _line)
        if(_line MATCHES "(^|[^a-z0-9_.-])(-l)?shell32(\\.lib)?([^a-z0-9_.]|$)")
            list(APPEND _offending "${_file}:${_line_number}: shell32 as a link item")
        endif()
    endforeach()
endforeach()

if(_offending)
    list(LENGTH _offending _count)
    list(JOIN _offending "\n  " _report)
    message(FATAL_ERROR
        "check_no_shell32: ${_count} use(s) of shell32 under ${SOURCE_DIR} "
        "(CLOpts must not depend on shell32):\n  ${_report}")
endif()
message(STATUS "check_no_shell32: no use of shell32 under ${SOURCE_DIR}")
