# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Checks the names of the internal headers of src/ (docs/dev/design.md): an
# internal function is named after the header that declares it, so that its
# name leads to its file. Run with `cmake -P`; registered as structure.names
# (label: structure).
#
# Variables (-D):
#   SOURCE_DIR   Required. The src/ directory of CLOpts.
#
# Checks:
#   - no internal header is named <something>_internal.h;
#   - every function an internal header src/<module>/x.h or an implementation
#     header src/<module>/x_impl.h declares is named clopts_x or
#     clopts_x_<...>.
#
# Public headers are not checked: a public name outside a type and its
# operations is named by what it means, not by its header. A definition that
# no header declares is reported by the compiler (-Wmissing-prototypes).
#
# A declaration is a line that, after comments are removed, starts at column 0
# with neither "#", "typedef", "static", "struct", "{" nor "}", and holds
# clopts_<name>(; the first such name on the line is the function. Every
# function of the library is written so.
# Exits with an error that lists every offending <file>:<line>.

cmake_minimum_required(VERSION 3.21)

if("${SOURCE_DIR}" STREQUAL "" OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "check_names: SOURCE_DIR is not a directory ('${SOURCE_DIR}')")
endif()
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)

# Characters that cannot appear in the scanned text stand in for ";", "[" and
# "]" while a file is handled as a CMake list of lines.
string(ASCII 1 _semicolon)
string(ASCII 2 _open_bracket)
string(ASCII 3 _close_bracket)

# _check_names_code(<out_var> <file>)
#
# Sets <out_var> to the lines of <file> as a list, with comments removed
# (their line breaks kept, so the position in the list is the line number
# minus one) and ";", "[" and "]" replaced by the stand-ins above.
function(_check_names_code out_var file)
    file(READ "${file}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE ";" "${_semicolon}" content "${content}")
    string(REPLACE "[" "${_open_bracket}" content "${content}")
    string(REPLACE "]" "${_close_bracket}" content "${content}")
    string(REPLACE "\n" ";" lines "${content}")
    set(result "")
    set(in_comment FALSE)
    foreach(line IN LISTS lines)
        set(code "")
        set(rest "${line}")
        while(NOT "${rest}" STREQUAL "")
            if(in_comment)
                string(FIND "${rest}" "*/" end)
                if(end EQUAL -1)
                    set(rest "")
                else()
                    math(EXPR end "${end} + 2")
                    string(SUBSTRING "${rest}" ${end} -1 rest)
                    set(in_comment FALSE)
                endif()
            else()
                string(FIND "${rest}" "/*" block)
                string(FIND "${rest}" "//" line_comment)
                if(NOT line_comment EQUAL -1 AND (block EQUAL -1 OR line_comment LESS block))
                    string(SUBSTRING "${rest}" 0 ${line_comment} head)
                    string(APPEND code "${head}")
                    set(rest "")
                elseif(NOT block EQUAL -1)
                    string(SUBSTRING "${rest}" 0 ${block} head)
                    string(APPEND code "${head} ")
                    math(EXPR block "${block} + 2")
                    string(SUBSTRING "${rest}" ${block} -1 rest)
                    set(in_comment TRUE)
                else()
                    string(APPEND code "${rest}")
                    set(rest "")
                endif()
            endif()
        endwhile()
        list(APPEND result "${code}")
    endforeach()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# _check_names_declared(<out_var> <file>)
#
# Sets <out_var> to the functions <file> declares, as <name>@<line> items.
function(_check_names_declared out_var file)
    _check_names_code(lines "${file}")
    set(found "")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "^[ \t#{}]" OR line MATCHES "^(typedef|static|struct)([^A-Za-z0-9_]|$)")
            continue()
        endif()
        if(line MATCHES "(^|[^A-Za-z0-9_])(clopts_[a-z0-9_]+)[ \t]*\\(")
            list(APPEND found "${CMAKE_MATCH_2}@${number}")
        endif()
    endforeach()
    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

set(_offending "")

file(GLOB_RECURSE _headers LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*.h")
list(SORT _headers)
foreach(_rel IN LISTS _headers)
    set(_file "${SOURCE_DIR}/${_rel}")
    get_filename_component(_x "${_rel}" NAME_WE)
    if(_x MATCHES "_internal$")
        list(APPEND _offending "${_file}:1: an internal header is named after a concept, not <module>_internal.h")
        continue()
    endif()
    if(_x MATCHES "^(.+)_impl$")
        set(_x "${CMAKE_MATCH_1}")
    endif()

    _check_names_declared(_declared "${_file}")
    foreach(_item IN LISTS _declared)
        string(REGEX MATCH "^(.*)@(.*)$" _ignored "${_item}")
        set(_name "${CMAKE_MATCH_1}")
        set(_line "${CMAKE_MATCH_2}")
        if(NOT _name STREQUAL "clopts_${_x}" AND NOT _name MATCHES "^clopts_${_x}_")
            list(APPEND _offending
                "${_file}:${_line}: '${_name}' is not named after its header (clopts_${_x}_*)")
        endif()
    endforeach()
endforeach()

if(_offending)
    list(LENGTH _offending _count)
    list(JOIN _offending "\n  " _report)
    message(FATAL_ERROR
        "check_names: ${_count} violation(s) of the naming rules:\n  ${_report}\n"
        "The rules are described in docs/dev/design.md.")
endif()
message(STATUS "check_names: every internal function is named after its header")
