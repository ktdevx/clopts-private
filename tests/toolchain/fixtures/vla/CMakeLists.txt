# Fixture for toolchain.vla_fails (EXPECT=build_fail).
#
# Applies the CLOpts compiler options to vla.c, which is warning-free C99
# except for a variable length array. The test passes CLOPTS_WERROR=OFF, so
# the build fails only through -Werror=vla (GCC / Clang) or because the
# compiler has no VLAs at all (MSVC).
#
# CLOpts is added EXCLUDE_FROM_ALL so that the only target built is the one
# under test.

cmake_minimum_required(VERSION 3.21)
project(fixture_vla C)

if("${CLOPTS_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "vla fixture: CLOPTS_SOURCE_DIR is not set")
endif()

add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts EXCLUDE_FROM_ALL)

if(CLOPTS_WERROR)
    message(FATAL_ERROR "vla fixture: run with -DCLOPTS_WERROR=OFF")
endif()

add_executable(fixture_vla vla.c)
clopts_apply_compiler_options(fixture_vla)
