/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#ifndef CLOPTS_PLATFORM_INTERNAL_H
#define CLOPTS_PLATFORM_INTERNAL_H

#include "clopts/env.h"
#include "clopts/platform.h"
#include "core/core_internal.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Internal declarations of the platform module, shared by its source files
 * (platform_split.c, platform_argv.c, platform_env.c and platform_win32.c
 * or platform_posix.c), its tests (tests/platform/) and the parser (the
 * process environment lookup). None of these
 * symbols is exported from the shared library. Every allocation goes through
 * the allocation gateway of core (clopts_core_malloc and the others).
 */

/* One UTF-16 code unit. */
#ifdef _WIN32
typedef wchar_t clopts_platform_u16_t;   /* 16-bit on Windows */
#else
typedef uint16_t clopts_platform_u16_t;
#endif

/* argv handle. Never changes after it is created. */
struct clopts_argv
{
    clopts_argv_status_t status;
    int argc;
    int error_index;        /* -1 unless INVALID_UTF16 */
    char *const *argv;      /* argv[argc] == NULL; caller's array, same block, or the empty array */
};

/*
 * Context of the process environment lookup: the buffers of the values it
 * lends. One parse owns one (on its stack) for the duration of the
 * resolution, so parses never share one. Nothing is allocated until a
 * lookup needs it, and nothing at all elsewhere than Windows.
 */
typedef struct clopts_platform_env
{
    char *value;                      /* UTF-8 value buffer (Windows) */
    size_t value_cap;
    void *wide;                       /* UTF-16 work buffer (Windows): name and value */
    size_t wide_cap;                  /* in clopts_platform_u16_t units */
} clopts_platform_env_t;

/*
 * Command line splitting with the rules of the current MSVC CRT
 * (platform_split.c). Pure: never allocates and never calls the OS.
 *
 * Splits cmdline (NUL-terminated). Always stores the element count in *argc
 * (>= 1) and the code units needed for all elements including one NUL each
 * in *units. When buf is not NULL it must hold *units code units from a
 * previous counting call, args must hold *argc pointers, and each args[i] is
 * set to the NUL-terminated element i inside buf. Never fails.
 */
void clopts_platform_split(const clopts_platform_u16_t *cmdline, clopts_platform_u16_t *buf,
                           clopts_platform_u16_t **args, size_t *argc, size_t *units);

/* argv handles (platform_argv.c). */

/* The shared out-of-memory sentinel (static, never written, never freed). */
clopts_argv_t *clopts_platform_argv_sentinel(void);
/* A handle with status (not OK), count 0 and error_index; the sentinel when the allocation fails. */
clopts_argv_t *clopts_platform_argv_failure(clopts_argv_status_t status, int error_index);
/* A handle that borrows argv (count argc); the sentinel when the allocation fails. */
clopts_argv_t *clopts_platform_argv_passthrough(int argc, char *const *argv);

/* Process environment lookup (platform_env.c, and platform_win32.c or platform_posix.c). */

/* Makes env empty: no buffer. */
void clopts_platform_env_init(clopts_platform_env_t *env);
/* Releases the buffers of env, leaving it empty. Every value it lent becomes invalid. */
void clopts_platform_env_release(clopts_platform_env_t *env);
/* Grows env->value to at least bytes (never shrinks). 0 on success, -1 on overflow or allocation failure. */
int clopts_platform_env_reserve_value(clopts_platform_env_t *env, size_t bytes);
/* Grows env->wide to at least units code units. Same return values. */
int clopts_platform_env_reserve_wide(clopts_platform_env_t *env, size_t units);

/*
 * The lookup of clopts_parse() in the process environment, a
 * clopts_getenv_fn whose ctx is a clopts_platform_env_t *. Stores the UTF-8
 * value of name in *value: on Windows read with GetEnvironmentVariableW() and
 * converted into env->value (valid until the next lookup with env or its
 * release); elsewhere getenv()'s pointer as is. Returns:
 *   - CLOPTS_ENV_FOUND, an empty value included;
 *   - CLOPTS_ENV_UNSET, also for a name that cannot be converted to UTF-16
 *     (no variable can have it; the builder rejects such names anyway);
 *   - CLOPTS_ENV_INVALID: Windows, the value is not valid UTF-16;
 *   - CLOPTS_ENV_FAILED: an allocation or the system call failed.
 */
clopts_env_status_t clopts_platform_env_lookup(void *ctx, const char *name, const char **value);

#ifdef _WIN32
/* Windows argv conversion (platform_win32.c). */

/* Splits cmdline and converts every element to UTF-8; CMDLINE_UNAVAILABLE when cmdline is NULL. */
clopts_argv_t *clopts_platform_argv_from_cmdline(const wchar_t *cmdline);
/* Converts wargs[0..argc-1] to UTF-8 in one block; INVALID_UTF16 with the index of the first bad element. */
clopts_argv_t *clopts_platform_argv_convert(int argc, const wchar_t *const *wargs);
#endif

#endif /* CLOPTS_PLATFORM_INTERNAL_H */
