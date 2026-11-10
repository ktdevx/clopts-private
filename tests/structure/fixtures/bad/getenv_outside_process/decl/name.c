/* Bad: only process may read the environment. */
#include "decl/scope.h"

static const char *names_value(void)
{
    return getenv("X");
}
