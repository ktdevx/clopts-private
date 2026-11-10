# Fixture for integration.internal_hidden (EXPECT=build_fail,
# FAIL_REGEX=clopts_size_mul; registered only for shared builds).
#
# Pulls in CLOpts with add_subdirectory in the form of the parent build tree
# (CLOPTS_BUILD_SHARED, passed by tests/integration/CMakeLists.txt) and links
# an executable whose main.c declares and calls the internal function
# clopts_size_mul itself. The shared library exports only the functions
# marked with CLOPTS_API, so the link must fail with an unresolved reference
# to that function. Against the static library it would link, which is why
# the test is not registered there.

cmake_minimum_required(VERSION 3.21)
project(fixture_internal_symbol C)

if("${CLOPTS_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "internal_symbol fixture: CLOPTS_SOURCE_DIR is not set")
endif()

# Plain variables, not cache entries: that is what a consumer would write.
set(CLOPTS_BUILD_TESTING OFF)
set(CLOPTS_INSTALL OFF)
set(CLOPTS_WERROR OFF)
add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLOpts::clopts)
set_target_properties(app PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FIXTURE_BIN_DIR}")
