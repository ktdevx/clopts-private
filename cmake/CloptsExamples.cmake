# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Programs referenced by the documentation: the examples under examples/ and
# the README minimal example under tests/docs/.
#
# Included unconditionally by the root CMakeLists.txt (tests/docs/ uses the
# same function even when CLOPTS_BUILD_EXAMPLES is OFF). Nothing here is
# installed.

include_guard(GLOBAL)

# clopts_add_doc_program(<name> SOURCES <file>... [ENV <var>...] [EXAMPLE])
#
#   <name>   program name used by the documentation markers. Must match
#            [a-z][a-z0-9_-]* and must not be registered already.
#   SOURCES  C sources (relative paths are resolved against the calling
#            directory).
#   ENV      names of the environment variables the program reads; the docs
#            check isolates them when it runs the program.
#   EXAMPLE  marks an example under examples/ (checked by docs.outputs
#            REQUIRE).
#
# Creates the executable target clopts_doc_<name> (OUTPUT_NAME <name>) linked
# to CLOpts::clopts with the CLOpts compiler options, and records it in the
# GLOBAL properties
#   CLOPTS_DOC_PROGRAMS               list of <name>
#   CLOPTS_DOC_PROGRAM_TARGET_<name>  the target name
#   CLOPTS_DOC_PROGRAM_ENV_<name>     the ENV names
#   CLOPTS_DOC_EXAMPLES               list of <name> registered with EXAMPLE
function(clopts_add_doc_program name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "EXAMPLE" "" "SOURCES;ENV")

    if(NOT name MATCHES "^[a-z][a-z0-9_-]*$")
        message(FATAL_ERROR
            "clopts_add_doc_program(${name}): the name must match [a-z][a-z0-9_-]* "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "clopts_add_doc_program(${name}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR
            "clopts_add_doc_program(${name}): SOURCES must not be empty "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(NOT TARGET CLOpts::clopts)
        message(FATAL_ERROR
            "clopts_add_doc_program(${name}): CLOpts::clopts does not exist yet "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()

    get_property(programs GLOBAL PROPERTY CLOPTS_DOC_PROGRAMS)
    if(name IN_LIST programs)
        get_property(previous GLOBAL PROPERTY CLOPTS_DOC_PROGRAM_FILE_${name})
        message(FATAL_ERROR
            "clopts_add_doc_program(${name}): the name '${name}' is already registered "
            "(by ${previous}; again in ${CMAKE_CURRENT_LIST_FILE})")
    endif()

    set(target "clopts_doc_${name}")
    add_executable("${target}" ${ARG_SOURCES})
    target_link_libraries("${target}" PRIVATE CLOpts::clopts)
    set_target_properties("${target}" PROPERTIES OUTPUT_NAME "${name}")
    clopts_apply_compiler_options("${target}")

    # Windows has no rpath: put the CLOpts DLL next to the program so that it
    # can run (same approach as the integration test fixtures).
    get_target_property(clopts_type CLOpts::clopts TYPE)
    if(WIN32 AND clopts_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
        )
    endif()

    set_property(GLOBAL APPEND PROPERTY CLOPTS_DOC_PROGRAMS "${name}")
    set_property(GLOBAL PROPERTY CLOPTS_DOC_PROGRAM_TARGET_${name} "${target}")
    set_property(GLOBAL PROPERTY CLOPTS_DOC_PROGRAM_ENV_${name} "${ARG_ENV}")
    set_property(GLOBAL PROPERTY CLOPTS_DOC_PROGRAM_FILE_${name} "${CMAKE_CURRENT_LIST_FILE}")
    if(ARG_EXAMPLE)
        set_property(GLOBAL APPEND PROPERTY CLOPTS_DOC_EXAMPLES "${name}")
    endif()
endfunction()
