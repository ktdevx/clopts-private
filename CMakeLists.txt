cmake_minimum_required(VERSION 3.21)

project(clopts VERSION 0.1.0 LANGUAGES C)

# Guard against a second inclusion of CLOpts in the same build (e.g. pulled in
# by two different dependencies). The first definition wins.
if(TARGET clopts)
    return()
endif()

option(CLOPTS_BUILD_TESTING "Build CLOpts tests" ${PROJECT_IS_TOP_LEVEL})
option(CLOPTS_INSTALL "Generate install rules for CLOpts" ${PROJECT_IS_TOP_LEVEL})
option(CLOPTS_WERROR "Treat warnings as errors in CLOpts targets" ${PROJECT_IS_TOP_LEVEL})

# Defines CMAKE_INSTALL_* cache variables only; needed for the install
# interface include directory below even when CLOPTS_INSTALL is OFF.
include(GNUInstallDirs)

include("${PROJECT_SOURCE_DIR}/cmake/CloptsCompilerOptions.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/CloptsModules.cmake")

add_library(clopts STATIC)
add_library(CLOpts::clopts ALIAS clopts)

target_include_directories(clopts
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

clopts_apply_compiler_options(clopts)

add_subdirectory(src)

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
