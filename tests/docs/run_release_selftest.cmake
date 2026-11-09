# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Self-test driver for check_changelog.cmake MODE=release on a valid input.
# Run with `cmake -P`; registered by
# tests/docs/CMakeLists.txt as docs_selftest.changelog.release_*.
#
# PASS_REGULAR_EXPRESSION cannot compare files, so this script runs the check
# and verifies all of its success contract:
#   - it exits with 0,
#   - its standard output is exactly the line "version <VERSION>",
#   - the notes it writes to OUTPUT are EXPECTED (line endings aside).
#
# Variables (-D):
#   TAG         passed to the check.
#   SOURCE_DIR  passed to the check.
#   CHANGELOG   passed to the check when set (default of the check otherwise).
#   OUTPUT      passed to the check; removed first.
#   EXPECTED    the expected release notes.
#   VERSION     the expected version (X.Y.Z).

cmake_minimum_required(VERSION 3.21)

foreach(var IN ITEMS TAG SOURCE_DIR OUTPUT EXPECTED VERSION)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "run_release_selftest: ${var} is not set")
    endif()
endforeach()

set(args "-DMODE=release" "-DTAG=${TAG}" "-DSOURCE_DIR=${SOURCE_DIR}" "-DOUTPUT=${OUTPUT}")
if(DEFINED CHANGELOG)
    list(APPEND args "-DCHANGELOG=${CHANGELOG}")
endif()

# A stale file from an earlier run must not be taken for the notes.
file(REMOVE "${OUTPUT}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${args} -P "${CMAKE_CURRENT_LIST_DIR}/check_changelog.cmake"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

set(failures "")
if(NOT result EQUAL 0)
    string(APPEND failures "\n exit code ${result} (expected 0); standard error:\n${err}")
endif()
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT out STREQUAL "version ${VERSION}\n")
    string(APPEND failures "\n standard output is '${out}' (expected 'version ${VERSION}' and a newline)")
endif()
if(NOT EXISTS "${OUTPUT}")
    string(APPEND failures "\n notes not written: ${OUTPUT}")
else()
    execute_process(
        # --ignore-eol: EXPECTED may be checked out with CRLF line endings
        # (git core.autocrlf on Windows); the check always writes LF.
        COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol "${OUTPUT}" "${EXPECTED}"
        RESULT_VARIABLE same)
    if(NOT same EQUAL 0)
        string(APPEND failures "\n notes ${OUTPUT} differ from ${EXPECTED}")
    endif()
endif()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR " release self-test failed (${TAG}):${failures}")
endif()
message(STATUS "release self-test: ${TAG} wrote the notes of ${EXPECTED}")
