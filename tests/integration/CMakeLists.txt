# Consumer integration tests (label: integration): add_subdirectory and
# install -> find_package paths, driven by tests/cmake/run_fixture.cmake.
# Both run fixtures/consumer, a project that only links CLOpts::clopts:
#
#   integration.subdirectory          CLOpts via add_subdirectory; nothing of
#                                     its PRIVATE warning flags / definitions
#                                     reaches the consumer (EXPECT=run_ok)
#   integration.install_find_package  CLOpts installed to a prefix, then
#                                     find_package(CLOpts <version> CONFIG
#                                     REQUIRED) (EXPECT=run_ok). Registered
#                                     only when CLOPTS_INSTALL is ON.

# Multi-config generators pick the configuration at ctest time; single-config
# generators use the configuration of this build tree.
get_property(_clopts_integration_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_clopts_integration_multi_config)
    set(_clopts_integration_config "$<CONFIG>")
else()
    set(_clopts_integration_config "${CMAKE_BUILD_TYPE}")
endif()

# _clopts_integration_add_test(<name> <mode> <consumer_mode>)
#
# Registers integration.<name>, which runs fixtures/consumer with
# CLOPTS_CONSUMER_MODE=<consumer_mode> through run_fixture.cmake in
# MODE=<mode>, with the generator, platform, C compiler and configuration of
# this build.
function(_clopts_integration_add_test name mode consumer_mode)
    add_test(NAME "integration.${name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/consumer"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/${name}"
            -DEXPECT=run_ok
            "-DMODE=${mode}"
            "-DCONFIG=${_clopts_integration_config}"
            "-DCLOPTS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DCLOPTS_BINARY_DIR=${PROJECT_BINARY_DIR}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DC_COMPILER=${CMAKE_C_COMPILER}"
            "-DEXTRA_ARGS=-DCLOPTS_CONSUMER_MODE=${consumer_mode}\;-DEXPECTED_VERSION=${PROJECT_VERSION}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/run_fixture.cmake"
    )
    set_tests_properties("integration.${name}" PROPERTIES LABELS integration)
endfunction()

_clopts_integration_add_test(subdirectory SUBDIRECTORY SUBDIRECTORY)

# The install rules exist only when CLOPTS_INSTALL is ON.
if(CLOPTS_INSTALL)
    _clopts_integration_add_test(install_find_package install_find_package PACKAGE)
endif()
