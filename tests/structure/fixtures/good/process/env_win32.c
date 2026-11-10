/* Good: process may touch the OS. */
#include "process/env_internal.h"

#include <windows.h>

static const char *win32_value(void)
{
    return getenv("X");
}
