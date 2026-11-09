# Consumer fixture for integration.subdirectory and
# integration.install_find_package (EXPECT=run_ok).
#
# Mimics a project that uses CLOpts by linking CLOpts::clopts and nothing
# else, from C (main.c) and C++ (consumer_cpp.cpp). CLOPTS_CONSUMER_MODE
# selects how CLOpts is brought in:
#
#   SUBDIRECTORY  add_subdirectory(${CLOPTS_SOURCE_DIR}) with tests, install
#                 rules and warnings-as-errors turned off; the form comes from
#                 CLOPTS_BUILD_SHARED (passed on the command line)
#   PACKAGE       find_package(CLOpts ${EXPECTED_VERSION} CONFIG REQUIRED)
#                 from the prefix run_fixture.cmake installed to; the prefix
#                 holds the form of the parent build tree only
#
# EXPECTED_TYPE (STATIC_LIBRARY or SHARED_LIBRARY) is the form of the parent
# build tree; CLOpts::clopts must have exactly that TYPE. For a shared
# CLOpts on Windows, the DLL is copied next to app after the build so that
# app can be run from FIXTURE_BIN_DIR.
#
# app gets no warning flags of its own. CLOpts' warning flags and internal
# definitions are PRIVATE, so app must neither see them in CLOpts::clopts'
# usage requirements (checked below) nor be compiled with them (main.c has an
# unused parameter and an #error on _CRT_SECURE_NO_WARNINGS, and so has
# consumer_cpp.cpp).

cmake_minimum_required(VERSION 3.21)
project(fixture_consumer C CXX)

if(NOT EXPECTED_TYPE MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY)$")
    message(FATAL_ERROR
        "consumer fixture: EXPECTED_TYPE must be STATIC_LIBRARY or SHARED_LIBRARY "
        "(got '${EXPECTED_TYPE}')")
endif()

if(CLOPTS_CONSUMER_MODE STREQUAL "SUBDIRECTORY")
    if("${CLOPTS_SOURCE_DIR}" STREQUAL "")
        message(FATAL_ERROR "consumer fixture: CLOPTS_SOURCE_DIR is not set")
    endif()
    # Plain variables, not cache entries: that is what a consumer would write.
    set(CLOPTS_BUILD_TESTING OFF)
    set(CLOPTS_INSTALL OFF)
    set(CLOPTS_WERROR OFF)
    add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts)
elseif(CLOPTS_CONSUMER_MODE STREQUAL "PACKAGE")
    if("${EXPECTED_VERSION}" STREQUAL "")
        message(FATAL_ERROR "consumer fixture: EXPECTED_VERSION is not set")
    endif()
    find_package(CLOpts ${EXPECTED_VERSION} CONFIG REQUIRED)
    if(EXPECTED_TYPE STREQUAL "SHARED_LIBRARY")
        set(_consumer_package_type SHARED)
    else()
        set(_consumer_package_type STATIC)
    endif()
    if(NOT "${CLOpts_LIBRARY_TYPE}" STREQUAL _consumer_package_type)
        message(FATAL_ERROR
            "consumer fixture: CLOpts_LIBRARY_TYPE is '${CLOpts_LIBRARY_TYPE}', "
            "expected ${_consumer_package_type}")
    endif()
else()
    message(FATAL_ERROR
        "consumer fixture: CLOPTS_CONSUMER_MODE must be SUBDIRECTORY or PACKAGE "
        "(got '${CLOPTS_CONSUMER_MODE}')")
endif()

if(NOT TARGET CLOpts::clopts)
    message(FATAL_ERROR "consumer fixture: CLOpts::clopts is not defined")
endif()

# The form of CLOpts::clopts must be the form of the parent build tree.
get_target_property(_consumer_type CLOpts::clopts TYPE)
if(NOT _consumer_type STREQUAL EXPECTED_TYPE)
    message(FATAL_ERROR
        "consumer fixture: CLOpts::clopts is ${_consumer_type}, expected ${EXPECTED_TYPE} "
        "(mode ${CLOPTS_CONSUMER_MODE})")
endif()

# Warning flags (GCC/Clang -W..., MSVC /W...) and _CRT_SECURE_NO_WARNINGS must
# not be part of what CLOpts::clopts hands to its consumers. Without
# warnings-as-errors, a propagated -Wall would not break the build of main.c,
# so it is detected here.
get_target_property(_consumer_options CLOpts::clopts INTERFACE_COMPILE_OPTIONS)
if(_consumer_options AND _consumer_options MATCHES "(^|[;>:])[-/]W")
    message(FATAL_ERROR
        "consumer fixture: CLOpts::clopts propagates warning flags to consumers "
        "(INTERFACE_COMPILE_OPTIONS: ${_consumer_options})")
endif()
get_target_property(_consumer_definitions CLOpts::clopts INTERFACE_COMPILE_DEFINITIONS)
if(_consumer_definitions AND _consumer_definitions MATCHES "_CRT_SECURE_NO_WARNINGS")
    message(FATAL_ERROR
        "consumer fixture: CLOpts::clopts propagates _CRT_SECURE_NO_WARNINGS to consumers "
        "(INTERFACE_COMPILE_DEFINITIONS: ${_consumer_definitions})")
endif()

add_executable(app main.c consumer_cpp.cpp)
target_link_libraries(app PRIVATE CLOpts::clopts)
set_target_properties(app PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FIXTURE_BIN_DIR}")

# Windows has no rpath: put the CLOpts DLL next to app so that it can run.
if(WIN32 AND _consumer_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET app POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:app> $<TARGET_FILE_DIR:app>
        COMMAND_EXPAND_LISTS
    )
endif()
