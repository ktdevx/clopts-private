# Helpers for adding internal modules to the clopts target and registering
# their tests.

include_guard(GLOBAL)

# clopts_add_module(<module> SOURCES <file>...)
#
# Adds the sources of the internal module <module> (src/<module>/) to the
# clopts target. No separate library target is created. Relative source paths
# are resolved against the calling directory.
function(clopts_add_module module)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "clopts_add_module(${module}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR
            "clopts_add_module(${module}): SOURCES must not be empty "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(NOT TARGET clopts)
        message(FATAL_ERROR
            "clopts_add_module(${module}): the clopts target does not exist yet "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()

    set(abs_sources "")
    foreach(src IN LISTS ARG_SOURCES)
        get_filename_component(abs "${src}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND abs_sources "${abs}")
    endforeach()

    target_sources(clopts PRIVATE ${abs_sources})
    set_property(GLOBAL APPEND PROPERTY CLOPTS_MODULES "${module}")
endfunction()

# clopts_add_module_tests(<module> SOURCES <file>...)
#
# Builds the Naos tests of <module> (tests/<module>/) as the executable
# <module>_tests, links CLOpts::clopts (and Naos), gives it the internal include
# root src/, applies the CLOpts compiler options and registers each test with
# CTest as <module>_tests.<suite>.<test> under the label <module>.
# Requires Naos (clopts_fetch_naos) and enable_testing() (include(CTest)).
function(clopts_add_module_tests module)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "clopts_add_module_tests(${module}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR
            "clopts_add_module_tests(${module}): SOURCES must not be empty "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if((NOT TARGET Naos::naos) OR (NOT COMMAND naos_add_executable))
        message(FATAL_ERROR
            "clopts_add_module_tests(${module}): Naos is not available; tests must be added "
            "after clopts_fetch_naos() (see the root CMakeLists.txt and cmake/CloptsNaos.cmake)")
    endif()

    set(target "${module}_tests")

    # The default PREFIX "<target>." gives the test names <module>_tests.<suite>.<test>.
    naos_add_executable("${target}"
        SOURCES ${ARG_SOURCES}
        LIBRARIES CLOpts::clopts
        LABELS "${module}"
    )
    target_include_directories("${target}" PRIVATE "${PROJECT_SOURCE_DIR}/src")
    clopts_apply_compiler_options("${target}")

    # Naos uses <math.h> but does not link libm itself. Not needed with current
    # glibc with GCC/Clang (links without it); kept for other C libraries.
    if(UNIX AND NOT APPLE)
        target_link_libraries("${target}" PRIVATE m)
    endif()
endfunction()
