# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Checks that names and files follow each other (docs/dev/design.md): one
# concept has one header, one source file and one prefix. Run with
# `cmake -P`; registered as structure.names (label: structure).
#
# Variables (-D):
#   SOURCE_DIR   Required. The src/ directory of CLOpts.
#   INCLUDE_DIR  Optional. The directory that holds clopts/ (the include/
#                directory of CLOpts).
#
# Checks:
#   - no internal header is named <something>_internal.h;
#   - every function an internal header src/<module>/x.h or an implementation
#     header src/<module>/x_impl.h declares is named clopts_x or
#     clopts_x_<...>, and is defined in src/<module>/x.c or in an OS-specific
#     src/<module>/x_<os>.c; an implementation header has such a file;
#   - the first #include of an internal header src/<module>/x.h whose public
#     header include/clopts/<module>/x.h exists is that public header, and
#     the first #include of src/<module>/x_impl.h is <module>/x.h, or the
#     public header when there is no internal x.h;
#   - every CLOPTS_API function a public header include/clopts/<module>/x.h
#     declares is defined in src/<module>/x.c or src/<module>/x_<os>.c;
#   - every function src/<module>/x.c or src/<module>/x_<os>.c defines
#     (static ones aside) is declared in src/<module>/x.h,
#     src/<module>/x_impl.h or include/clopts/<module>/x.h. <os> is posix or
#     win32.
#
# What counts as a declaration or a definition (after comments are removed;
# every function of the library is written so):
#   - a declaration is a line that starts at column 0 with neither "#",
#     "typedef", "static", "struct", "{" nor "}", and holds clopts_<name>(;
#     the first such name on the line is the function;
#   - a definition is a line that starts at column 0 with neither "#" nor
#     "static", holds <name>( and does not end with ";".
# Exits with an error that lists every offending <file>:<line>.

cmake_minimum_required(VERSION 3.21)

if("${SOURCE_DIR}" STREQUAL "" OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "check_names: SOURCE_DIR is not a directory ('${SOURCE_DIR}')")
endif()
if(NOT "${INCLUDE_DIR}" STREQUAL "" AND NOT IS_DIRECTORY "${INCLUDE_DIR}/clopts")
    message(FATAL_ERROR "check_names: INCLUDE_DIR has no clopts/ directory ('${INCLUDE_DIR}')")
endif()
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)
if(NOT "${INCLUDE_DIR}" STREQUAL "")
    get_filename_component(INCLUDE_DIR "${INCLUDE_DIR}" ABSOLUTE)
endif()

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

# _check_names_declared(<out_var> <file> <public>)
#
# Sets <out_var> to the functions <file> declares, as <name>@<line> items.
# With <public> TRUE only CLOPTS_API declarations count.
function(_check_names_declared out_var file public)
    _check_names_code(lines "${file}")
    set(found "")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "^[ \t#{}]" OR line MATCHES "^(typedef|static|struct)([^A-Za-z0-9_]|$)")
            continue()
        endif()
        if(public AND NOT line MATCHES "^CLOPTS_API[ \t]")
            continue()
        endif()
        if(line MATCHES "(^|[^A-Za-z0-9_])(clopts_[a-z0-9_]+)[ \t]*\\(")
            list(APPEND found "${CMAKE_MATCH_2}@${number}")
        endif()
    endforeach()
    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

# _check_names_defined_in(<out_var> <file>)
#
# Sets <out_var> to the functions <file> defines, as <name>@<line> items.
function(_check_names_defined_in out_var file)
    _check_names_code(lines "${file}")
    set(found "")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "^[ \t#{}]" OR line MATCHES "^static([^A-Za-z0-9_]|$)")
            continue()
        endif()
        if(line MATCHES "${_semicolon}[ \t]*$")
            continue()
        endif()
        if(line MATCHES "(^|[^A-Za-z0-9_])(clopts_[a-z0-9_]+)[ \t]*\\(")
            list(APPEND found "${CMAKE_MATCH_2}@${number}")
        endif()
    endforeach()
    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

# _check_names_defined(<out_var> <dir> <x>)
#
# Sets <out_var> to the functions defined in <dir>/<x>.c and <dir>/<x>_*.c.
function(_check_names_defined out_var dir x)
    file(GLOB sources LIST_DIRECTORIES false "${dir}/${x}.c" "${dir}/${x}_posix.c" "${dir}/${x}_win32.c")
    set(found "")
    foreach(source IN LISTS sources)
        _check_names_code(lines "${source}")
        foreach(line IN LISTS lines)
            if(line MATCHES "^[ \t#{}]" OR line MATCHES "^static([^A-Za-z0-9_]|$)")
                continue()
            endif()
            if(line MATCHES "${_semicolon}[ \t]*$")
                continue()
            endif()
            if(line MATCHES "(^|[^A-Za-z0-9_])(clopts_[a-z0-9_]+)[ \t]*\\(")
                list(APPEND found "${CMAKE_MATCH_2}")
            endif()
        endforeach()
    endforeach()
    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

# _check_names_first_include(<out_var> <file>)
#
# Sets <out_var> to the path in the first #include "..." of <file>, as
# <path>@<line>, or to "" when it has none.
function(_check_names_first_include out_var file)
    _check_names_code(lines "${file}")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "^[ \t]*#[ \t]*include[ \t]*\"([^\"]*)\"")
            set(${out_var} "${CMAKE_MATCH_1}@${number}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} "" PARENT_SCOPE)
endfunction()

set(_offending "")

# ---- internal headers ------------------------------------------------------
file(GLOB_RECURSE _headers LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*.h")
list(SORT _headers)
foreach(_rel IN LISTS _headers)
    set(_file "${SOURCE_DIR}/${_rel}")
    get_filename_component(_dir "${_file}" DIRECTORY)
    get_filename_component(_x "${_rel}" NAME_WE)
    get_filename_component(_module "${_dir}" NAME)
    if(_x MATCHES "_internal$")
        list(APPEND _offending "${_file}:1: an internal header is named after a concept, not <module>_internal.h")
        continue()
    endif()

    # The header the first #include must name: for x_impl.h the header of
    # the concept x, for x.h its public header (when there is one).
    set(_expected "")
    if(_x MATCHES "^(.+)_impl$")
        set(_x "${CMAKE_MATCH_1}")
        if(EXISTS "${_dir}/${_x}.h")
            set(_expected "${_module}/${_x}.h")
        else()
            set(_expected "clopts/${_module}/${_x}.h")
        endif()
        file(GLOB _impl_sources LIST_DIRECTORIES false
            "${_dir}/${_x}.c" "${_dir}/${_x}_posix.c" "${_dir}/${_x}_win32.c")
        if(NOT _impl_sources)
            list(APPEND _offending "${_file}:1: an implementation header needs ${_x}.c or ${_x}_<os>.c")
        endif()
    elseif(NOT "${INCLUDE_DIR}" STREQUAL "" AND EXISTS "${INCLUDE_DIR}/clopts/${_module}/${_x}.h")
        set(_expected "clopts/${_module}/${_x}.h")
    endif()
    if(NOT _expected STREQUAL "")
        _check_names_first_include(_first "${_file}")
        string(REGEX MATCH "^(.*)@(.*)$" _ignored "${_first}")
        if(NOT CMAKE_MATCH_1 STREQUAL _expected)
            list(APPEND _offending "${_file}:1: the first #include is not \"${_expected}\"")
        endif()
    endif()

    _check_names_declared(_declared "${_file}" FALSE)
    _check_names_defined(_defined "${_dir}" "${_x}")
    foreach(_item IN LISTS _declared)
        string(REGEX MATCH "^(.*)@(.*)$" _ignored "${_item}")
        set(_name "${CMAKE_MATCH_1}")
        set(_line "${CMAKE_MATCH_2}")
        if(NOT _name STREQUAL "clopts_${_x}" AND NOT _name MATCHES "^clopts_${_x}_")
            list(APPEND _offending "${_file}:${_line}: '${_name}' is not named after its header (clopts_${_x}_*)")
            continue()
        endif()
        list(FIND _defined "${_name}" _found)
        if(_found EQUAL -1)
            list(APPEND _offending "${_file}:${_line}: '${_name}' is not defined in ${_x}.c or ${_x}_<os>.c")
        endif()
    endforeach()
endforeach()

# ---- public headers --------------------------------------------------------
if(NOT "${INCLUDE_DIR}" STREQUAL "")
    file(GLOB_RECURSE _headers LIST_DIRECTORIES false RELATIVE "${INCLUDE_DIR}/clopts" "${INCLUDE_DIR}/clopts/*.h")
    list(SORT _headers)
    foreach(_rel IN LISTS _headers)
        set(_file "${INCLUDE_DIR}/clopts/${_rel}")
        if(NOT _rel MATCHES "^([^/]+)/([^/]+)\\.h$")
            # clopts.h declares nothing; check_structure.cmake places the others.
            continue()
        endif()
        set(_module "${CMAKE_MATCH_1}")
        set(_x "${CMAKE_MATCH_2}")
        _check_names_declared(_declared "${_file}" TRUE)
        _check_names_defined(_defined "${SOURCE_DIR}/${_module}" "${_x}")
        foreach(_item IN LISTS _declared)
            string(REGEX MATCH "^(.*)@(.*)$" _ignored "${_item}")
            set(_name "${CMAKE_MATCH_1}")
            set(_line "${CMAKE_MATCH_2}")
            list(FIND _defined "${_name}" _found)
            if(_found EQUAL -1)
                list(APPEND _offending
                    "${_file}:${_line}: '${_name}' is not defined in src/${_module}/${_x}.c or ${_x}_<os>.c")
            endif()
        endforeach()
    endforeach()
endif()

# ---- definitions -----------------------------------------------------------
file(GLOB_RECURSE _sources LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*.c")
list(SORT _sources)
foreach(_rel IN LISTS _sources)
    set(_file "${SOURCE_DIR}/${_rel}")
    if(NOT _rel MATCHES "^([^/]+)/([^/]+)\\.c$")
        continue()
    endif()
    set(_module "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "_(posix|win32)$" "" _x "${CMAKE_MATCH_2}")
    set(_declared_names "")
    foreach(_header IN ITEMS "${_x}.h" "${_x}_impl.h")
        if(EXISTS "${SOURCE_DIR}/${_module}/${_header}")
            _check_names_declared(_items "${SOURCE_DIR}/${_module}/${_header}" FALSE)
            list(APPEND _declared_names ${_items})
        endif()
    endforeach()
    if(NOT "${INCLUDE_DIR}" STREQUAL "" AND EXISTS "${INCLUDE_DIR}/clopts/${_module}/${_x}.h")
        _check_names_declared(_items "${INCLUDE_DIR}/clopts/${_module}/${_x}.h" TRUE)
        list(APPEND _declared_names ${_items})
    endif()
    list(TRANSFORM _declared_names REPLACE "@.*$" "")
    _check_names_defined_in(_defined "${_file}")
    foreach(_item IN LISTS _defined)
        string(REGEX MATCH "^(.*)@(.*)$" _ignored "${_item}")
        list(FIND _declared_names "${CMAKE_MATCH_1}" _found)
        if(_found EQUAL -1)
            list(APPEND _offending
                "${_file}:${CMAKE_MATCH_2}: '${CMAKE_MATCH_1}' is not declared in ${_module}/${_x}.h or ${_x}_impl.h")
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
message(STATUS "check_names: every name follows its header and file")
