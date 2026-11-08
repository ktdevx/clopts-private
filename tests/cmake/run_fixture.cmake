# Shared driver for the build-configuration tests (tests/toolchain/,
# tests/integration/). Run with `cmake -P`: configures, builds, optionally
# installs CLOpts first and runs the fixture, then checks the outcome against
# the expectation. Exit code 0 means "as expected".
#
# Required variables (-D):
#   FIXTURE_SOURCE     Source directory of the fixture CMake project.
#   WORK_DIR           Scratch directory for this test. Deleted and recreated on
#                      every run: <WORK_DIR>/build (fixture build tree),
#                      <WORK_DIR>/bin (FIXTURE_BIN_DIR), <WORK_DIR>/prefix
#                      (install prefix in install_find_package mode).
#   EXPECT             configure_ok | build_ok | build_fail | run_ok
#   CLOPTS_SOURCE_DIR  CLOpts source tree, forwarded to the fixture.
#   GENERATOR          Generator of the parent build (${CMAKE_GENERATOR}).
#
# Optional variables (-D):
#   CONFIG             Configuration ($<CONFIG>). When non-empty it is passed
#                      as CMAKE_BUILD_TYPE (single-config generators) and as
#                      --config to build/install (multi-config generators).
#   C_COMPILER         C compiler of the parent build (${CMAKE_C_COMPILER}).
#                      Not forwarded to Visual Studio generators, which pick
#                      the compiler from the platform/toolset.
#   GENERATOR_PLATFORM ${CMAKE_GENERATOR_PLATFORM} of the parent (MSVC -A).
#   CLOPTS_BINARY_DIR  CLOpts build tree; required in install_find_package mode.
#   EXTRA_ARGS         ;-separated extra arguments for the fixture configure
#                      step (e.g. -DCLOPTS_WERROR=ON).
#   MODE               "" or SUBDIRECTORY (default: nothing extra), or
#                      install_find_package: install CLOPTS_BINARY_DIR to
#                      <WORK_DIR>/prefix first and pass it as CMAKE_PREFIX_PATH.
#
# The fixture receives -DCLOPTS_SOURCE_DIR=..., -DFIXTURE_BIN_DIR=<WORK_DIR>/bin
# (where run_ok fixtures must put their single executable via
# RUNTIME_OUTPUT_DIRECTORY) and -DCLOPTS_BUILD_TESTING=OFF, so that pulling in
# CLOpts never fetches Naos again in the child build.

cmake_minimum_required(VERSION 3.21)

function(_fixture_fail what result output)
    message("---- ${what}: exit status ${result} ----")
    message("${output}")
    message("---- end of output ----")
    message(FATAL_ERROR "run_fixture: ${what} (EXPECT=${EXPECT}, fixture ${FIXTURE_SOURCE})")
endfunction()

# Runs a child command and stores its exit status and combined output.
function(_fixture_run out_result out_output)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_output} "${output}" PARENT_SCOPE)
endfunction()

# ---- Input validation ----------------------------------------------------
foreach(var IN ITEMS FIXTURE_SOURCE WORK_DIR EXPECT CLOPTS_SOURCE_DIR GENERATOR)
    if("${${var}}" STREQUAL "")
        message(FATAL_ERROR "run_fixture: required variable ${var} is not set (see tests/cmake/run_fixture.cmake)")
    endif()
endforeach()

if(NOT EXPECT MATCHES "^(configure_ok|build_ok|build_fail|run_ok)$")
    message(FATAL_ERROR "run_fixture: EXPECT must be configure_ok, build_ok, build_fail or run_ok (got '${EXPECT}')")
endif()

if(NOT "${MODE}" MATCHES "^(|SUBDIRECTORY|install_find_package)$")
    message(FATAL_ERROR "run_fixture: MODE must be empty, SUBDIRECTORY or install_find_package (got '${MODE}')")
endif()

if(NOT EXISTS "${FIXTURE_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR "run_fixture: no CMakeLists.txt in FIXTURE_SOURCE '${FIXTURE_SOURCE}'")
endif()

if(MODE STREQUAL "install_find_package" AND "${CLOPTS_BINARY_DIR}" STREQUAL "")
    message(FATAL_ERROR "run_fixture: MODE=install_find_package requires CLOPTS_BINARY_DIR")
endif()

# ---- Fresh work directory ------------------------------------------------
set(build_dir "${WORK_DIR}/build")
set(bin_dir "${WORK_DIR}/bin")
set(prefix_dir "${WORK_DIR}/prefix")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(config_args "")
if(NOT "${CONFIG}" STREQUAL "")
    set(config_args --config "${CONFIG}")
endif()

# ---- Install CLOpts first (install_find_package) -------------------------
if(MODE STREQUAL "install_find_package")
    _fixture_run(result output
        "${CMAKE_COMMAND}" --install "${CLOPTS_BINARY_DIR}" --prefix "${prefix_dir}" ${config_args})
    if(NOT result EQUAL 0)
        _fixture_fail("install of CLOpts from ${CLOPTS_BINARY_DIR} failed" "${result}" "${output}")
    endif()
endif()

# ---- Configure -----------------------------------------------------------
set(configure_args
    -S "${FIXTURE_SOURCE}"
    -B "${build_dir}"
    -G "${GENERATOR}"
    --no-warn-unused-cli
    "-DCLOPTS_SOURCE_DIR=${CLOPTS_SOURCE_DIR}"
    "-DFIXTURE_BIN_DIR=${bin_dir}"
    -DCLOPTS_BUILD_TESTING=OFF
)
if(NOT "${GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND configure_args -A "${GENERATOR_PLATFORM}")
endif()
if(NOT "${C_COMPILER}" STREQUAL "" AND NOT GENERATOR MATCHES "^Visual Studio")
    list(APPEND configure_args "-DCMAKE_C_COMPILER=${C_COMPILER}")
endif()
if(NOT "${CONFIG}" STREQUAL "")
    list(APPEND configure_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
endif()
if(MODE STREQUAL "install_find_package")
    list(APPEND configure_args "-DCMAKE_PREFIX_PATH=${prefix_dir}")
endif()
list(APPEND configure_args ${EXTRA_ARGS})

_fixture_run(result output "${CMAKE_COMMAND}" ${configure_args})
if(NOT result EQUAL 0)
    # Also for EXPECT=build_fail: a configure failure is never the expected
    # failure, so a broken fixture cannot pass as "build failed".
    _fixture_fail("configure failed" "${result}" "${output}")
endif()

if(EXPECT STREQUAL "configure_ok")
    message(STATUS "run_fixture: configure succeeded as expected")
    return()
endif()

# ---- Build ---------------------------------------------------------------
_fixture_run(result output "${CMAKE_COMMAND}" --build "${build_dir}" ${config_args})

if(EXPECT STREQUAL "build_fail")
    if(result EQUAL 0)
        _fixture_fail("build succeeded but was expected to fail" "${result}" "${output}")
    endif()
    message(STATUS "run_fixture: build failure detected as expected (exit status ${result})")
    return()
endif()

if(NOT result EQUAL 0)
    _fixture_fail("build failed" "${result}" "${output}")
endif()

if(EXPECT STREQUAL "build_ok")
    message(STATUS "run_fixture: build succeeded as expected")
    return()
endif()

# ---- Run (EXPECT=run_ok) -------------------------------------------------
# Multi-config generators append a per-config subdirectory to a
# RUNTIME_OUTPUT_DIRECTORY without generator expressions, so search the tree.
file(GLOB_RECURSE candidates LIST_DIRECTORIES false "${bin_dir}/*")
list(FILTER candidates EXCLUDE REGEX "\\.(pdb|ilk|exp|lib|a|manifest)$")
list(LENGTH candidates count)
if(NOT count EQUAL 1)
    message(FATAL_ERROR
        "run_fixture: EXPECT=run_ok needs exactly one executable under ${bin_dir} "
        "(set RUNTIME_OUTPUT_DIRECTORY to FIXTURE_BIN_DIR in the fixture); found ${count}: ${candidates}")
endif()

_fixture_run(result output "${candidates}")
if(NOT result EQUAL 0)
    _fixture_fail("run of ${candidates} failed" "${result}" "${output}")
endif()
message(STATUS "run_fixture: ${candidates} ran successfully\n${output}")
