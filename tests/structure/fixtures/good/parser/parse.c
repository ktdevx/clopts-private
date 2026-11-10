/*
 * Good: only parse.c of parser may include process.
 * #include "text/text_internal.h" and getenv("X") in a comment are ignored.
 */
#include "core/core_internal.h"
#include "decl/decl_internal.h"
#include "parser/parser_internal.h"
#include "process/process_internal.h"   // #include <windows.h> after // is ignored
