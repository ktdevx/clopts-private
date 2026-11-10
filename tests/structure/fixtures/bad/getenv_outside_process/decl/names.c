/* Bad: only process may read the environment. */
#include "decl/decl_internal.h"

static const char *names_value(void)
{
    return getenv("X");
}
