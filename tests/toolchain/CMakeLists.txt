# Build-configuration tests (label: toolchain). Each test drives a fixture
# project under fixtures/ through tests/cmake/run_fixture.cmake:
#
#   toolchain.no_global_state  CLOpts as a subproject leaves no global state
#                              behind (EXPECT=configure_ok)
#   toolchain.warning_fails    warnings are errors with CLOPTS_WERROR=ON
#                              (EXPECT=build_fail)
#   toolchain.vla_fails        a VLA is a build error (EXPECT=build_fail)

# Multi-config generators pick the configuration at ctest time; single-config
# generators use the configuration of this build tree.
get_property(_clopts_toolchain_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_clopts_toolchain_multi_config)
    set(_clopts_toolchain_config "$<CONFIG>")
else()
    set(_clopts_toolchain_config "${CMAKE_BUILD_TYPE}")
endif()

# _clopts_toolchain_add_test(<name> <fixture> <expect> [EXTRA_ARGS <arg>...])
#
# Registers toolchain.<name>, which runs fixtures/<fixture> with the generator,
# platform, C compiler and configuration of this build.
function(_clopts_toolchain_add_test name fixture expect)
    cmake_parse_arguments(PARSE_ARGV 3 ARG "" "" "EXTRA_ARGS")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "_clopts_toolchain_add_test(${name}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()

    add_test(NAME "toolchain.${name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${fixture}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/${name}"
            "-DEXPECT=${expect}"
            "-DCONFIG=${_clopts_toolchain_config}"
            "-DCLOPTS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DC_COMPILER=${CMAKE_C_COMPILER}"
            "-DEXTRA_ARGS=${ARG_EXTRA_ARGS}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/run_fixture.cmake"
    )
    set_tests_properties("toolchain.${name}" PROPERTIES LABELS toolchain)
endfunction()

_clopts_toolchain_add_test(no_global_state no_global_state configure_ok)

# CLOPTS_WERROR is set explicitly: the result must not depend on the parent.
_clopts_toolchain_add_test(warning_fails warning build_fail
    EXTRA_ARGS -DCLOPTS_WERROR=ON)

# CLOPTS_WERROR=OFF on purpose: the VLA must fail through -Werror=vla (GCC /
# Clang) or the language itself (MSVC), not through warnings-as-errors.
_clopts_toolchain_add_test(vla_fails vla build_fail
    EXTRA_ARGS -DCLOPTS_WERROR=OFF)
