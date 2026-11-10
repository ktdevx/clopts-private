# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Consumer integration tests (label: integration), driven by
# tests/cmake/run_fixture.cmake. All of them use the form (static or shared)
# of this build tree:
#
#   integration.subdirectory          fixtures/consumer with CLOpts via
#                                     add_subdirectory (CLOPTS_BUILD_SHARED of
#                                     this tree); nothing of its PRIVATE
#                                     warning flags / definitions reaches the
#                                     consumer, C and C++ both build, app runs
#                                     (EXPECT=run_ok)
#   integration.install_find_package  fixtures/consumer with CLOpts installed
#                                     to a prefix, then find_package(CLOpts
#                                     <version> CONFIG REQUIRED); the prefix
#                                     holds this tree's form only
#                                     (EXPECT=run_ok). Registered only when
#                                     CLOPTS_INSTALL is ON.
#   integration.internal_hidden       fixtures/internal_symbol references the
#                                     internal clopts_size_mul; linking
#                                     against the shared library must fail on
#                                     that symbol (EXPECT=build_fail). Shared
#                                     builds only: a static library has no
#                                     export list, so the link would succeed.

# Multi-config generators pick the configuration at ctest time; single-config
# generators use the configuration of this build tree.
get_property(_clopts_integration_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_clopts_integration_multi_config)
    set(_clopts_integration_config "$<CONFIG>")
else()
    set(_clopts_integration_config "${CMAKE_BUILD_TYPE}")
endif()

# Form of this build tree, as a ON/OFF value and as the expected TYPE of
# CLOpts::clopts in the fixtures.
if(CLOPTS_BUILD_SHARED)
    set(_clopts_integration_shared ON)
    set(_clopts_integration_type SHARED_LIBRARY)
else()
    set(_clopts_integration_shared OFF)
    set(_clopts_integration_type STATIC_LIBRARY)
endif()

# _clopts_integration_add_test(<name> <fixture> <expect> <mode> [FAIL_REGEX <regex>]
#                              [EXTRA_ARGS <arg>...])
#
# Registers integration.<name>, which runs fixtures/<fixture> through
# run_fixture.cmake with EXPECT=<expect> and MODE=<mode>, with the generator,
# platform, C compiler and configuration of this build. The fixture always
# gets -DCLOPTS_BUILD_SHARED=<form of this tree>, followed by EXTRA_ARGS.
function(_clopts_integration_add_test name fixture expect mode)
    cmake_parse_arguments(PARSE_ARGV 4 arg "" "FAIL_REGEX" "EXTRA_ARGS")
    set(extra_args "-DCLOPTS_BUILD_SHARED=${_clopts_integration_shared}" ${arg_EXTRA_ARGS})
    list(JOIN extra_args "\\;" extra_args)
    add_test(NAME "integration.${name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${fixture}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/${name}"
            "-DEXPECT=${expect}"
            "-DFAIL_REGEX=${arg_FAIL_REGEX}"
            "-DMODE=${mode}"
            "-DCONFIG=${_clopts_integration_config}"
            "-DCLOPTS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DCLOPTS_BINARY_DIR=${PROJECT_BINARY_DIR}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DC_COMPILER=${CMAKE_C_COMPILER}"
            "-DEXTRA_ARGS=${extra_args}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/run_fixture.cmake"
    )
    set_tests_properties("integration.${name}" PROPERTIES LABELS integration)
endfunction()

set(_clopts_integration_consumer_args
    "-DEXPECTED_TYPE=${_clopts_integration_type}"
    "-DEXPECTED_VERSION=${PROJECT_VERSION}"
)

_clopts_integration_add_test(subdirectory consumer run_ok SUBDIRECTORY
    EXTRA_ARGS -DCLOPTS_CONSUMER_MODE=SUBDIRECTORY ${_clopts_integration_consumer_args})

# The install rules exist only when CLOPTS_INSTALL is ON.
if(CLOPTS_INSTALL)
    _clopts_integration_add_test(install_find_package consumer run_ok install_find_package
        EXTRA_ARGS -DCLOPTS_CONSUMER_MODE=PACKAGE ${_clopts_integration_consumer_args})
endif()

# Only the shared library hides its internal functions.
if(CLOPTS_BUILD_SHARED)
    _clopts_integration_add_test(internal_hidden internal_symbol build_fail SUBDIRECTORY
        FAIL_REGEX clopts_size_mul)
endif()
