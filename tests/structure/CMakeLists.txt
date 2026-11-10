# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

# Module boundaries of src/ and include/clopts/ (check_structure.cmake): the
# dependencies between modules of docs/dev/architecture.md and the OS
# boundary (only process touches the OS). Names of src/ and include/clopts/
# (check_names.cmake): one concept has one header, one source file and one
# prefix (docs/dev/design.md).
set(_clopts_structure_script "${CMAKE_CURRENT_SOURCE_DIR}/check_structure.cmake")
set(_clopts_structure_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# _clopts_structure_add_test(<name> <source_dir> [INCLUDE_DIR <dir>] [SCRIPT <file>]
#                            [PASS_REGEX <regex>])
#
# Registers structure.<name>, which runs SCRIPT (default check_structure.cmake)
# on <source_dir> and, when given, INCLUDE_DIR.
function(_clopts_structure_add_test name source_dir)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "PASS_REGEX;INCLUDE_DIR;SCRIPT" "")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "_clopts_structure_add_test(${name}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(NOT ARG_SCRIPT)
        set(ARG_SCRIPT "${_clopts_structure_script}")
    endif()
    set(include_arg "")
    if(ARG_INCLUDE_DIR)
        set(include_arg "-DINCLUDE_DIR=${ARG_INCLUDE_DIR}")
    endif()
    add_test(NAME "structure.${name}"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${source_dir}" ${include_arg} -P "${ARG_SCRIPT}")
    set_tests_properties("structure.${name}" PROPERTIES LABELS structure)
    if(DEFINED ARG_PASS_REGEX)
        set_tests_properties("structure.${name}" PROPERTIES PASS_REGULAR_EXPRESSION "${ARG_PASS_REGEX}")
    endif()
endfunction()

_clopts_structure_add_test(modules "${PROJECT_SOURCE_DIR}/src" INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")

_clopts_structure_add_test(selftest.good "${_clopts_structure_fixtures}/good")

# Each bad case must report exactly its one violation: file, line and reason.
set(_clopts_structure_bad_text_reads_decl
    "text/layout\\.c:3: text may not include 'decl/'")
set(_clopts_structure_bad_decl_reads_parse
    "decl/scope\\.c:2: decl may not include 'parse/'")
set(_clopts_structure_bad_base_reads_decl
    "base/vec\\.c:2: base may not include 'decl/'")
set(_clopts_structure_bad_getenv_outside_process
    "decl/name\\.c:6: call of getenv outside process")
set(_clopts_structure_bad_windows_h_outside_process
    "text/out\\.c:2: #include <windows\\.h> outside process")
set(_clopts_structure_bad_windows_h_outside_win32
    "process/env\\.c:2: #include <windows\\.h> outside process/\\*_win32\\.c")
set(_clopts_structure_bad_text_reads_process_api
    "text/out\\.c:2: text may not include 'clopts/process/'")
set(_clopts_structure_bad_unknown_module
    "extra/extra\\.c:1: module 'extra' has no dependency rule")
foreach(_clopts_structure_case IN ITEMS
        text_reads_decl decl_reads_parse base_reads_decl
        getenv_outside_process windows_h_outside_process windows_h_outside_win32 text_reads_process_api
        unknown_module)
    _clopts_structure_add_test("selftest.${_clopts_structure_case}"
        "${_clopts_structure_fixtures}/bad/${_clopts_structure_case}"
        PASS_REGEX "1 violation\\(s\\)[^\n]*[\r\n ]+[^\n]*/${_clopts_structure_bad_${_clopts_structure_case}}")
endforeach()

# The public headers: the good src/ fixture with a public tree each.
set(_clopts_structure_public "${_clopts_structure_fixtures}/public")
_clopts_structure_add_test(selftest.public_good "${_clopts_structure_fixtures}/good"
    INCLUDE_DIR "${_clopts_structure_public}/good")
set(_clopts_structure_public_decl_reads_parse
    "clopts/decl/program\\.h:2: decl may not include 'clopts/parse/'")
set(_clopts_structure_public_public_reads_internal
    "clopts/decl/scope\\.h:2: a public header may not include 'decl/'")
set(_clopts_structure_public_outside_module
    "clopts/types\\.h:1: not in a module directory")
foreach(_clopts_structure_case IN ITEMS decl_reads_parse public_reads_internal outside_module)
    _clopts_structure_add_test("selftest.public_${_clopts_structure_case}" "${_clopts_structure_fixtures}/good"
        INCLUDE_DIR "${_clopts_structure_public}/bad/${_clopts_structure_case}"
        PASS_REGEX "1 violation\\(s\\)[^\n]*[\r\n ]+[^\n]*/${_clopts_structure_public_${_clopts_structure_case}}")
endforeach()

# ---- names (check_names.cmake) ----------------------------------------------
set(_clopts_structure_names_script "${CMAKE_CURRENT_SOURCE_DIR}/check_names.cmake")
set(_clopts_structure_names "${_clopts_structure_fixtures}/names")

_clopts_structure_add_test(names "${PROJECT_SOURCE_DIR}/src" INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include"
    SCRIPT "${_clopts_structure_names_script}")
_clopts_structure_add_test(names_selftest.good "${_clopts_structure_names}/good/src"
    INCLUDE_DIR "${_clopts_structure_names}/good/include" SCRIPT "${_clopts_structure_names_script}")

# Each bad case must report exactly its one violation.
set(_clopts_structure_names_internal_header
    "base/base_internal\\.h:1: an internal header is named after a concept")
set(_clopts_structure_names_wrong_prefix
    "base/size\\.h:2: 'clopts_mem_size' is not named after its header")
set(_clopts_structure_names_defined_elsewhere
    "base/size\\.h:2: 'clopts_size' is not defined in size\\.c or size_\\*\\.c")
set(_clopts_structure_names_public_defined_elsewhere
    "clopts/base/allocator\\.h:3: 'clopts_set_allocator' is not defined in src/base/allocator\\.c")
set(_clopts_structure_names_undeclared
    "base/size\\.c:6: 'clopts_size_left_over' is not declared in base/size\\.h")
foreach(_clopts_structure_case IN ITEMS internal_header wrong_prefix undeclared)
    _clopts_structure_add_test("names_selftest.${_clopts_structure_case}"
        "${_clopts_structure_names}/bad/${_clopts_structure_case}/src" SCRIPT "${_clopts_structure_names_script}"
        PASS_REGEX "1 violation\\(s\\)[^\n]*[\r\n ]+[^\n]*/${_clopts_structure_names_${_clopts_structure_case}}")
endforeach()
# A function defined in the wrong file breaks both directions: it is not
# defined next to its declaration, and its definition is not declared next
# to it. Both are reported.
_clopts_structure_add_test("names_selftest.defined_elsewhere"
    "${_clopts_structure_names}/bad/defined_elsewhere/src" SCRIPT "${_clopts_structure_names_script}"
    PASS_REGEX "2 violation\\(s\\)[^\n]*[\r\n ]+[^\n]*/${_clopts_structure_names_defined_elsewhere}")
_clopts_structure_add_test("names_selftest.public_defined_elsewhere"
    "${_clopts_structure_names}/bad/public_defined_elsewhere/src"
    INCLUDE_DIR "${_clopts_structure_names}/bad/public_defined_elsewhere/include"
    SCRIPT "${_clopts_structure_names_script}"
    PASS_REGEX "2 violation\\(s\\)[^\n]*[\r\n ]+[^\n]*/${_clopts_structure_names_public_defined_elsewhere}")
