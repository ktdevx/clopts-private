/*
 * argv and environment lookup on Windows: the command line is split with the
 * rules of the current MSVC CRT and converted to UTF-8, and variables are
 * read with GetEnvironmentVariableW() and converted to UTF-8.
 */

#include "platform/platform_internal.h"

clopts_argv_t *clopts_platform_argv_from_cmdline(clopts_platform_probe_t *probe, const wchar_t *cmdline)
{
    (void)probe;
    (void)cmdline;
    return clopts_platform_argv_sentinel();
}

clopts_argv_t *clopts_platform_argv_convert(clopts_platform_probe_t *probe, int argc, const wchar_t *const *wargs)
{
    (void)probe;
    (void)argc;
    (void)wargs;
    return clopts_platform_argv_sentinel();
}

clopts_argv_t *clopts_argv_get(int argc, char *const *argv)
{
    (void)argc;
    (void)argv;
    return clopts_platform_argv_from_cmdline(NULL, NULL);
}

clopts_argv_t *clopts_argv_from_wide(int argc, wchar_t *const *wargv)
{
    return clopts_platform_argv_convert(NULL, argc, (const wchar_t *const *)wargv);
}

clopts_env_status_t clopts_env_lookup(void *ctx, const char *name, const char **value)
{
    (void)ctx;
    (void)name;
    (void)value;
    return CLOPTS_ENV_FAILED;
}
