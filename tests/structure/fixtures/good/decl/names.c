/* Good: decl may include base. clopts_getenv_fn is not a call of getenv. */
#include "base/mem.h"
#include "decl/decl_internal.h"

static clopts_getenv_fn names_fn;
