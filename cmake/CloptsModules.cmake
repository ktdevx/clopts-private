# Helpers for adding internal modules to the clopts target.

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
