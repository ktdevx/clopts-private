# Driver for integration.package_selection. Run with `cmake -P`: builds the
# static and the shared form of CLOpts from the source tree, installs them side
# by side, and checks how find_package(CLOpts CONFIG ...) selects between them
# (requirements 6.7, 12.6, 13.2 to 13.5). Exit code 0 means "as expected".
#
# Required variables (-D):
#   CLOPTS_SOURCE_DIR  CLOpts source tree (built twice, and the selection
#                      fixture tests/integration/fixtures/package_select).
#   WORK_DIR           Scratch directory, deleted and recreated on every run:
#                      <WORK_DIR>/build_static, <WORK_DIR>/build_shared (CLOpts
#                      build trees), <WORK_DIR>/prefix_both (both forms),
#                      <WORK_DIR>/prefix_static (static form only),
#                      <WORK_DIR>/cases/<case> (fixture build trees).
#   GENERATOR          Generator of the parent build (${CMAKE_GENERATOR}).
#
# Optional variables (-D):
#   CONFIG             Configuration ($<CONFIG>). When non-empty it is passed
#                      as CMAKE_BUILD_TYPE (single-config generators) and as
#                      --config to build/install (multi-config generators).
#   C_COMPILER         C compiler of the parent build (${CMAKE_C_COMPILER}).
#                      Not forwarded to Visual Studio generators.
#   GENERATOR_PLATFORM ${CMAKE_GENERATOR_PLATFORM} of the parent (MSVC -A).
#
# Both forms are always built here, whatever the form of the parent build
# tree, with tests off and install rules on. The fixture is only configured
# (never built), once per case below. Every case runs; afterwards each failing
# case is reported with its name and the output of the child configure step.

cmake_minimum_required(VERSION 3.21)

# ---- Input validation ----------------------------------------------------
foreach(var IN ITEMS CLOPTS_SOURCE_DIR WORK_DIR GENERATOR)
    if("${${var}}" STREQUAL "")
        message(FATAL_ERROR "run_package_selection: required variable ${var} is not set (see tests/cmake/run_package_selection.cmake)")
    endif()
endforeach()

set(fixture_source "${CLOPTS_SOURCE_DIR}/tests/integration/fixtures/package_select")
if(NOT EXISTS "${fixture_source}/CMakeLists.txt")
    message(FATAL_ERROR "run_package_selection: no CMakeLists.txt in ${fixture_source}")
endif()

# ---- Helpers -------------------------------------------------------------
function(_selection_fail what result output)
    message("---- ${what}: exit status ${result} ----")
    message("${output}")
    message("---- end of output ----")
    message(FATAL_ERROR "run_package_selection: ${what}")
endfunction()

# Runs a child command and stores its exit status and combined output.
function(_selection_run out_result out_output)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_output} "${output}" PARENT_SCOPE)
endfunction()

# Runs a child command and stops with its output unless it succeeds.
function(_selection_run_ok what)
    _selection_run(result output ${ARGN})
    if(NOT result EQUAL 0)
        _selection_fail("${what} failed" "${result}" "${output}")
    endif()
endfunction()

# ---- Fresh work directory ------------------------------------------------
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(prefix_both "${WORK_DIR}/prefix_both")
set(prefix_static "${WORK_DIR}/prefix_static")

# Arguments shared by every child configure step (CLOpts and the fixture).
set(common_configure_args -G "${GENERATOR}" --no-warn-unused-cli)
if(NOT "${GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND common_configure_args -A "${GENERATOR_PLATFORM}")
endif()
if(NOT "${C_COMPILER}" STREQUAL "" AND NOT GENERATOR MATCHES "^Visual Studio")
    list(APPEND common_configure_args "-DCMAKE_C_COMPILER=${C_COMPILER}")
endif()
if(NOT "${CONFIG}" STREQUAL "")
    list(APPEND common_configure_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
endif()

set(config_args "")
if(NOT "${CONFIG}" STREQUAL "")
    set(config_args --config "${CONFIG}")
endif()

# ---- Build and install both forms ----------------------------------------
# _selection_build_form(<form> <ON|OFF> <prefix>...)
function(_selection_build_form form shared)
    set(build_dir "${WORK_DIR}/build_${form}")
    _selection_run_ok("configure of the ${form} CLOpts"
        "${CMAKE_COMMAND}" -S "${CLOPTS_SOURCE_DIR}" -B "${build_dir}"
        ${common_configure_args}
        "-DCLOPTS_BUILD_SHARED=${shared}"
        -DCLOPTS_BUILD_TESTING=OFF
        -DCLOPTS_INSTALL=ON)
    _selection_run_ok("build of the ${form} CLOpts"
        "${CMAKE_COMMAND}" --build "${build_dir}" ${config_args})
    foreach(prefix IN LISTS ARGN)
        _selection_run_ok("install of the ${form} CLOpts to ${prefix}"
            "${CMAKE_COMMAND}" --install "${build_dir}" --prefix "${prefix}" ${config_args})
    endforeach()
endfunction()

_selection_build_form(static OFF "${prefix_both}" "${prefix_static}")
_selection_build_form(shared ON "${prefix_both}")

# ---- Side-by-side install ------------------------------------------------
# Both target files under the one package directory.
foreach(kind IN ITEMS Static Shared)
    file(GLOB_RECURSE found LIST_DIRECTORIES false "${prefix_both}/*/CLOpts${kind}Targets.cmake")
    if(found STREQUAL "")
        message(FATAL_ERROR "run_package_selection: CLOpts${kind}Targets.cmake is missing in ${prefix_both}")
    endif()
endforeach()

# MSVC-style toolchains (.lib for both the static library and the import
# library): the static library must not overwrite the import library (12.6).
file(GLOB_RECURSE static_libs LIST_DIRECTORIES false "${prefix_static}/*.lib")
if(NOT static_libs STREQUAL "")
    file(GLOB_RECURSE both_libs LIST_DIRECTORIES false RELATIVE "${prefix_both}" "${prefix_both}/*.lib")
    foreach(name IN ITEMS clopts_static.lib clopts.lib)
        set(present FALSE)
        foreach(lib IN LISTS both_libs)
            get_filename_component(lib_name "${lib}" NAME)
            if(lib_name STREQUAL name)
                set(present TRUE)
            endif()
        endforeach()
        if(NOT present)
            message(FATAL_ERROR
                "run_package_selection: ${name} is missing in ${prefix_both}; the static "
                "library and the import library must be installed side by side "
                "(found: ${both_libs})")
        endif()
    endforeach()
    message(STATUS "run_package_selection: clopts_static.lib and clopts.lib installed side by side")
else()
    message(STATUS "run_package_selection: no .lib files (not an MSVC-style toolchain); side-by-side .lib check skipped")
endif()

# ---- Selection cases -----------------------------------------------------
# _selection_case(<name> PREFIX both|static [SELECT <arg>...]
#                 [DEFINE <var>=<value>...]
#                 EXPECT STATIC_LIBRARY|SHARED_LIBRARY|FAIL [FAIL_REGEX <regex>])
#
# Configures the fixture against prefix_<PREFIX> with SELECT_ARGS=<SELECT> and
# -D<DEFINE>. STATIC_LIBRARY / SHARED_LIBRARY: the configure step must succeed
# (the fixture checks CLOpts_LIBRARY_TYPE and the TYPE of CLOpts::clopts).
# FAIL: it must fail, with output matching FAIL_REGEX (a space in it matches
# any whitespace, line breaks included). Appends <name> to
# failed_cases in the caller on a mismatch.
function(_selection_case name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "PREFIX;EXPECT;FAIL_REGEX" "SELECT;DEFINE")
    if(NOT arg_EXPECT MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|FAIL)$"
       OR NOT arg_PREFIX MATCHES "^(both|static)$"
       OR (arg_EXPECT STREQUAL "FAIL" AND "${arg_FAIL_REGEX}" STREQUAL ""))
        message(FATAL_ERROR "run_package_selection: bad definition of case ${name}")
    endif()

    set(args
        -S "${fixture_source}"
        -B "${WORK_DIR}/cases/${name}"
        ${common_configure_args}
        "-DCMAKE_PREFIX_PATH=${prefix_${arg_PREFIX}}")
    foreach(define IN LISTS arg_DEFINE)
        list(APPEND args "-D${define}")
    endforeach()
    if(NOT arg_EXPECT STREQUAL "FAIL")
        list(APPEND args "-DEXPECTED_TYPE=${arg_EXPECT}")
    endif()
    # Called directly (not through _selection_run, whose ARGN would split it)
    # and quoted, so that the ;-list SELECT_ARGS stays one argument.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${args} "-DSELECT_ARGS=${arg_SELECT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )

    set(problem "")
    if(arg_EXPECT STREQUAL "FAIL")
        if(result EQUAL 0)
            set(problem "configure succeeded but find_package was expected to fail")
        else()
            # CMake word-wraps the reason given by the package, so a space in
            # FAIL_REGEX matches any run of whitespace (line breaks included).
            string(REPLACE " " "[ \t\r\n]+" fail_regex "${arg_FAIL_REGEX}")
            if(NOT output MATCHES "${fail_regex}")
                set(problem "configure failed, but the output does not match '${arg_FAIL_REGEX}'")
            endif()
        endif()
    elseif(NOT result EQUAL 0)
        set(problem "configure failed; expected CLOpts::clopts of type ${arg_EXPECT}")
    endif()

    if(problem STREQUAL "")
        message(STATUS "run_package_selection: case ${name}: ok (${arg_EXPECT})")
        return()
    endif()
    message("---- case ${name} FAILED: ${problem} (prefix_${arg_PREFIX}, "
        "SELECT_ARGS '${arg_SELECT}', DEFINE '${arg_DEFINE}'; exit status ${result}) ----")
    message("${output}")
    message("---- end of output of case ${name} ----")
    set(failed_cases ${failed_cases} "${name}" PARENT_SCOPE)
endfunction()

set(failed_cases "")

_selection_case(static_component    PREFIX both   SELECT COMPONENTS static EXPECT STATIC_LIBRARY)
_selection_case(shared_component    PREFIX both   SELECT COMPONENTS shared EXPECT SHARED_LIBRARY)
_selection_case(prefer_clopts_var   PREFIX both   DEFINE CLOPTS_SHARED_LIBS=ON EXPECT SHARED_LIBRARY)
_selection_case(prefer_build_shared PREFIX both   DEFINE BUILD_SHARED_LIBS=ON EXPECT SHARED_LIBRARY)
_selection_case(default_static      PREFIX both   EXPECT STATIC_LIBRARY)
_selection_case(both_components     PREFIX both   SELECT COMPONENTS static shared EXPECT FAIL
    FAIL_REGEX "components 'static' and 'shared' cannot be used together")
_selection_case(missing_shared      PREFIX static SELECT COMPONENTS shared EXPECT FAIL
    FAIL_REGEX "the shared form \\(component 'shared'\\) is not installed")
_selection_case(fallback            PREFIX static DEFINE CLOPTS_SHARED_LIBS=ON EXPECT STATIC_LIBRARY)

if(NOT failed_cases STREQUAL "")
    message(FATAL_ERROR "run_package_selection: failed case(s): ${failed_cases}")
endif()
message(STATUS "run_package_selection: all cases passed")
