/* Allowed: a struct definition (a public struct is frozen instead). */
#ifndef GOOD_STRUCT_DEF_H
#define GOOD_STRUCT_DEF_H

#include "clopts/export.h"

typedef struct good_layout
{
    int count;
    const char *name;
    void *(*make)(int count);
} good_layout_t;

CLOPTS_API void good_layout_use(const good_layout_t *layout);

#endif /* GOOD_STRUCT_DEF_H */
