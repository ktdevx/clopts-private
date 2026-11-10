/*
 * Good: only parse.c of parser may include process.
 * #include "text/text_internal.h" and getenv("X") in a comment are ignored.
 */
#include "base/mem.h"
#include "decl/scope.h"
#include "parser/parser_internal.h"
#include "process/env.h"   // #include <windows.h> after // is ignored
