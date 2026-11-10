# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Checks the names of the headers of src/ and include/clopts/
# (docs/dev/design.md): a struct and its operations form one pair, so that
# the name of an operation leads to its file. Run with `cmake -P`; registered as
# structure.names (label: structure).
#
# Variables (-D):
#   SOURCE_DIR    Required. The src/ directory of CLOpts.
#   INCLUDE_DIR   Optional. The include/ directory of CLOpts (the one that
#                 holds clopts/).
#
# Checks:
#   - no internal header is named <module>_internal.h;
#   - with INCLUDE_DIR: an internal header of a concept that has a public
#     header clopts/<module>/x.h is named <module>/x_internal.h, never
#     <module>/x.h, and an internal header <module>/x_internal.h has that
#     public header;
#   - an operation of a struct, a function named clopts_x or clopts_x_<...>
#     where clopts_x_t is a struct, is declared in a header that declares
#     clopts_x_t. The header of a struct is <module>/x.h, public or internal;
#     an internal header <module>/x_internal.h counts as <module>/x.h. When
#     several structs match, the longest name wins (clopts_usage_error_t over
#     a clopts_usage_t).
#
# Any other name is given by what it means and is not checked: a function
# outside a struct, and a function named after an enumeration or another
# typedef (clopts_error_get_message). A definition that no header declares is
# reported by the compiler (-Wmissing-prototypes).
#
# A declaration is a line that, after comments are removed, starts at column 0
# with neither "#", "typedef", "static", "struct", "{" nor "}", and holds
# clopts_<name>(; the first such name on the line is the function. A struct is
# declared by a line "typedef struct ... clopts_<name>_t;", or by a line
# "} clopts_<name>_t;" that closes a definition opened by a line that starts
# with "typedef struct". Every function and struct of the library is written
# so.
# Exits with an error that lists every offending <file>:<line>.

cmake_minimum_required(VERSION 3.21)

if("${SOURCE_DIR}" STREQUAL "" OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "check_names: SOURCE_DIR is not a directory ('${SOURCE_DIR}')")
endif()
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)
if(NOT "${INCLUDE_DIR}" STREQUAL "")
    if(NOT IS_DIRECTORY "${INCLUDE_DIR}/clopts")
        message(FATAL_ERROR "check_names: INCLUDE_DIR has no clopts/ directory ('${INCLUDE_DIR}')")
    endif()
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

set(_offending "")
set(_types "")      # x of every struct clopts_x_t
set(_functions "")  # <name>|<line>|<file>|<key> of every function

# _check_names_scan(<root> <rel>)
#
# Reads the header <root>/<rel>, whose key is <rel> without ".h" and
# "_internal" (decl/scope for decl/scope_internal.h). Adds the structs it
# declares to _types and _type_homes_<x> (the keys of the headers that declare
# clopts_x_t), and the functions it declares to _functions.
macro(_check_names_scan root rel)
    set(_file "${root}/${rel}")
    string(REGEX REPLACE "(_internal)?\\.h$" "" _key "${rel}")
    _check_names_code(_lines "${_file}")
    set(_number 0)
    set(_open "")  # what the typedef being defined is: struct, enum or union
    foreach(_line IN LISTS _lines)
        math(EXPR _number "${_number} + 1")
        set(_x "")
        if(_line MATCHES "^typedef[ \t]+struct[^A-Za-z0-9_][^${_semicolon}]*[^A-Za-z0-9_]clopts_([a-z0-9_]+)_t[ \t]*${_semicolon}")
            set(_x "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^typedef[ \t]+(struct|enum|union)([^A-Za-z0-9_][^${_semicolon}]*)?$")
            set(_open "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^\\}[^${_semicolon}]*[^A-Za-z0-9_]clopts_([a-z0-9_]+)_t[ \t]*${_semicolon}")
            if(_open STREQUAL "struct")
                set(_x "${CMAKE_MATCH_1}")
            endif()
            set(_open "")
        endif()
        if(NOT _x STREQUAL "")
            list(APPEND _types "${_x}")
            list(APPEND "_type_homes_${_x}" "${_key}")
            continue()
        endif()
        if(_line MATCHES "^[ \t#{}]" OR _line MATCHES "^(typedef|static|struct)([^A-Za-z0-9_]|$)")
            continue()
        endif()
        if(_line MATCHES "(^|[^A-Za-z0-9_])clopts_([a-z0-9_]+)[ \t]*\\(")
            list(APPEND _functions "${CMAKE_MATCH_2}|${_number}|${_file}|${_key}")
        endif()
    endforeach()
endmacro()

file(GLOB_RECURSE _headers LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*.h")
list(SORT _headers)
foreach(_rel IN LISTS _headers)
    if(_rel MATCHES "^([^/]+)/([^/]+)_internal\\.h$" AND CMAKE_MATCH_1 STREQUAL CMAKE_MATCH_2)
        list(APPEND _offending
            "${SOURCE_DIR}/${_rel}:1: an internal header is named after a concept, not <module>_internal.h")
        continue()
    endif()
    if(NOT "${INCLUDE_DIR}" STREQUAL "")
        if(_rel MATCHES "^(.+)_internal\\.h$")
            if(NOT EXISTS "${INCLUDE_DIR}/clopts/${CMAKE_MATCH_1}.h")
                list(APPEND _offending
                    "${SOURCE_DIR}/${_rel}:1: _internal is only for the internal header of a public header, and clopts/${CMAKE_MATCH_1}.h does not exist (name it ${CMAKE_MATCH_1}.h)")
            endif()
        elseif(EXISTS "${INCLUDE_DIR}/clopts/${_rel}")
            string(REGEX REPLACE "\\.h$" "_internal.h" _internal_name "${_rel}")
            list(APPEND _offending
                "${SOURCE_DIR}/${_rel}:1: has the name of the public header clopts/${_rel} (name it ${_internal_name})")
        endif()
    endif()
    _check_names_scan("${SOURCE_DIR}" "${_rel}")
endforeach()

if(NOT "${INCLUDE_DIR}" STREQUAL "")
    file(GLOB_RECURSE _headers LIST_DIRECTORIES false RELATIVE "${INCLUDE_DIR}/clopts" "${INCLUDE_DIR}/clopts/*.h")
    list(SORT _headers)
    foreach(_rel IN LISTS _headers)
        _check_names_scan("${INCLUDE_DIR}/clopts" "${_rel}")
    endforeach()
endif()

list(REMOVE_DUPLICATES _types)
foreach(_item IN LISTS _functions)
    string(REPLACE "|" ";" _fields "${_item}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _line)
    list(GET _fields 2 _file)
    list(GET _fields 3 _key)

    # The struct the function is an operation of: the longest x with
    # _name == x or _name == x_<...>.
    set(_owner "")
    string(LENGTH "${_owner}" _owner_length)
    foreach(_x IN LISTS _types)
        string(LENGTH "${_x}" _length)
        string(FIND "${_name}_" "${_x}_" _at)
        if(_at EQUAL 0 AND _length GREATER _owner_length)
            set(_owner "${_x}")
            set(_owner_length ${_length})
        endif()
    endforeach()
    if(_owner STREQUAL "")
        continue()
    endif()

    list(FIND "_type_homes_${_owner}" "${_key}" _home)
    if(_home EQUAL -1)
        set(_homes "${_type_homes_${_owner}}")
        list(REMOVE_DUPLICATES _homes)
        list(TRANSFORM _homes APPEND ".h")
        list(JOIN _homes ", " _homes)
        list(APPEND _offending
            "${_file}:${_line}: 'clopts_${_name}' is an operation of clopts_${_owner}_t but is not declared with it (${_homes})")
    endif()
endforeach()

if(_offending)
    list(LENGTH _offending _count)
    list(JOIN _offending "\n  " _report)
    message(FATAL_ERROR
        "check_names: ${_count} violation(s) of the naming rules:\n  ${_report}\n"
        "The rules are described in docs/dev/design.md.")
endif()
message(STATUS "check_names: every operation of a struct is declared with its struct")
