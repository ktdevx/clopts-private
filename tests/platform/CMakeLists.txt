set(clopts_platform_test_sources
    platform_tests_main.c
    test_util.c
    test_split.c
    test_argv.c
    test_env.c
    test_oom.c
)
# The Windows-only suites (registered under #ifdef _WIN32 in platform_tests_main.c).
if(WIN32)
    list(APPEND clopts_platform_test_sources test_convert.c test_crt.c)
endif()

clopts_add_module_tests(platform SOURCES ${clopts_platform_test_sources})
