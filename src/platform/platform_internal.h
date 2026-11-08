#ifndef CLOPTS_PLATFORM_INTERNAL_H
#define CLOPTS_PLATFORM_INTERNAL_H

#include "clopts/platform.h"
#include "core/core_internal.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Internal declarations of the platform module, shared by its source files
 * (platform_alloc.c, platform_split.c, platform_argv.c, platform_env.c and
 * platform_win32.c or platform_posix.c) and its tests (tests/platform/).
 * None of these symbols is exported from the shared library.
 */

/* One UTF-16 code unit. */
#ifdef _WIN32
typedef wchar_t clopts_platform_u16_t;   /* 16-bit on Windows */
#else
typedef uint16_t clopts_platform_u16_t;
#endif

/*
 * Allocation probe. When a probe is given, every allocation through the
 * gateway below increments calls and, when fail_at != 0 and
 * calls >= fail_at, fails without allocating; live counts the blocks
 * currently allocated.
 */
typedef struct clopts_platform_probe
{
    size_t fail_at;  /* 0: never fail; otherwise the calls-th and later allocations fail */
    size_t calls;    /* allocations attempted */
    size_t live;     /* blocks currently allocated */
} clopts_platform_probe_t;

/* argv handle. Never changes after it is created. */
struct clopts_argv
{
    clopts_argv_status_t status;
    int argc;
    int error_index;        /* -1 unless INVALID_UTF16 */
    char *const *argv;      /* argv[argc] == NULL; caller's array, same block, or the empty array */
};

/* Environment lookup context. */
struct clopts_env
{
    clopts_platform_probe_t *probe;   /* NULL in production */
    clopts_env_error_t error;
    char *error_name;                 /* NULL or owned copy */
    char *value;                      /* UTF-8 value buffer (Windows) */
    size_t value_cap;
    void *wide;                       /* UTF-16 work buffer (Windows): name and value */
    size_t wide_cap;                  /* in clopts_platform_u16_t units */
};

/*
 * Allocation gateway (platform_alloc.c). Without a probe these only call
 * malloc / free. A size of 0 is allocated as 1 byte, so NULL always means
 * failure.
 */
void *clopts_platform_malloc(clopts_platform_probe_t *probe, size_t size);
/* count * size with clopts_core_size_mul; NULL on overflow without allocating. */
void *clopts_platform_malloc_array(clopts_platform_probe_t *probe, size_t count, size_t size);
void clopts_platform_free(clopts_platform_probe_t *probe, void *ptr);  /* NULL is ignored */

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
clopts_argv_t *clopts_platform_argv_failure(clopts_platform_probe_t *probe, clopts_argv_status_t status,
                                            int error_index);
/* A handle that borrows argv (count argc); the sentinel when the allocation fails. */
clopts_argv_t *clopts_platform_argv_passthrough(clopts_platform_probe_t *probe, int argc, char *const *argv);
/* Releases args through probe (NULL and the sentinel are ignored). clopts_argv_free() passes NULL. */
void clopts_platform_argv_free(clopts_platform_probe_t *probe, clopts_argv_t *args);

/* Environment lookup contexts (platform_env.c). */

clopts_env_t *clopts_platform_env_new(clopts_platform_probe_t *probe);  /* clopts_env_new() passes NULL */
/* Clears the failure record (error NONE, error_name released). Called first by every lookup. */
void clopts_platform_env_clear(clopts_env_t *env);
/* Records kind and a copy of name; when the copy fails, records OUT_OF_MEMORY with error_name NULL. */
void clopts_platform_env_fail(clopts_env_t *env, clopts_env_error_t kind, const char *name);
/* Grows env->value to at least bytes (never shrinks). 0 on success, -1 on overflow or allocation failure. */
int clopts_platform_env_reserve_value(clopts_env_t *env, size_t bytes);
/* Grows env->wide to at least units code units. Same return values. */
int clopts_platform_env_reserve_wide(clopts_env_t *env, size_t units);

#ifdef _WIN32
/* Windows argv conversion (platform_win32.c). */

/* Splits cmdline and converts every element to UTF-8; CMDLINE_UNAVAILABLE when cmdline is NULL. */
clopts_argv_t *clopts_platform_argv_from_cmdline(clopts_platform_probe_t *probe, const wchar_t *cmdline);
/* Converts wargs[0..argc-1] to UTF-8 in one block; INVALID_UTF16 with the index of the first bad element. */
clopts_argv_t *clopts_platform_argv_convert(clopts_platform_probe_t *probe, int argc, const wchar_t *const *wargs);
#endif

#endif /* CLOPTS_PLATFORM_INTERNAL_H */
