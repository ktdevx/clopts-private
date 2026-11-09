# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT
#
# Install rules, export sets and package config files for CLOpts.
#
# Included and called by the root CMakeLists.txt only when CLOPTS_INSTALL is
# ON. Only the clopts library, the public headers under include/clopts/ and the
# generated package files are installed; nothing under src/ is (in particular
# the internal clopts_testing library is never installed or exported).

include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# clopts_install()
#
# Installs the clopts library (static or shared, as built), the include/clopts/
# header tree and the package files under <libdir>/cmake/CLOpts:
#   CLOptsTargets.cmake          export set (namespace CLOpts::)
#   CLOptsConfig.cmake           loads CLOptsTargets.cmake; records the form
#                                (STATIC or SHARED) and, for the static form,
#                                the MSVC runtime library (CRT) the build used
#   CLOptsConfigVersion.cmake    version file
#
# A prefix holds one form. Every package file has the same name for both
# forms, so installing the other form into the same prefix replaces them all
# consistently (the last install wins); library files of the earlier form are
# left behind but no longer referenced.
#
# The version comes from the root project() only; the compatibility rule is
# CLOPTS_VERSION_COMPATIBILITY, set by clopts_create_library()
# (cmake/CloptsLibrary.cmake) in the calling scope.
function(clopts_install)
    if(NOT TARGET clopts)
        message(FATAL_ERROR
            "clopts_install: the clopts target does not exist yet "
            "(see cmake/CloptsInstall.cmake)")
    endif()
    if(NOT CLOPTS_VERSION_COMPATIBILITY)
        message(FATAL_ERROR
            "clopts_install: CLOPTS_VERSION_COMPATIBILITY is not set; call "
            "clopts_create_library() first (see cmake/CloptsInstall.cmake)")
    endif()

    get_target_property(type clopts TYPE)
    if(type STREQUAL "STATIC_LIBRARY")
        set(CLOPTS_PACKAGE_LIBRARY_TYPE "STATIC")
    elseif(type STREQUAL "SHARED_LIBRARY")
        set(CLOPTS_PACKAGE_LIBRARY_TYPE "SHARED")
    else()
        message(FATAL_ERROR
            "clopts_install: unexpected type '${type}' of the clopts target "
            "(see cmake/CloptsInstall.cmake)")
    endif()

    # Recorded as a plain string (empty for the shared form, when unset, or
    # when not using MSVC); see CLOptsConfig.cmake.in for how it is compared.
    set(CLOPTS_RECORDED_MSVC_RUNTIME_LIBRARY "")
    if(CLOPTS_PACKAGE_LIBRARY_TYPE STREQUAL "STATIC" AND MSVC)
        set(CLOPTS_RECORDED_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()

    set(config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/CLOpts")
    set(gen_dir "${PROJECT_BINARY_DIR}/cmake")

    # ARCHIVE: static library and Windows import library.
    # LIBRARY: versioned shared object and its namelink (Linux / macOS).
    # RUNTIME: DLL (Windows).
    install(TARGETS clopts
        EXPORT CLOptsTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/clopts"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(EXPORT CLOptsTargets
        NAMESPACE CLOpts::
        FILE CLOptsTargets.cmake
        DESTINATION "${config_dir}"
    )

    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/CLOptsConfig.cmake.in"
        "${gen_dir}/CLOptsConfig.cmake"
        INSTALL_DESTINATION "${config_dir}"
    )
    # Not ARCH_INDEPENDENT: an install built for another architecture is not
    # considered compatible.
    write_basic_package_version_file(
        "${gen_dir}/CLOptsConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY "${CLOPTS_VERSION_COMPATIBILITY}"
    )
    install(FILES
        "${gen_dir}/CLOptsConfig.cmake"
        "${gen_dir}/CLOptsConfigVersion.cmake"
        DESTINATION "${config_dir}"
    )
endfunction()
