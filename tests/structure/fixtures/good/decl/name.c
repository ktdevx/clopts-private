/* Good: decl may include base. An identifier holding getenv is not a call of getenv. */
#include "base/alloc_internal.h"
#include "decl/scope_internal.h"

static int (*names_getenv_fn)(const char *name);
