# Selection fixture for integration.package_selection (configure only; driven
# by tests/cmake/run_package_selection.cmake).
#
# Mimics a project that finds an installed CLOpts with
#
#   find_package(CLOpts CONFIG REQUIRED ${SELECT_ARGS})
#
# from CMAKE_PREFIX_PATH (a prefix holding the static form, or both forms side
# by side). SELECT_ARGS is a ;-list such as "COMPONENTS;static"; the defaults
# CLOPTS_SHARED_LIBS / BUILD_SHARED_LIBS, if any, come from the command line.
#
# EXPECTED_TYPE (STATIC_LIBRARY or SHARED_LIBRARY) is the form the package
# config must select: both CLOpts_LIBRARY_TYPE and the TYPE of CLOpts::clopts
# must match it. Cases that expect find_package to fail pass no EXPECTED_TYPE,
# so that an unexpected success still stops the configure step (with a
# message that does not match the expected failure reason).

cmake_minimum_required(VERSION 3.21)
project(fixture_package_select C)

find_package(CLOpts CONFIG REQUIRED ${SELECT_ARGS})

if(NOT EXPECTED_TYPE MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY)$")
    message(FATAL_ERROR
        "package_select fixture: find_package(CLOpts CONFIG REQUIRED ${SELECT_ARGS}) "
        "succeeded (CLOpts_LIBRARY_TYPE '${CLOpts_LIBRARY_TYPE}'), but EXPECTED_TYPE is "
        "'${EXPECTED_TYPE}' (must be STATIC_LIBRARY or SHARED_LIBRARY)")
endif()

# The package must come from the prefix under test, not from elsewhere.
file(REAL_PATH "${CMAKE_PREFIX_PATH}" _prefix)
file(REAL_PATH "${CLOpts_DIR}" _clopts_dir)
string(FIND "${_clopts_dir}/" "${_prefix}/" _pos)
if(NOT _pos EQUAL 0)
    message(FATAL_ERROR
        "package_select fixture: CLOpts was found in '${CLOpts_DIR}', "
        "not under CMAKE_PREFIX_PATH '${CMAKE_PREFIX_PATH}'")
endif()

if(NOT TARGET CLOpts::clopts)
    message(FATAL_ERROR "package_select fixture: CLOpts::clopts does not exist")
endif()
get_target_property(_type CLOpts::clopts TYPE)

message(STATUS "package_select: CLOpts_LIBRARY_TYPE=${CLOpts_LIBRARY_TYPE}, CLOpts::clopts TYPE=${_type}")

if(NOT "${CLOpts_LIBRARY_TYPE}_LIBRARY" STREQUAL EXPECTED_TYPE)
    message(FATAL_ERROR
        "package_select fixture: CLOpts_LIBRARY_TYPE is '${CLOpts_LIBRARY_TYPE}', "
        "expected the form of ${EXPECTED_TYPE}")
endif()
if(NOT _type STREQUAL EXPECTED_TYPE)
    message(FATAL_ERROR
        "package_select fixture: CLOpts::clopts has TYPE '${_type}', expected '${EXPECTED_TYPE}'")
endif()
