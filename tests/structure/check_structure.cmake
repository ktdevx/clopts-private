# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Checks the module boundaries of src/ (docs/dev/architecture.md). Run with
# `cmake -P`; registered as structure.modules (label: structure).
#
# Required variables (-D):
#   SOURCE_DIR  Directory to scan (the src/ directory of CLOpts).
#
# Every .c / .h file must be in a module directory (SOURCE_DIR/<module>/).
# After comments are removed, each file is checked for:
#   - dependencies between modules: an #include "<other>/..." of another
#     module directory is allowed only when the rules below list <other> for
#     the module, or for the file itself. #include "clopts/..." (the public
#     headers) and includes of the file's own module are always allowed.
#   - the OS boundary: outside the process module, no #include <windows.h>
#     and no call of getenv / _wgetenv / secure_getenv /
#     GetEnvironmentVariable*.
# A module directory that the rules do not list is an error, so a new module
# has to be given its rule. Exits with an error that lists every offending
# <file>:<line> when something is found.

if("${SOURCE_DIR}" STREQUAL "" OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "check_structure: SOURCE_DIR is not a directory ('${SOURCE_DIR}')")
endif()

# ---- Rules -----------------------------------------------------------------
# <module>:<modules it may include, comma-separated>
set(_module_rules
    "base:"
    "decl:base"
    "parser:base,decl"
    "process:base"
    "text:base"
)
# <module>/<file>:<further modules that file may include>
set(_file_rules
    # The parse entry wires the process environment lookup (parse.c).
    "parser/parse.c:process"
)
# The only module that may touch the OS.
set(_os_module "process")

foreach(_rule IN LISTS _module_rules)
    string(REGEX MATCH "^([a-z0-9_]+):(.*)$" _ignored "${_rule}")
    string(REPLACE "," ";" _deps "${CMAKE_MATCH_2}")
    set(_allowed_${CMAKE_MATCH_1} ${_deps})
    set(_known_${CMAKE_MATCH_1} TRUE)
endforeach()
foreach(_rule IN LISTS _file_rules)
    string(REGEX MATCH "^([^:]+):(.*)$" _ignored "${_rule}")
    string(REPLACE "," ";" _deps "${CMAKE_MATCH_2}")
    string(MAKE_C_IDENTIFIER "${CMAKE_MATCH_1}" _key)
    set(_file_allowed_${_key} ${_deps})
endforeach()

# Characters that cannot appear in the scanned text stand in for ";", "[" and
# "]" while the file is handled as a CMake list of lines.
string(ASCII 1 _semicolon)
string(ASCII 2 _open_bracket)
string(ASCII 3 _close_bracket)

# _check_structure_lines(<out_var> <file>)
#
# Sets <out_var> to the lines of <file> as a list (empty lines kept, so that
# the position in the list is the line number minus one), with ";", "[" and
# "]" replaced by the stand-ins above.
function(_check_structure_lines out_var file)
    file(READ "${file}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE ";" "${_semicolon}" content "${content}")
    string(REPLACE "[" "${_open_bracket}" content "${content}")
    string(REPLACE "]" "${_close_bracket}" content "${content}")
    string(REPLACE "\n" ";" content "${content}")
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

set(_offending "")

file(GLOB_RECURSE _files LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*.c" "${SOURCE_DIR}/*.h")
list(SORT _files)
foreach(_rel IN LISTS _files)
    set(_file "${SOURCE_DIR}/${_rel}")
    if(NOT _rel MATCHES "^([^/]+)/")
        list(APPEND _offending "${_file}:1: not in a module directory")
        continue()
    endif()
    set(_module "${CMAKE_MATCH_1}")
    if(NOT _known_${_module})
        list(APPEND _offending "${_file}:1: module '${_module}' has no dependency rule (see check_structure.cmake)")
        continue()
    endif()
    string(MAKE_C_IDENTIFIER "${_rel}" _key)
    set(_allowed ${_allowed_${_module}} ${_file_allowed_${_key}})

    _check_structure_lines(_lines "${_file}")
    set(_line_number 0)
    set(_in_block_comment FALSE)
    foreach(_line IN LISTS _lines)
        math(EXPR _line_number "${_line_number} + 1")
        # Strip /* */ (also across lines) and // comments.
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

        # Dependencies between modules.
        if(_code MATCHES "^[ \t]*#[ \t]*include[ \t]*\"([^\"/]+)/[^\"]*\"")
            set(_target "${CMAKE_MATCH_1}")
            if(NOT _target STREQUAL "clopts" AND NOT _target STREQUAL _module)
                list(FIND _allowed "${_target}" _found)
                if(_found EQUAL -1)
                    list(APPEND _offending "${_file}:${_line_number}: ${_module} may not include '${_target}/'")
                endif()
            endif()
        endif()

        # The OS boundary.
        if(NOT _module STREQUAL _os_module)
            string(TOLOWER "${_code}" _lower)
            if(_lower MATCHES "^[ \t]*#[ \t]*include[ \t]*<windows\\.h>")
                list(APPEND _offending "${_file}:${_line_number}: #include <windows.h> outside ${_os_module}")
            elseif(_lower MATCHES "(^|[^a-z0-9_])(_w|secure_)?getenv[ \t]*\\(")
                list(APPEND _offending "${_file}:${_line_number}: call of getenv outside ${_os_module}")
            elseif(_lower MATCHES "(^|[^a-z0-9_])getenvironmentvariable[a-z]*[ \t]*\\(")
                list(APPEND _offending "${_file}:${_line_number}: call of GetEnvironmentVariable outside ${_os_module}")
            endif()
        endif()
    endforeach()
endforeach()

if(_offending)
    list(LENGTH _offending _count)
    list(JOIN _offending "\n  " _report)
    message(FATAL_ERROR
        "check_structure: ${_count} violation(s) of the module boundaries:\n  ${_report}\n"
        "The boundaries are described in docs/dev/architecture.md.")
endif()
message(STATUS "check_structure: the module boundaries hold under ${SOURCE_DIR}")
