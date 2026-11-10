/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_SRC_PROCESS_ARGV_IMPL_H
#define CLOPTS_SRC_PROCESS_ARGV_IMPL_H

#include "clopts/process/argv.h"

/*
 * The implementation of the argv handle for the files that implement it:
 * argv.c makes and reads handles, and argv_posix.c and argv_win32.c create
 * them from the process (clopts_argv_create). Only files of the process
 * module include it (docs/dev/design.md, chapter 3). Every
 * allocation goes through the allocation gateway (base/alloc.h).
 */

/* argv handle. Never changes after it is created. */
struct clopts_argv
{
    clopts_argv_status_t status;
    int argc;
    int error_index;        /* -1 unless INVALID_UTF16 */
    char *const *argv;      /* argv[argc] == NULL; caller's array, same block, or the empty array */
};

/* The shared out-of-memory sentinel (static, never written, never freed). */
clopts_argv_t *clopts_argv_sentinel(void);
/* A handle with status (not OK), count 0 and error_index; the sentinel when the allocation fails. */
clopts_argv_t *clopts_argv_failure(clopts_argv_status_t status, int error_index);
/* A handle that borrows argv (count argc); the sentinel when the allocation fails. */
clopts_argv_t *clopts_argv_passthrough(int argc, char *const *argv);

#ifdef _WIN32
/* Windows (argv_win32.c). */

/* Splits cmdline and converts every element to UTF-8; CMDLINE_UNAVAILABLE when cmdline is NULL. */
clopts_argv_t *clopts_argv_from_cmdline(const wchar_t *cmdline);
/* Converts wargs[0..argc-1] to UTF-8 in one block; INVALID_UTF16 with the index of the first bad element. */
clopts_argv_t *clopts_argv_convert(int argc, const wchar_t *const *wargs);
#endif

#endif /* CLOPTS_SRC_PROCESS_ARGV_IMPL_H */
