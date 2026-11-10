/* Allowed: opaque types declared without a body. */
#ifndef GOOD_FORWARD_DECL_H
#define GOOD_FORWARD_DECL_H

#include "clopts/base/export.h"

typedef struct good_handle good_handle_t;
struct good_other;
typedef struct good_split
    good_split_t;

CLOPTS_API good_handle_t *good_handle_new(void);
CLOPTS_API void good_handle_free(good_handle_t *handle);

#endif /* GOOD_FORWARD_DECL_H */
