/* Good: decl may include base. clopts_getenv_fn is not a call of getenv. */
#include "base/mem.h"
#include "decl/scope.h"

static clopts_getenv_fn names_fn;
