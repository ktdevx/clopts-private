/* Bad: a struct definition puts its layout into the caller's binary. */
#include "clopts/export.h"

typedef struct bad_handle bad_handle_t;

typedef struct bad_layout
{
    int count;
    const char *name;
} bad_layout_t;

CLOPTS_API void bad_use(bad_handle_t *handle);
