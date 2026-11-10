/* Good: decl may include base. An identifier holding getenv is not a call of getenv. */
#include "base/mem.h"
#include "decl/scope.h"

static int (*names_getenv_fn)(const char *name);
