set(clopts_platform_test_sources
    platform_tests_main.c
    test_util.c
    test_split.c
    test_argv.c
    test_env.c
    test_oom.c
    ../support/test_alloc.c
)
# The Windows-only suites (registered under #ifdef _WIN32 in platform_tests_main.c).
if(WIN32)
    list(APPEND clopts_platform_test_sources test_convert.c test_crt.c)
endif()

clopts_add_module_tests(platform SOURCES ${clopts_platform_test_sources})

# platform_probe (Windows only): compares clopts_argv_get with the CRT's
# __argc / __argv; suite crt starts it with raw command lines. It links the
# same target as the unit tests (a static library in both builds, so no DLL
# has to be found at run time).
if(WIN32)
    get_property(clopts_platform_test_link GLOBAL PROPERTY CLOPTS_TEST_LINK_TARGET)
    add_executable(platform_probe platform_probe.c)
    target_link_libraries(platform_probe PRIVATE "${clopts_platform_test_link}")
    clopts_apply_compiler_options(platform_probe)
    add_dependencies(platform_tests platform_probe)
    target_compile_definitions(platform_tests PRIVATE
        "PLATFORM_PROBE_PATH=\"$<TARGET_FILE:platform_probe>\"")
endif()

# platform.no_shell32 (all OSes): nothing under src/ uses shell32
# (requirement 8.1); see check_no_shell32.cmake for what is matched.
add_test(NAME platform.no_shell32
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_no_shell32.cmake"
)
set_tests_properties(platform.no_shell32 PROPERTIES LABELS platform)
