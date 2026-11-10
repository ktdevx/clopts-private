# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

# Module boundaries of src/ (check_structure.cmake): the dependencies between
# modules of docs/dev/architecture.md and the OS boundary (only process
# touches the OS).
set(_clopts_structure_script "${CMAKE_CURRENT_SOURCE_DIR}/check_structure.cmake")
set(_clopts_structure_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# _clopts_structure_add_test(<name> <source_dir> [PASS_REGEX <regex>])
#
# Registers structure.<name>, which runs check_structure.cmake on <source_dir>.
function(_clopts_structure_add_test name source_dir)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "PASS_REGEX" "")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "_clopts_structure_add_test(${name}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    add_test(NAME "structure.${name}"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${source_dir}" -P "${_clopts_structure_script}")
    set_tests_properties("structure.${name}" PROPERTIES LABELS structure)
    if(DEFINED ARG_PASS_REGEX)
        set_tests_properties("structure.${name}" PROPERTIES PASS_REGULAR_EXPRESSION "${ARG_PASS_REGEX}")
    endif()
endfunction()

_clopts_structure_add_test(modules "${PROJECT_SOURCE_DIR}/src")

_clopts_structure_add_test(selftest.good "${_clopts_structure_fixtures}/good")

# Each bad case must report exactly its one violation: file, line and reason.
set(_clopts_structure_bad_text_reads_decl
    "text/layout\\.c:3: text may not include 'decl/'")
set(_clopts_structure_bad_scan_reads_process
    "parser/scan\\.c:3: parser may not include 'process/'")
set(_clopts_structure_bad_base_reads_decl
    "base/vec\\.c:2: base may not include 'decl/'")
set(_clopts_structure_bad_getenv_outside_process
    "decl/names\\.c:6: call of getenv outside process")
set(_clopts_structure_bad_windows_h_outside_process
    "text/out\\.c:2: #include <windows\\.h> outside process")
set(_clopts_structure_bad_windows_h_outside_win32
    "process/env\\.c:2: #include <windows\\.h> outside process/\\*_win32\\.c")
set(_clopts_structure_bad_unknown_module
    "extra/extra\\.c:1: module 'extra' has no dependency rule")
foreach(_clopts_structure_case IN ITEMS
        text_reads_decl scan_reads_process base_reads_decl
        getenv_outside_process windows_h_outside_process windows_h_outside_win32 unknown_module)
    _clopts_structure_add_test("selftest.${_clopts_structure_case}"
        "${_clopts_structure_fixtures}/bad/${_clopts_structure_case}"
        PASS_REGEX "1 violation\\(s\\)[^\n]*[\r\n ]+[^\n]*/${_clopts_structure_bad_${_clopts_structure_case}}")
endforeach()
