/*
 * Good: parse may include process.
 * #include "text/out.h" and getenv("X") in a comment are ignored.
 */
#include "base/alloc_internal.h"
#include "decl/lookup.h"
#include "parse/matches.h"
#include "process/env_internal.h"   // #include <windows.h> after // is ignored
