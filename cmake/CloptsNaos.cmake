# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Fetching the Naos test framework, pinned to one release.
#
# Naos is used by the test executables only; the clopts target never links it.
# To update Naos, change the two lines below together (tag and the commit it
# points to). Offline configuration: -DFETCHCONTENT_SOURCE_DIR_NAOS=<path>.

include_guard(GLOBAL)

set(CLOPTS_NAOS_TAG "v0.4.0")
set(CLOPTS_NAOS_COMMIT "09fde7172c4b63d416c32e98499cb0c0306d3dea")

include(FetchContent)

# _clopts_check_naos_version(<naos source dir>)
#
# Reads include/naos/version.h and stops configuration unless the version it
# declares matches CLOPTS_NAOS_TAG (without the leading "v").
function(_clopts_check_naos_version naos_dir)
    set(header "${naos_dir}/include/naos/version.h")
    string(REGEX REPLACE "^v" "" expected "${CLOPTS_NAOS_TAG}")

    if(NOT EXISTS "${header}")
        message(FATAL_ERROR
            "Naos version mismatch: expected ${expected}, but '${header}' does not exist "
            "(update CLOPTS_NAOS_TAG / CLOPTS_NAOS_COMMIT in cmake/CloptsNaos.cmake, "
            "or check FETCHCONTENT_SOURCE_DIR_NAOS)")
    endif()

    set(parts "")
    foreach(component IN ITEMS MAJOR MINOR PATCH)
        file(STRINGS "${header}" line
            REGEX "^#define[ \t]+NAOS_VERSION_${component}[ \t]+[0-9]+[ \t]*$")
        if(NOT line)
            message(FATAL_ERROR
                "Naos version mismatch: expected ${expected}, but NAOS_VERSION_${component} "
                "was not found in '${header}' (update the version check in cmake/CloptsNaos.cmake)")
        endif()
        string(REGEX REPLACE "^#define[ \t]+NAOS_VERSION_${component}[ \t]+([0-9]+).*$" "\\1"
            value "${line}")
        list(APPEND parts "${value}")
    endforeach()
    list(JOIN parts "." actual)

    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "Naos version mismatch: expected ${expected} (${CLOPTS_NAOS_TAG}, commit "
            "${CLOPTS_NAOS_COMMIT}), but '${naos_dir}' is ${actual} "
            "(update CLOPTS_NAOS_TAG / CLOPTS_NAOS_COMMIT in cmake/CloptsNaos.cmake, "
            "or check FETCHCONTENT_SOURCE_DIR_NAOS)")
    endif()
endfunction()

# clopts_fetch_naos()
#
# Makes Naos available (fetched by commit SHA, or taken from
# FETCHCONTENT_SOURCE_DIR_NAOS) and checks its version against CLOPTS_NAOS_TAG.
# Postcondition: the Naos::naos target exists. Naos options are left at their
# defaults (NAOS_SELFTEST / NAOS_INSTALL are OFF for a subproject).
function(clopts_fetch_naos)
    # A local copy can be checked before Naos' own CMakeLists.txt runs, so that
    # a wrong version fails here rather than somewhere inside Naos.
    if(FETCHCONTENT_SOURCE_DIR_NAOS)
        _clopts_check_naos_version("${FETCHCONTENT_SOURCE_DIR_NAOS}")
    endif()

    # A full clone: GIT_SHALLOW cannot fetch an arbitrary commit SHA.
    FetchContent_Declare(Naos
        GIT_REPOSITORY https://github.com/vabelorg/naos.git
        GIT_TAG ${CLOPTS_NAOS_COMMIT}
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(Naos)

    FetchContent_GetProperties(Naos SOURCE_DIR naos_source_dir)
    _clopts_check_naos_version("${naos_source_dir}")

    if(NOT TARGET Naos::naos)
        message(FATAL_ERROR
            "clopts_fetch_naos: Naos was made available but Naos::naos is not defined "
            "(see cmake/CloptsNaos.cmake)")
    endif()
endfunction()
