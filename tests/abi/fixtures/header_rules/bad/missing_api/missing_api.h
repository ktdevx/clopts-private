#ifndef BAD_MISSING_API_H
#define BAD_MISSING_API_H

/* Bad: a function declared without CLOPTS_API. (The file starts with
 * preprocessor lines, so the reported line also checks that blank lines at
 * the top are counted.) */
#include "clopts/base/export.h"

CLOPTS_API int bad_marked(void);
int bad_unmarked(const char *name);

#endif /* BAD_MISSING_API_H */
