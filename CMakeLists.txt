cmake_minimum_required(VERSION 3.21)

project(clopts VERSION 0.1.0 LANGUAGES C)

# Guard against a second inclusion of CLOpts in the same build (e.g. pulled in
# by two different dependencies). The first definition wins.
if(TARGET clopts)
    return()
endif()

# Library form: shared or static. Defaults to the configure-time value of
# BUILD_SHARED_LIBS (static when undefined); a parent's normal variable
# CLOPTS_BUILD_SHARED takes precedence (CMP0077 is NEW).
if(BUILD_SHARED_LIBS)
    set(_clopts_shared_default ON)
else()
    set(_clopts_shared_default OFF)
endif()
option(CLOPTS_BUILD_SHARED "Build CLOpts as a shared library" ${_clopts_shared_default})
unset(_clopts_shared_default)

option(CLOPTS_BUILD_TESTING "Build CLOpts tests" ${PROJECT_IS_TOP_LEVEL})
option(CLOPTS_INSTALL "Generate install rules for CLOpts" ${PROJECT_IS_TOP_LEVEL})
option(CLOPTS_WERROR "Treat warnings as errors in CLOpts targets" ${PROJECT_IS_TOP_LEVEL})
option(CLOPTS_BUILD_EXAMPLES "Build CLOpts examples" ${PROJECT_IS_TOP_LEVEL})

# Defines CMAKE_INSTALL_* cache variables only; needed for the install
# interface include directory below even when CLOPTS_INSTALL is OFF.
include(GNUInstallDirs)

include("${PROJECT_SOURCE_DIR}/cmake/CloptsCompilerOptions.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/CloptsLibrary.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/CloptsModules.cmake")
# Always included: tests/docs/ registers the README minimal example with
# clopts_add_doc_program even when the examples are not built.
include("${PROJECT_SOURCE_DIR}/cmake/CloptsExamples.cmake")

# Creates clopts (static or shared) and CLOpts::clopts; sets CLOPTS_SOVERSION
# and CLOPTS_VERSION_COMPATIBILITY in this scope.
clopts_create_library()

add_subdirectory(src)

# Examples (ON by default at top level, OFF as a subproject). Added before the
# tests so that tests/docs/ sees every program registered by
# clopts_add_doc_program.
if(CLOPTS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests: include(CTest) only when CLOpts' tests are wanted, so that a consuming
# project never gets BUILD_TESTING or enable_testing() from CLOpts. Naos is
# fetched only when testing is actually enabled; clopts itself never uses it.
if(CLOPTS_BUILD_TESTING)
    include(CTest)
    if(BUILD_TESTING)
        include("${PROJECT_SOURCE_DIR}/cmake/CloptsNaos.cmake")
        clopts_fetch_naos()
        add_subdirectory(tests)
    endif()
endif()

# Install rules, export set and package config: only when requested (ON by
# default at top level, OFF as a subproject).
if(CLOPTS_INSTALL)
    include("${PROJECT_SOURCE_DIR}/cmake/CloptsInstall.cmake")
    clopts_install()
endif()
