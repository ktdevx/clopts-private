# Copyright (c) 2026 The CLOpts Authors
# SPDX-License-Identifier: MIT

set(clopts_process_test_sources
    process_tests_main.c
    test_util.c
    test_cmdline.c
    test_argv.c
    test_env.c
    test_oom.c
    ../support/test_alloc.c
)
# The Windows-only suites (registered under #ifdef _WIN32 in process_tests_main.c).
if(WIN32)
    list(APPEND clopts_process_test_sources test_convert.c test_crt.c)
endif()

clopts_add_module_tests(process SOURCES ${clopts_process_test_sources})

# process_probe (Windows only): compares clopts_argv_create with the CRT's
# __argc / __argv; suite crt starts it with raw command lines. It links the
# same target as the unit tests (a static library in both builds, so no DLL
# has to be found at run time).
if(WIN32)
    get_property(clopts_process_test_link GLOBAL PROPERTY CLOPTS_TEST_LINK_TARGET)
    add_executable(process_probe process_probe.c)
    target_link_libraries(process_probe PRIVATE "${clopts_process_test_link}")
    clopts_apply_compiler_options(process_probe)
    add_dependencies(process_tests process_probe)
    target_compile_definitions(process_tests PRIVATE
        "PROCESS_PROBE_PATH=\"$<TARGET_FILE:process_probe>\"")
endif()

# process.no_shell32 (all OSes): nothing under src/ uses shell32;
# see check_no_shell32.cmake for what is matched.
add_test(NAME process.no_shell32
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_no_shell32.cmake"
)
set_tests_properties(process.no_shell32 PROPERTIES LABELS process)
