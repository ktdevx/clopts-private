/* Allowed: typedef enums and function pointer types (requirement 4.5). */
#ifndef GOOD_TYPEDEFS_H
#define GOOD_TYPEDEFS_H

#include <stddef.h>

#include "clopts/export.h"

typedef enum good_status
{
    GOOD_OK = 0, /* { an unbalanced brace in a comment */
    GOOD_FAILED = 1 // } and another one
} good_status_t;

enum good_plain
{
    GOOD_PLAIN = 0
};

typedef int (*good_write_fn)(void *ctx, const char *data, size_t len);
typedef good_status_t (*good_lookup_fn)(void *ctx,
                                        const char *name);

#define GOOD_MACRO(x) \
    struct good_macro_struct { int x; }; \
    int good_macro_fn(int)

CLOPTS_API good_status_t good_lookup(good_lookup_fn lookup, void *ctx);

#endif /* GOOD_TYPEDEFS_H */
