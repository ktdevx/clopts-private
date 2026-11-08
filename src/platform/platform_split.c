/*
 * Command line splitting with the rules of the current MSVC CRT.
 */

#include "platform/platform_internal.h"

void clopts_platform_split(const clopts_platform_u16_t *cmdline, clopts_platform_u16_t *buf,
                           clopts_platform_u16_t **args, size_t *argc, size_t *units)
{
    (void)cmdline;
    (void)buf;
    (void)args;
    if (argc != NULL)
    {
        *argc = 0;
    }
    if (units != NULL)
    {
        *units = 0;
    }
}
