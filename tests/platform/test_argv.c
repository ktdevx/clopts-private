/*
 * Suite argv: argv handles, their accessors and release, the NULL handle
 * and the out-of-memory sentinel, the pass-through outside Windows and the
 * match with the CRT's __argv on Windows.
 *
 * It also checks the ASCII array helper of test_util.
 */

#include "test_util.h"

#include "platform/platform_internal.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdlib.h>   /* __argc / __argv on Windows */
#include <string.h>

NAOS_TEST(u16_from_ascii_copies_each_byte_and_terminates)
{
    platform_test_u16_t s;
    const char *text = "a \"b\\c\td";
    clopts_platform_u16_t *units;
    size_t i;

    units = platform_test_u16_from_ascii(&s, text);
    NAOS_CHECK_PTR_EQ(units, s.units);
    NAOS_CHECK_UINT_EQ(s.len, strlen(text));
    for (i = 0; i < s.len; ++i)
    {
        NAOS_CHECK_UINT_EQ(s.units[i], (unsigned char)text[i]);
    }
    NAOS_CHECK_UINT_EQ(s.units[s.len], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(units, text));
}

NAOS_TEST(u16_from_ascii_accepts_the_empty_string)
{
    platform_test_u16_t s;

    platform_test_u16_from_ascii(&s, "");
    NAOS_CHECK_UINT_EQ(s.len, 0);
    NAOS_CHECK_UINT_EQ(s.units[0], 0);
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, ""));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "a"));
}

NAOS_TEST(u16_eq_ascii_rejects_prefixes_and_longer_strings)
{
    platform_test_u16_t s;

    platform_test_u16_from_ascii(&s, "abc");
    NAOS_CHECK_TRUE(platform_test_u16_eq_ascii(s.units, "abc"));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "ab"));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "abcd"));
    NAOS_CHECK_FALSE(platform_test_u16_eq_ascii(s.units, "abd"));
}

/* ---- all OS: sentinel, NULL handle, failure and pass-through handles ---- */

/* Checks the accessors of a handle that reads like the out-of-memory sentinel. */
static void platform_argv_check_oom(const clopts_argv_t *args)
{
    char *const *values;

    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
    values = clopts_argv_values(args);
    NAOS_CHECK_NOT_NULL(values);
    if (values != NULL)
    {
        NAOS_CHECK_NULL(values[0]);
    }
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
}

NAOS_TEST(null_handle_reads_like_the_sentinel)
{
    platform_argv_check_oom(NULL);
}

NAOS_TEST(sentinel_accessors)
{
    clopts_argv_t *sentinel = clopts_platform_argv_sentinel();

    NAOS_CHECK_NOT_NULL(sentinel);
    NAOS_CHECK_PTR_EQ(clopts_platform_argv_sentinel(), sentinel);
    platform_argv_check_oom(sentinel);
}

NAOS_TEST(free_ignores_null_and_the_sentinel)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    clopts_argv_t *sentinel = clopts_platform_argv_sentinel();

    clopts_argv_free(NULL);
    clopts_argv_free(sentinel);
    clopts_platform_argv_free(&probe, NULL);
    clopts_platform_argv_free(&probe, sentinel);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
    /* The sentinel still reads the same after the calls above. */
    platform_argv_check_oom(sentinel);
}

/* Creates a failure handle through a probe and checks its invariants. */
static void platform_argv_check_failure(clopts_argv_status_t status, int error_index, int expected_index)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };
    clopts_argv_t *args;
    char *const *values;

    args = clopts_platform_argv_failure(&probe, status, error_index);
    NAOS_CHECK_NOT_NULL(args);
    NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), status);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
    values = clopts_argv_values(args);
    NAOS_CHECK_NOT_NULL(values);
    if (values != NULL)
    {
        NAOS_CHECK_NULL(values[0]);
    }
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), expected_index);
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    clopts_platform_argv_free(&probe, args);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(failure_handle_invalid_utf16_keeps_the_index)
{
    platform_argv_check_failure(CLOPTS_ARGV_INVALID_UTF16, 0, 0);
    platform_argv_check_failure(CLOPTS_ARGV_INVALID_UTF16, 3, 3);
}

NAOS_TEST(failure_handle_cmdline_unavailable_has_no_index)
{
    platform_argv_check_failure(CLOPTS_ARGV_CMDLINE_UNAVAILABLE, -1, -1);
}

/* Failure handles share the empty values array; the public free releases them. */
NAOS_TEST(failure_handles_share_the_empty_array)
{
    clopts_argv_t *a;
    clopts_argv_t *b;

    a = clopts_platform_argv_failure(NULL, CLOPTS_ARGV_INVALID_UTF16, 1);
    b = clopts_platform_argv_failure(NULL, CLOPTS_ARGV_CMDLINE_UNAVAILABLE, -1);
    NAOS_CHECK_TRUE(a != clopts_platform_argv_sentinel());
    NAOS_CHECK_TRUE(b != clopts_platform_argv_sentinel());
    NAOS_CHECK_PTR_EQ(clopts_argv_values(a), clopts_argv_values(b));
    NAOS_CHECK_PTR_EQ(clopts_argv_values(a), clopts_argv_values(NULL));
    clopts_argv_free(a);
    clopts_argv_free(b);
}

NAOS_TEST(failure_allocation_failure_returns_the_sentinel)
{
    clopts_platform_probe_t probe = { 1, 0, 0 };
    clopts_argv_t *args;

    args = clopts_platform_argv_failure(&probe, CLOPTS_ARGV_INVALID_UTF16, 2);
    NAOS_CHECK_PTR_EQ(args, clopts_platform_argv_sentinel());
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    platform_argv_check_oom(args);
    clopts_platform_argv_free(&probe, args);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

/* An OUT_OF_MEMORY failure is the sentinel itself, without an allocation. */
NAOS_TEST(failure_out_of_memory_is_the_sentinel)
{
    clopts_platform_probe_t probe = { 0, 0, 0 };

    NAOS_CHECK_PTR_EQ(clopts_platform_argv_failure(&probe, CLOPTS_ARGV_OUT_OF_MEMORY, -1),
                      clopts_platform_argv_sentinel());
    NAOS_CHECK_UINT_EQ(probe.calls, 0);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(passthrough_borrows_every_pointer)
{
    char a0[] = "tool";
    char a1[] = "--name";
    char a2[] = "";
    char a3[] = "\xe3\x83\x86";
    char *argv[5];
    clopts_platform_probe_t probe = { 0, 0, 0 };
    clopts_argv_t *args;
    char *const *values;
    int i;

    argv[0] = a0;
    argv[1] = a1;
    argv[2] = a2;
    argv[3] = a3;
    argv[4] = NULL;
    args = clopts_platform_argv_passthrough(&probe, 4, argv);
    NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 4);
    values = clopts_argv_values(args);
    NAOS_CHECK_PTR_EQ(values, argv);
    if (values == argv)
    {
        for (i = 0; i < 4; ++i)
        {
            NAOS_CHECK_PTR_EQ(values[i], argv[i]);
        }
        NAOS_CHECK_NULL(values[4]);
    }
    /* Neither the array nor the strings are written. */
    NAOS_CHECK_STR_EQ(a0, "tool");
    NAOS_CHECK_STR_EQ(a1, "--name");
    NAOS_CHECK_PTR_EQ(argv[3], a3);
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    clopts_platform_argv_free(&probe, args);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
    NAOS_CHECK_PTR_EQ(argv[0], a0);
}

NAOS_TEST(passthrough_with_argc_zero)
{
    char *empty[1] = { NULL };
    clopts_platform_probe_t probe = { 0, 0, 0 };
    clopts_argv_t *args;
    char *const *values;

    args = clopts_platform_argv_passthrough(&probe, 0, empty);
    NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
    NAOS_CHECK_PTR_EQ(clopts_argv_values(args), empty);
    clopts_platform_argv_free(&probe, args);
    NAOS_CHECK_UINT_EQ(probe.live, 0);

    args = clopts_platform_argv_passthrough(&probe, 0, NULL);
    NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
    values = clopts_argv_values(args);
    NAOS_CHECK_NOT_NULL(values);
    if (values != NULL)
    {
        NAOS_CHECK_NULL(values[0]);
    }
    NAOS_CHECK_UINT_EQ(probe.live, 1);
    clopts_platform_argv_free(&probe, args);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
}

NAOS_TEST(passthrough_allocation_failure_returns_the_sentinel)
{
    char a0[] = "tool";
    char *argv[2];
    clopts_platform_probe_t probe = { 1, 0, 0 };
    clopts_argv_t *args;

    argv[0] = a0;
    argv[1] = NULL;
    args = clopts_platform_argv_passthrough(&probe, 1, argv);
    NAOS_CHECK_PTR_EQ(args, clopts_platform_argv_sentinel());
    NAOS_CHECK_UINT_EQ(probe.calls, 1);
    platform_argv_check_oom(args);
    clopts_platform_argv_free(&probe, args);
    NAOS_CHECK_UINT_EQ(probe.live, 0);
    NAOS_CHECK_PTR_EQ(argv[0], a0);
    NAOS_CHECK_NULL(argv[1]);
}

/* The public release frees a pass-through handle (checked by leak tools) without touching argv. */
NAOS_TEST(public_free_releases_a_passthrough_handle)
{
    char a0[] = "tool";
    char *argv[2];
    clopts_argv_t *args;

    argv[0] = a0;
    argv[1] = NULL;
    args = clopts_platform_argv_passthrough(NULL, 1, argv);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 1);
    clopts_argv_free(args);
    NAOS_CHECK_STR_EQ(argv[0], "tool");
}

#ifndef _WIN32
/* ---- outside Windows: clopts_argv_get passes argv through ---------------- */

NAOS_TEST(argv_get_returns_the_callers_argv_unchanged)
{
    char a0[] = "tool";
    char a1[] = "--name";
    char a2[] = "";
    char a3[] = "\xe3\x83\x86\xff";
    char *argv[5];
    clopts_argv_t *args;
    char *const *values;
    int i;

    argv[0] = a0;
    argv[1] = a1;
    argv[2] = a2;
    argv[3] = a3;
    argv[4] = NULL;
    args = clopts_argv_get(4, argv);
    NAOS_CHECK_NOT_NULL(args);
    NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 4);
    values = clopts_argv_values(args);
    /* neither the array nor the strings are copied or converted */
    NAOS_CHECK_PTR_EQ(values, argv);
    if (values == argv)
    {
        for (i = 0; i < 4; ++i)
        {
            NAOS_CHECK_PTR_EQ(values[i], argv[i]);
        }
        NAOS_CHECK_NULL(values[4]);
    }
    NAOS_CHECK_STR_EQ(a3, "\xe3\x83\x86\xff");
    clopts_argv_free(args);
    NAOS_CHECK_PTR_EQ(argv[0], a0);
    NAOS_CHECK_STR_EQ(argv[1], "--name");
}

NAOS_TEST(argv_get_with_argc_zero)
{
    char *empty[1] = { NULL };
    clopts_argv_t *args;
    char *const *values;

    args = clopts_argv_get(0, empty);
    NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
    NAOS_CHECK_PTR_EQ(clopts_argv_values(args), empty);
    clopts_argv_free(args);

    /* argv may be NULL when argc is 0; values is still a NULL-terminated array */
    args = clopts_argv_get(0, NULL);
    NAOS_CHECK_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_count(args), 0);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
    values = clopts_argv_values(args);
    NAOS_CHECK_NOT_NULL(values);
    if (values != NULL)
    {
        NAOS_CHECK_NULL(values[0]);
    }
    clopts_argv_free(args);
}
#else /* _WIN32 */
/* ---- Windows: clopts_argv_get splits the process command line ------------- */

/*
 * The CRT's __argc / __argv. MinGW-w64 declares them in <stdlib.h> even with
 * the language extensions off (-std=c99, __STRICT_ANSI__): as macros over
 * __MINGW_IMP_SYMBOL / __p___argv on msvcrt and over __p___argc /
 * __p___argv on UCRT. MSVC's UCRT declares the accessor functions but may
 * hide the __argc / __argv macros when the non-standard names are disabled
 * (/std:c11 without extensions), so when the macros are missing they are
 * defined here over the accessors, which every CRT that lacks the macros
 * exports. The probe of suite crt reuses this workaround.
 */

#ifndef __argc
int *__cdecl __p___argc(void);
#define __argc (*__p___argc())
#endif
#ifndef __argv
char ***__cdecl __p___argv(void);
#define __argv (*__p___argv())
#endif

/*
 * Checks args against the CRT's view of the same process. Only __argc and
 * __argv[0] are compared: the test runner (Naos) removes its own options from
 * main's argv in place, and with MSVC's UCRT main's argv is the very array
 * __argv points to, so __argv[1..] no longer holds the command line here
 * (MinGW hands main a copy, which hides this). Every element is compared with
 * the CRT in a fresh process by the probe of suite crt (9.3).
 */
static void platform_argv_check_matches_crt(const clopts_argv_t *args)
{
    char *const *values;
    int i;

    NAOS_REQUIRE_TRUE(args != clopts_platform_argv_sentinel());
    NAOS_CHECK_INT_EQ(clopts_argv_status(args), CLOPTS_ARGV_OK);
    NAOS_CHECK_INT_EQ(clopts_argv_error_index(args), -1);
    NAOS_REQUIRE_INT_EQ(clopts_argv_count(args), __argc);
    values = clopts_argv_values(args);
    NAOS_REQUIRE_NOT_NULL(values);
    for (i = 0; i < __argc; ++i)
    {
        NAOS_REQUIRE_NOT_NULL(values[i]);
    }
    NAOS_REQUIRE_NOT_NULL(__argv[0]);
    NAOS_CHECK_STR_EQ(values[0], __argv[0]);
    NAOS_CHECK_NULL(values[__argc]);
}

/* CTest starts the test executable with ASCII arguments only (9.3). */
NAOS_TEST(argv_get_matches_the_crt_argv)
{
    clopts_argv_t *args;

    NAOS_REQUIRE_TRUE(__argc >= 1);
    args = clopts_argv_get(0, NULL);
    platform_argv_check_matches_crt(args);
    clopts_argv_free(args);
}

/* The argc / argv given to clopts_argv_get are not read on Windows. */
NAOS_TEST(argv_get_ignores_the_given_argv)
{
    char a0[] = "not-the-program";
    char *argv[2];
    clopts_argv_t *args;

    argv[0] = a0;
    argv[1] = NULL;
    args = clopts_argv_get(1, argv);
    platform_argv_check_matches_crt(args);
    NAOS_CHECK_TRUE(clopts_argv_values(args) != argv);
    clopts_argv_free(args);
    NAOS_CHECK_STR_EQ(a0, "not-the-program");
}
#endif /* _WIN32 */

/* Preprocessor directives cannot appear inside the macro arguments, so each OS has its own list. */
#ifndef _WIN32
NAOS_SUITE(argv,
    NAOS_ADD_TEST(u16_from_ascii_copies_each_byte_and_terminates),
    NAOS_ADD_TEST(u16_from_ascii_accepts_the_empty_string),
    NAOS_ADD_TEST(u16_eq_ascii_rejects_prefixes_and_longer_strings),
    NAOS_ADD_TEST(null_handle_reads_like_the_sentinel),
    NAOS_ADD_TEST(sentinel_accessors),
    NAOS_ADD_TEST(free_ignores_null_and_the_sentinel),
    NAOS_ADD_TEST(failure_handle_invalid_utf16_keeps_the_index),
    NAOS_ADD_TEST(failure_handle_cmdline_unavailable_has_no_index),
    NAOS_ADD_TEST(failure_handles_share_the_empty_array),
    NAOS_ADD_TEST(failure_allocation_failure_returns_the_sentinel),
    NAOS_ADD_TEST(failure_out_of_memory_is_the_sentinel),
    NAOS_ADD_TEST(passthrough_borrows_every_pointer),
    NAOS_ADD_TEST(passthrough_with_argc_zero),
    NAOS_ADD_TEST(passthrough_allocation_failure_returns_the_sentinel),
    NAOS_ADD_TEST(public_free_releases_a_passthrough_handle),
    NAOS_ADD_TEST(argv_get_returns_the_callers_argv_unchanged),
    NAOS_ADD_TEST(argv_get_with_argc_zero)
)
#else
NAOS_SUITE(argv,
    NAOS_ADD_TEST(u16_from_ascii_copies_each_byte_and_terminates),
    NAOS_ADD_TEST(u16_from_ascii_accepts_the_empty_string),
    NAOS_ADD_TEST(u16_eq_ascii_rejects_prefixes_and_longer_strings),
    NAOS_ADD_TEST(null_handle_reads_like_the_sentinel),
    NAOS_ADD_TEST(sentinel_accessors),
    NAOS_ADD_TEST(free_ignores_null_and_the_sentinel),
    NAOS_ADD_TEST(failure_handle_invalid_utf16_keeps_the_index),
    NAOS_ADD_TEST(failure_handle_cmdline_unavailable_has_no_index),
    NAOS_ADD_TEST(failure_handles_share_the_empty_array),
    NAOS_ADD_TEST(failure_allocation_failure_returns_the_sentinel),
    NAOS_ADD_TEST(failure_out_of_memory_is_the_sentinel),
    NAOS_ADD_TEST(passthrough_borrows_every_pointer),
    NAOS_ADD_TEST(passthrough_with_argc_zero),
    NAOS_ADD_TEST(passthrough_allocation_failure_returns_the_sentinel),
    NAOS_ADD_TEST(public_free_releases_a_passthrough_handle),
    NAOS_ADD_TEST(argv_get_matches_the_crt_argv),
    NAOS_ADD_TEST(argv_get_ignores_the_given_argv)
)
#endif
