/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite crt (Windows only; built only on Windows): starts the probe
 * executable with raw command lines and compares clopts_argv_create with the
 * CRT's __argc / __argv there.
 *
 * It also checks that a command line built with test_util can be compared
 * with what the CRT split gives, element by element.
 */

#include "test_util.h"

#include "process/argv_internal.h"
#include "process/utf16.h"

#include <naos/naos.h>

#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>

NAOS_TEST(u16_from_ascii_builds_a_wide_command_line)
{
    process_test_u16_t cmdline;
    process_test_u16_t a;
    process_test_u16_t b;
    clopts_utf16_t *args[2];
    static const char *const expected[] = {"p", "foo\"bar", NULL};

    process_test_u16_from_ascii(&cmdline, "p \"foo\"\"bar\"");
    NAOS_CHECK_INT_EQ(wcscmp(cmdline.units, L"p \"foo\"\"bar\""), 0);

    args[0] = process_test_u16_from_ascii(&a, "p");
    args[1] = process_test_u16_from_ascii(&b, "foo\"bar");
    NAOS_CHECK_INT_EQ(process_test_split_mismatch(args, 2, expected), -1);
}

/* ---- The probe in a child process ----------------------------------------- */

/* Path of process_probe.exe, passed by CMake as $<TARGET_FILE:process_probe>. */
#ifndef PROCESS_PROBE_PATH
#error "PROCESS_PROBE_PATH must be defined (see tests/process/CMakeLists.txt)"
#endif

/* Time a probe run may take (wine starts processes slowly). */
#define PROCESS_CRT_TIMEOUT_MS 120000u

/*
 * Starts the probe with the raw command line cmdline (ASCII) and returns its
 * exit code: 0 match, 1 mismatch, 2 clopts_argv_create failed. Returns -1 when
 * the probe could not be started or did not finish in time.
 */
static long process_crt_run_probe(const char *cmdline)
{
    wchar_t path[MAX_PATH * 4];
    process_test_u16_t line;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    DWORD code;
    long result;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, PROCESS_PROBE_PATH, -1, path,
                            (int)(sizeof(path) / sizeof(path[0]))) == 0)
    {
        return -1;
    }
    /* CreateProcessW may write to the command line buffer; line is writable. */
    process_test_u16_from_ascii(&line, cmdline);

    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    memset(&process, 0, sizeof(process));
    if (!CreateProcessW(path, line.units, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
    {
        return -1;
    }
    result = -1;
    if ((WaitForSingleObject(process.hProcess, PROCESS_CRT_TIMEOUT_MS) == WAIT_OBJECT_0) &&
        GetExitCodeProcess(process.hProcess, &code))
    {
        result = (long)code;
    }
    else
    {
        TerminateProcess(process.hProcess, 3u);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

/*
 * One test per raw command line of the design's table (ASCII, no
 * wildcards), so that a failing row is named by its test. <TAB> is a tab.
 */
#define PROCESS_CRT_ROW(name, cmdline)                       \
    NAOS_TEST(name)                                           \
    {                                                         \
        NAOS_CHECK_INT_EQ(process_crt_run_probe(cmdline), 0); \
    }

PROCESS_CRT_ROW(probe_matches_doubled_quote_inside_quotes, "p \"foo\"\"bar\"")
PROCESS_CRT_ROW(probe_matches_odd_backslashes_before_quote, "p a\\\\\\\"b")
PROCESS_CRT_ROW(probe_matches_even_backslashes_before_quote, "p a\\\\\\\\\"b c\"")
PROCESS_CRT_ROW(probe_matches_trailing_backslash, "p a\\")
PROCESS_CRT_ROW(probe_matches_leading_space, " p x")
PROCESS_CRT_ROW(probe_matches_empty_quoted_argument, "p \"\"")
PROCESS_CRT_ROW(probe_matches_unterminated_quote, "p \"a b")
PROCESS_CRT_ROW(probe_matches_quoted_program_name, "\"C:\\x y\\p\"q z")
PROCESS_CRT_ROW(probe_matches_tab_separator, "p a\tb")
PROCESS_CRT_ROW(probe_matches_repeated_doubled_quotes, "p \"a\"\"b\"\"c\"")
PROCESS_CRT_ROW(probe_matches_trailing_spaces, "p x   ")

NAOS_SUITE(crt,
    NAOS_ADD_TEST(u16_from_ascii_builds_a_wide_command_line),
    NAOS_ADD_TEST(probe_matches_doubled_quote_inside_quotes),
    NAOS_ADD_TEST(probe_matches_odd_backslashes_before_quote),
    NAOS_ADD_TEST(probe_matches_even_backslashes_before_quote),
    NAOS_ADD_TEST(probe_matches_trailing_backslash),
    NAOS_ADD_TEST(probe_matches_leading_space),
    NAOS_ADD_TEST(probe_matches_empty_quoted_argument),
    NAOS_ADD_TEST(probe_matches_unterminated_quote),
    NAOS_ADD_TEST(probe_matches_quoted_program_name),
    NAOS_ADD_TEST(probe_matches_tab_separator),
    NAOS_ADD_TEST(probe_matches_repeated_doubled_quotes),
    NAOS_ADD_TEST(probe_matches_trailing_spaces)
)
