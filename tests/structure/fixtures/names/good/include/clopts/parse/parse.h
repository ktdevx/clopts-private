/*
 * Good: a public function outside a struct, and a function named after an
 * enumeration of another header (an enumeration is not a struct).
 */
#include "clopts/parse/result.h"

CLOPTS_API clopts_result_t *clopts_parse(int argc);
CLOPTS_API const char *clopts_source_name(clopts_source_t source);
