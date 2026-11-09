# Subproject fixture for docs.examples_subproject_default and
# docs.examples_subproject_on (EXPECT=configure_ok, requirements 7.6, 7.7).
#
# Mimics a project that pulls CLOpts in with add_subdirectory and checks
# whether CLOpts created its example programs (the target
# clopts_doc_taskbook):
#
#   EXPECT_EXAMPLES not ON  the default of a subproject: the examples must not
#                           be built, so the target must not exist
#   EXPECT_EXAMPLES ON      the parent passed CLOPTS_BUILD_EXAMPLES=ON: the
#                           target must exist
#
# Any other outcome is a FATAL_ERROR, so the configure step fails.
# CLOPTS_SOURCE_DIR and CLOPTS_BUILD_TESTING=OFF come from
# tests/cmake/run_fixture.cmake; CLOPTS_BUILD_SHARED comes from the test.

cmake_minimum_required(VERSION 3.21)
project(fixture_subproject C)

if("${CLOPTS_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "subproject fixture: CLOPTS_SOURCE_DIR is not set")
endif()

# Plain variables, not cache entries: that is what a consumer would write.
# CLOPTS_BUILD_EXAMPLES is deliberately left alone here.
set(CLOPTS_BUILD_TESTING OFF)
set(CLOPTS_INSTALL OFF)
set(CLOPTS_WERROR OFF)
add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts)

if(EXPECT_EXAMPLES)
    if(NOT TARGET clopts_doc_taskbook)
        message(FATAL_ERROR
            "subproject fixture: CLOPTS_BUILD_EXAMPLES=ON was passed, "
            "but the target clopts_doc_taskbook does not exist")
    endif()
    message(STATUS "subproject fixture: clopts_doc_taskbook exists as expected")
else()
    if(TARGET clopts_doc_taskbook)
        message(FATAL_ERROR
            "subproject fixture: the examples are built by default when CLOpts "
            "is a subproject (target clopts_doc_taskbook exists)")
    endif()
    message(STATUS "subproject fixture: clopts_doc_taskbook does not exist as expected")
endif()
