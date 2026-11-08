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
#   CLOpts<Kind>Targets.cmake    export set of this form (<Kind> is Static or
#                                Shared; namespace CLOpts::), so that a static
#                                and a shared build installed into the same
#                                prefix do not overwrite each other
#   CLOptsStaticRuntime.cmake    static form only: the MSVC runtime library
#                                (CRT) the build used
#   CLOptsConfig.cmake           identical for both forms; selects the form
#   CLOptsConfigVersion.cmake    identical for both forms
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
        set(kind "Static")
    elseif(type STREQUAL "SHARED_LIBRARY")
        set(kind "Shared")
    else()
        message(FATAL_ERROR
            "clopts_install: unexpected type '${type}' of the clopts target "
            "(see cmake/CloptsInstall.cmake)")
    endif()

    set(config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/CLOpts")
    set(gen_dir "${PROJECT_BINARY_DIR}/cmake")

    # ARCHIVE: static library and Windows import library.
    # LIBRARY: versioned shared object and its namelink (Linux / macOS).
    # RUNTIME: DLL (Windows).
    install(TARGETS clopts
        EXPORT CLOpts${kind}Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/clopts"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(EXPORT CLOpts${kind}Targets
        NAMESPACE CLOpts::
        FILE CLOpts${kind}Targets.cmake
        DESTINATION "${config_dir}"
    )

    if(kind STREQUAL "Static")
        # Recorded as a plain string (empty when unset or not using MSVC); see
        # CLOptsConfig.cmake.in for how it is compared.
        set(CLOPTS_RECORDED_MSVC_RUNTIME_LIBRARY "")
        if(MSVC)
            set(CLOPTS_RECORDED_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        endif()
        configure_file(
            "${PROJECT_SOURCE_DIR}/cmake/CLOptsStaticRuntime.cmake.in"
            "${gen_dir}/CLOptsStaticRuntime.cmake"
            @ONLY
        )
        install(FILES "${gen_dir}/CLOptsStaticRuntime.cmake"
            DESTINATION "${config_dir}"
        )
    endif()

    # The config does not depend on the form, so that either build may
    # overwrite it when both are installed into the same prefix.
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
