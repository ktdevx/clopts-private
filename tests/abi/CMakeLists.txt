# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# ABI compatibility tests (label: abi).
#
#   abi.symbol_list            format of the public symbol list
#                              abi/clopts.symbols (check_symbols.cmake)
#   abi.exports                shared builds only: the symbols the shared
#                              library exports match the list
#                              (check_symbols.cmake with LIBRARY/FORMAT/TOOL)
#   abi.symbols_selftest.<format>
#                              the recorded dump fixtures/exports/<format>.txt
#                              matches fixtures/exports/expected.symbols
#                              (formats nm-elf, nm-macho, dumpbin, objdump-pe)
#   abi.symbols_selftest.mismatch
#                              a list with one name missing and one extra is
#                              reported with both names
#   abi.symbols_selftest.bad_list
#                              every format error of a list is reported with
#                              its line
#   abi.header_rules           the public headers under include/clopts have
#                              CLOPTS_API on every function
#                              (check_header_rules.cmake)
#   abi.header_rules_selftest.good / .<case>
#                              fixtures/header_rules/good passes; each
#                              fixtures/header_rules/bad/<case> reports the
#                              expected file, line and reason
#   abi.headers_c              every public header compiles on its own as C99
#                              (fixtures/headers through run_fixture.cmake)
#   abi.headers_cxx            the same as C++11, plus a C++ program that links
#                              every listed symbol; DISABLED when no C++
#                              compiler is found (CLOpts itself needs none)
#   abi.compare_selftest       run_abidiff.cmake MODE=self_test (baseline
#                              selection and abidiff exit code rules)
#
# Tests that expect a failure of the script use PASS_REGULAR_EXPRESSION, so
# that they pass only when the expected report is printed (a failure for any
# other reason does not match).

set(_clopts_abi_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(_clopts_abi_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set(_clopts_abi_symbols "${PROJECT_SOURCE_DIR}/abi/clopts.symbols")

# _clopts_abi_add_script_test(<name> <script> [ARGS <-Dvar=value>...]
#                             [PASS_REGEX <regex>])
#
# Registers abi.<name>, which runs tests/abi/<script> with `cmake -P`.
function(_clopts_abi_add_script_test name script)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "PASS_REGEX" "ARGS")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "_clopts_abi_add_script_test(${name}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    add_test(NAME "abi.${name}"
        COMMAND "${CMAKE_COMMAND}" ${ARG_ARGS} -P "${_clopts_abi_dir}/${script}")
    set_tests_properties("abi.${name}" PROPERTIES LABELS abi)
    if(DEFINED ARG_PASS_REGEX)
        set_tests_properties("abi.${name}" PROPERTIES PASS_REGULAR_EXPRESSION "${ARG_PASS_REGEX}")
    endif()
endfunction()

# ---- Public symbol list and exports ----------
_clopts_abi_add_script_test(symbol_list check_symbols.cmake
    ARGS "-DSYMBOLS_FILE=${_clopts_abi_symbols}")

# The export dump tool of this toolchain. A tool that is not
# found leaves TOOL empty: the test is still registered and check_symbols.cmake
# reports "export dump tool not found" when it runs.
if(MSVC)
    set(_clopts_abi_format dumpbin)
    set(_clopts_abi_tool "")
    if(CMAKE_LINKER)
        get_filename_component(_clopts_abi_linker_dir "${CMAKE_LINKER}" DIRECTORY)
        find_program(_clopts_abi_dumpbin NAMES dumpbin
            HINTS "${_clopts_abi_linker_dir}" NO_CACHE)
        if(_clopts_abi_dumpbin)
            set(_clopts_abi_tool "${_clopts_abi_dumpbin}")
        else()
            # check_symbols.cmake runs link.exe as `link /dump /exports`.
            get_filename_component(_clopts_abi_linker_name "${CMAKE_LINKER}" NAME)
            string(TOLOWER "${_clopts_abi_linker_name}" _clopts_abi_linker_name)
            if(_clopts_abi_linker_name MATCHES "^link(\\.exe)?$")
                set(_clopts_abi_tool "${CMAKE_LINKER}")
            endif()
        endif()
    endif()
elseif(MINGW)
    set(_clopts_abi_format objdump-pe)
    set(_clopts_abi_tool "${CMAKE_OBJDUMP}")
elseif(APPLE)
    set(_clopts_abi_format nm-macho)
    set(_clopts_abi_tool "${CMAKE_NM}")
else()
    set(_clopts_abi_format nm-elf)
    set(_clopts_abi_tool "${CMAKE_NM}")
endif()
if(NOT _clopts_abi_tool)
    # Also turns <tool>-NOTFOUND into an empty TOOL.
    set(_clopts_abi_tool "")
elseif(NOT IS_ABSOLUTE "${_clopts_abi_tool}")
    # TOOL is passed as a full path.
    find_program(_clopts_abi_tool_path NAMES "${_clopts_abi_tool}" NO_CACHE)
    if(_clopts_abi_tool_path)
        set(_clopts_abi_tool "${_clopts_abi_tool_path}")
    else()
        set(_clopts_abi_tool "")
    endif()
endif()

# Only the shared form exports symbols.
if(CLOPTS_BUILD_SHARED)
    _clopts_abi_add_script_test(exports check_symbols.cmake
        ARGS
            "-DSYMBOLS_FILE=${_clopts_abi_symbols}"
            "-DFORMAT=${_clopts_abi_format}"
            "-DLIBRARY=$<TARGET_FILE:clopts>"
            "-DTOOL=${_clopts_abi_tool}")
endif()

# ---- Self-tests of check_symbols.cmake (recorded dumps, every OS) --------
foreach(_clopts_abi_dump_format IN ITEMS nm-elf nm-macho dumpbin objdump-pe)
    _clopts_abi_add_script_test("symbols_selftest.${_clopts_abi_dump_format}" check_symbols.cmake
        ARGS
            "-DSYMBOLS_FILE=${_clopts_abi_fixtures}/exports/expected.symbols"
            "-DFORMAT=${_clopts_abi_dump_format}"
            "-DDUMP_FILE=${_clopts_abi_fixtures}/exports/${_clopts_abi_dump_format}.txt")
endforeach()

# mismatch.symbols lacks clopts_write_help (exported: extra) and adds
# clopts_result_take (not exported: missing). Both names, the list and the
# guidance must be reported.
_clopts_abi_add_script_test(symbols_selftest.mismatch check_symbols.cmake
    ARGS
        "-DSYMBOLS_FILE=${_clopts_abi_fixtures}/exports/mismatch.symbols"
        "-DFORMAT=nm-elf"
        "-DDUMP_FILE=${_clopts_abi_fixtures}/exports/nm-elf.txt"
    PASS_REGEX
        "abi symbol check failed \\(nm-elf, [^\n]*nm-elf\\.txt\\)[\r\n ]+missing \\(in the list but not exported\\):[\r\n ]+clopts_result_take[\r\n ]+extra \\(exported but not in the list\\):[\r\n ]+clopts_write_help[\r\n ]+The public symbol list is [^\n]*mismatch\\.symbols\\.[\r\n ]+If the change is intended, update the list")

# bad_list.symbols: a duplicate, two invalid names and two invalid platform
# tags, each reported with its line (comments and blank lines are not).
_clopts_abi_add_script_test(symbols_selftest.bad_list check_symbols.cmake
    ARGS
        "-DSYMBOLS_FILE=${_clopts_abi_fixtures}/exports/bad_list.symbols"
    PASS_REGEX
        "5 error\\(s\\) in the public symbol list [^\n]*bad_list\\.symbols:[\r\n ]+[^\n]*bad_list\\.symbols:9: duplicate symbol 'clopts_program_create'[\r\n ]+[^\n]*bad_list\\.symbols:10: invalid symbol name 'parse_args'[\r\n ]+[^\n]*bad_list\\.symbols:11: invalid symbol name 'Clopts_Parse'[\r\n ]+[^\n]*bad_list\\.symbols:12: invalid platform tag 'linux'[\r\n ]+[^\n]*bad_list\\.symbols:13: invalid platform tag 'windows extra'")

# ---- Public header rules --------------------------
_clopts_abi_add_script_test(header_rules check_header_rules.cmake
    ARGS "-DHEADER_DIR=${PROJECT_SOURCE_DIR}/include/clopts")

_clopts_abi_add_script_test(header_rules_selftest.good check_header_rules.cmake
    ARGS "-DHEADER_DIR=${_clopts_abi_fixtures}/header_rules/good")

# Each bad case must report exactly its one violation: file, line and reason.
set(_clopts_abi_bad_missing_api
    "missing_api\\.h:10: function 'bad_unmarked' lacks CLOPTS_API")
set(_clopts_abi_bad_missing_api_multiline
    "missing_api_multiline\\.h:12: function 'bad_unmarked_multiline' lacks CLOPTS_API")
foreach(_clopts_abi_case IN ITEMS missing_api missing_api_multiline)
    _clopts_abi_add_script_test("header_rules_selftest.${_clopts_abi_case}" check_header_rules.cmake
        ARGS "-DHEADER_DIR=${_clopts_abi_fixtures}/header_rules/bad/${_clopts_abi_case}"
        PASS_REGEX "1 violation\\(s\\)[^\n]*[\r\n ]+[^\n]*/${_clopts_abi_bad_${_clopts_abi_case}}")
endforeach()

# ---- Header compilation as C99 / C++11 ------------
# The child project is driven like tests/toolchain/: the generator, platform,
# C compiler and configuration of this build tree.
get_property(_clopts_abi_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_clopts_abi_multi_config)
    set(_clopts_abi_config "$<CONFIG>")
else()
    set(_clopts_abi_config "${CMAKE_BUILD_TYPE}")
endif()

# The form of this build tree is passed on, so that a shared build tree links
# the C++ check against the shared library.
if(CLOPTS_BUILD_SHARED)
    set(_clopts_abi_shared ON)
else()
    set(_clopts_abi_shared OFF)
endif()

# _clopts_abi_add_headers_test(<lang> <name> [EXTRA_ARGS <arg>...])
function(_clopts_abi_add_headers_test lang name)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "EXTRA_ARGS")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "_clopts_abi_add_headers_test(${lang}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    set(extra_args
        "-DHEADER_CHECK_LANG=${lang}"
        "-DSYMBOLS_FILE=${_clopts_abi_symbols}"
        "-DCLOPTS_BUILD_SHARED=${_clopts_abi_shared}"
        "-DCLOPTS_WERROR=ON"
        ${ARG_EXTRA_ARGS})
    add_test(NAME "abi.${name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DFIXTURE_SOURCE=${_clopts_abi_fixtures}/headers"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/${name}"
            "-DEXPECT=build_ok"
            "-DCONFIG=${_clopts_abi_config}"
            "-DCLOPTS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DC_COMPILER=${CMAKE_C_COMPILER}"
            "-DEXTRA_ARGS=${extra_args}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/run_fixture.cmake"
    )
    set_tests_properties("abi.${name}" PROPERTIES LABELS abi)
endfunction()

_clopts_abi_add_headers_test(C headers_c)

# C++ is optional: it is detected here without
# enabling it, so CLOpts' project() stays C only. check_language(CXX) keeps a
# CMAKE_CXX_COMPILER that is already set (including <...>-NOTFOUND).
include(CheckLanguage)
check_language(CXX)
set(_clopts_abi_cxx_args "")
if(CMAKE_CXX_COMPILER AND NOT CMAKE_GENERATOR MATCHES "^Visual Studio")
    # Visual Studio generators pick the compiler from the platform/toolset.
    set(_clopts_abi_cxx_args "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
endif()
_clopts_abi_add_headers_test(CXX headers_cxx EXTRA_ARGS ${_clopts_abi_cxx_args})
if(NOT CMAKE_CXX_COMPILER)
    message(STATUS "CLOpts: no C++ compiler found; abi.headers_cxx is disabled")
    set_tests_properties(abi.headers_cxx PROPERTIES DISABLED TRUE)
endif()

# ---- Self-test of the abidiff comparison script ---
_clopts_abi_add_script_test(compare_selftest run_abidiff.cmake
    ARGS "-DMODE=self_test")
