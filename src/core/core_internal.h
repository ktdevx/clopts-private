#ifndef CLOPTS_CORE_INTERNAL_H
#define CLOPTS_CORE_INTERNAL_H

#include <stddef.h>

/*
 * Overflow-checked size arithmetic.
 *
 * Both functions return 0 and store the result in *out on success. On
 * overflow they return -1 and leave *out unchanged. out must not be NULL.
 */
int clopts_core_size_mul(size_t a, size_t b, size_t *out);
int clopts_core_size_add(size_t a, size_t b, size_t *out);

#endif /* CLOPTS_CORE_INTERNAL_H */
