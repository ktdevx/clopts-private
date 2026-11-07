# Install rules, export set and package config files for CLOpts.
#
# Included and called by the root CMakeLists.txt only when CLOPTS_INSTALL is
# ON. Only the clopts library, the public headers under include/clopts/ and the
# generated package files are installed; nothing under src/ is.

include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# clopts_install()
#
# Installs the clopts static library, the include/clopts/ header tree, the
# export set CLOptsTargets (namespace CLOpts::), CLOptsConfig.cmake and
# CLOptsConfigVersion.cmake under <libdir>/cmake/CLOpts. The version comes
# from the root project() only.
function(clopts_install)
    if(NOT TARGET clopts)
        message(FATAL_ERROR
            "clopts_install: the clopts target does not exist yet "
            "(see cmake/CloptsInstall.cmake)")
    endif()

    set(config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/CLOpts")
    set(gen_dir "${PROJECT_BINARY_DIR}/cmake")

    install(TARGETS clopts
        EXPORT CLOptsTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/clopts"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(EXPORT CLOptsTargets
        NAMESPACE CLOpts::
        FILE CLOptsTargets.cmake
        DESTINATION "${config_dir}"
    )

    # Recorded as a plain string (empty when unset or not using MSVC); see
    # CLOptsConfig.cmake.in for how it is compared.
    set(CLOPTS_RECORDED_MSVC_RUNTIME_LIBRARY "")
    if(MSVC)
        set(CLOPTS_RECORDED_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()

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
        COMPATIBILITY SameMinorVersion
    )
    install(FILES
        "${gen_dir}/CLOptsConfig.cmake"
        "${gen_dir}/CLOptsConfigVersion.cmake"
        DESTINATION "${config_dir}"
    )
endfunction()
