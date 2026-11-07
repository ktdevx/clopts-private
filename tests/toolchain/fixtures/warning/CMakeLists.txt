# Fixture for toolchain.warning_fails (EXPECT=build_fail).
#
# Applies the CLOpts compiler options to warning.c, which only has warnings
# (unused variable, signed/unsigned comparison). With CLOPTS_WERROR=ON
# (passed by the test) the build must fail.
#
# CLOpts is added EXCLUDE_FROM_ALL so that the only target built is the one
# under test and a failure cannot come from anywhere else.

cmake_minimum_required(VERSION 3.21)
project(fixture_warning C)

if("${CLOPTS_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "warning fixture: CLOPTS_SOURCE_DIR is not set")
endif()

add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts EXCLUDE_FROM_ALL)

if(NOT CLOPTS_WERROR)
    message(FATAL_ERROR "warning fixture: run with -DCLOPTS_WERROR=ON")
endif()

add_executable(fixture_warning warning.c)
clopts_apply_compiler_options(fixture_warning)
