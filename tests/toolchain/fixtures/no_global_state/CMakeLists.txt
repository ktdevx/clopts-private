# Fixture for toolchain.no_global_state (EXPECT=configure_ok).
#
# Pulls CLOpts in with add_subdirectory and fails the configure step if CLOpts
# sets global state: global CMAKE_* defaults, BUILD_SHARED_LIBS /
# BUILD_TESTING, directory-level compile or link settings, propagated compile
# options/definitions, or the test-only Naos. CLOpts is then added a second
# time (another binary directory), which must not be a configure error, and
# the checks run again.
#
# The variables and directory properties are checked in this directory and in
# every directory CLOpts adds: a plain set(CMAKE_C_STANDARD 99) in CLOpts'
# CMakeLists.txt does not reach this scope, but it is still a global default
# for everything defined in CLOpts' directories.
#
# Expects -DCLOPTS_SOURCE_DIR=... (passed by tests/cmake/run_fixture.cmake,
# together with -DCLOPTS_BUILD_TESTING=OFF).

cmake_minimum_required(VERSION 3.21)
project(fixture_no_global_state C)

if("${CLOPTS_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "no_global_state: CLOPTS_SOURCE_DIR is not set")
endif()

# Variables CLOpts must not define (as normal or cache variables).
set(fixture_global_vars
    CMAKE_MSVC_RUNTIME_LIBRARY
    CMAKE_C_STANDARD
    CMAKE_C_EXTENSIONS
    CMAKE_RUNTIME_OUTPUT_DIRECTORY
    CMAKE_LIBRARY_OUTPUT_DIRECTORY
    CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    BUILD_SHARED_LIBS
    BUILD_TESTING
)

# Directory properties that would apply settings to every target of a
# directory instead of to CLOpts' own targets.
set(fixture_dir_props
    COMPILE_OPTIONS
    COMPILE_DEFINITIONS
    INCLUDE_DIRECTORIES
    LINK_LIBRARIES
    LINK_OPTIONS
    LINK_DIRECTORIES
)

# fixture_collect_dirs(<out> <source dir>)
#
# Stores <source dir> and all directories below it that have been added with
# add_subdirectory (recursively) in <out>.
function(fixture_collect_dirs out dir)
    set(result "${dir}")
    get_directory_property(children DIRECTORY "${dir}" SUBDIRECTORIES)
    foreach(child IN LISTS children)
        fixture_collect_dirs(below "${child}")
        list(APPEND result ${below})
    endforeach()
    list(REMOVE_DUPLICATES result)
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

function(fixture_check when)
    # This scope: normal and cache variables.
    foreach(var IN LISTS fixture_global_vars)
        if(DEFINED ${var} OR DEFINED CACHE{${var}})
            message(FATAL_ERROR
                "no_global_state (${when}): ${var} is defined ('${${var}}'); "
                "CLOpts must not set global defaults")
        endif()
    endforeach()

    # Every directory processed so far (this one and those CLOpts added).
    fixture_collect_dirs(dirs "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(dir IN LISTS dirs)
        foreach(var IN LISTS fixture_global_vars)
            get_directory_property(value DIRECTORY "${dir}" DEFINITION ${var})
            if(NOT "${value}" STREQUAL "")
                message(FATAL_ERROR
                    "no_global_state (${when}): ${var} is set to '${value}' in ${dir}; "
                    "CLOpts must not set global defaults")
            endif()
        endforeach()
        foreach(prop IN LISTS fixture_dir_props)
            get_directory_property(value DIRECTORY "${dir}" ${prop})
            if(NOT "${value}" STREQUAL "")
                message(FATAL_ERROR
                    "no_global_state (${when}): directory property ${prop} of ${dir} "
                    "is '${value}'; CLOpts must apply settings per target")
            endif()
        endforeach()
    endforeach()

    if(NOT "${when}" STREQUAL "before add_subdirectory")
        if((NOT TARGET clopts) OR (NOT TARGET CLOpts::clopts))
            message(FATAL_ERROR
                "no_global_state (${when}): targets clopts / CLOpts::clopts are missing")
        endif()
        get_target_property(value clopts INTERFACE_COMPILE_OPTIONS)
        if(value)
            message(FATAL_ERROR
                "no_global_state (${when}): clopts INTERFACE_COMPILE_OPTIONS is '${value}'; "
                "warning flags must be PRIVATE")
        endif()
        # Only the form marker may propagate: CLOPTS_STATIC for the static
        # library, nothing for the shared one. The form is taken from the
        # target itself.
        get_target_property(type clopts TYPE)
        if(type STREQUAL "STATIC_LIBRARY")
            set(expected_defs "CLOPTS_STATIC")
        else()
            set(expected_defs "")
        endif()
        get_target_property(value clopts INTERFACE_COMPILE_DEFINITIONS)
        if(NOT value)
            set(value "")
        endif()
        if(NOT "${value}" STREQUAL "${expected_defs}")
            message(FATAL_ERROR
                "no_global_state (${when}): clopts INTERFACE_COMPILE_DEFINITIONS is "
                "'${value}' (${type}); expected '${expected_defs}': internal "
                "definitions must be PRIVATE")
        endif()
        get_target_property(features clopts INTERFACE_COMPILE_FEATURES)
        if(NOT "${features}" STREQUAL "c_std_99")
            message(FATAL_ERROR
                "no_global_state (${when}): clopts INTERFACE_COMPILE_FEATURES is "
                "'${features}'; expected exactly 'c_std_99'")
        endif()
    endif()

    if(TARGET Naos::naos)
        message(FATAL_ERROR
            "no_global_state (${when}): Naos::naos exists; Naos must only be "
            "fetched when CLOpts' tests are enabled")
    endif()
endfunction()

# Precondition: the checks are only meaningful if nothing is set beforehand.
fixture_check("before add_subdirectory")

add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts)
fixture_check("after add_subdirectory")

# As a subproject, install rules and warnings-as-errors default to OFF
# (CLOPTS_BUILD_TESTING is forced OFF by run_fixture.cmake, so it is not checked here).
foreach(_opt IN ITEMS CLOPTS_INSTALL CLOPTS_WERROR)
    if(${_opt})
        message(FATAL_ERROR "no_global_state: ${_opt} must default to OFF when CLOpts is a subproject")
    endif()
endforeach()

# A second inclusion in the same build must not be a configure error.
add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts_second)
fixture_check("after second add_subdirectory")

message(STATUS "no_global_state: all checks passed")
