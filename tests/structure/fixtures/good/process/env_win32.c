/* Good: process may touch the OS. */
#include "process/env.h"

#include <windows.h>

static const char *win32_value(void)
{
    return getenv("X");
}
