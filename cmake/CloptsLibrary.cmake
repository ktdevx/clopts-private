# Creation of the clopts library target (static or shared).
#
# The form is chosen once per build tree by CLOPTS_BUILD_SHARED (defined in
# the root CMakeLists.txt). All form-specific settings are applied to the
# clopts target only; no CMAKE_* variable, BUILD_SHARED_LIBS or directory
# property is touched.

include_guard(GLOBAL)

# clopts_compute_soversion(<version> <out_soversion> <out_compat>)
#
# Computes the SOVERSION and the package version compatibility rule from
# <version> (major.minor[.patch...]):
#   0.x   -> SOVERSION 0.<minor>, compatibility SameMinorVersion
#   >=1.0 -> SOVERSION <major>,   compatibility SameMajorVersion
# The results are stored in <out_soversion> and <out_compat> in the caller's
# scope.
function(clopts_compute_soversion version out_soversion out_compat)
    if(NOT "${version}" MATCHES "^([0-9]+)\\.([0-9]+)")
        message(FATAL_ERROR
            "clopts_compute_soversion: '${version}' is not a major.minor[.patch] version "
            "(see cmake/CloptsLibrary.cmake)")
    endif()
    set(major "${CMAKE_MATCH_1}")
    set(minor "${CMAKE_MATCH_2}")

    if(major EQUAL 0)
        set(${out_soversion} "0.${minor}" PARENT_SCOPE)
        set(${out_compat} "SameMinorVersion" PARENT_SCOPE)
    else()
        set(${out_soversion} "${major}" PARENT_SCOPE)
        set(${out_compat} "SameMajorVersion" PARENT_SCOPE)
    endif()
endfunction()

# clopts_create_library()
#
# Creates the clopts library (STATIC or SHARED according to
# CLOPTS_BUILD_SHARED) and its alias CLOpts::clopts, sets the include roots,
# applies the CLOpts compiler options and the form-specific properties, and
# sets CLOPTS_SOVERSION and CLOPTS_VERSION_COMPATIBILITY in the caller's scope
# (normal variables, not cached).
function(clopts_create_library)
    if(NOT DEFINED CLOPTS_BUILD_SHARED)
        message(FATAL_ERROR
            "clopts_create_library: CLOPTS_BUILD_SHARED is not defined "
            "(see cmake/CloptsLibrary.cmake)")
    endif()
    if(TARGET clopts)
        message(FATAL_ERROR
            "clopts_create_library: the clopts target already exists "
            "(see cmake/CloptsLibrary.cmake)")
    endif()

    clopts_compute_soversion("${PROJECT_VERSION}" soversion compat)

    if(CLOPTS_BUILD_SHARED)
        add_library(clopts SHARED)
    else()
        add_library(clopts STATIC)
    endif()
    add_library(CLOpts::clopts ALIAS clopts)

    target_include_directories(clopts
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
    )

    clopts_apply_compiler_options(clopts)

    if(CLOPTS_BUILD_SHARED)
        # CLOPTS_BUILDING is set only while compiling the library itself.
        set_target_properties(clopts PROPERTIES
            DEFINE_SYMBOL CLOPTS_BUILDING
            VERSION "${PROJECT_VERSION}"
            SOVERSION "${soversion}"
        )
        # On PE targets visibility has no effect (exports are limited by the
        # dllexport marker), and GCC may warn with -Wattributes.
        if(NOT WIN32)
            set_target_properties(clopts PROPERTIES C_VISIBILITY_PRESET hidden)
        endif()
    else()
        set_target_properties(clopts PROPERTIES POSITION_INDEPENDENT_CODE ON)
        # PUBLIC: both the library itself and its consumers see CLOPTS_STATIC
        # (and it is recorded in the exported target).
        target_compile_definitions(clopts PUBLIC CLOPTS_STATIC)
        if(MSVC)
            # Keeps clopts_static.lib apart from the shared import library clopts.lib.
            set_target_properties(clopts PROPERTIES OUTPUT_NAME clopts_static)
        endif()
    endif()

    set(CLOPTS_SOVERSION "${soversion}" PARENT_SCOPE)
    set(CLOPTS_VERSION_COMPATIBILITY "${compat}" PARENT_SCOPE)
endfunction()
