/* Allowed: a static inline definition inside extern "C" (requirement 4.5). */
#ifndef GOOD_STATIC_INLINE_H
#define GOOD_STATIC_INLINE_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int good_file_write(void *ctx, const char *data, size_t len)
{
    if (len == 0)
    {
        return 0;
    }
    return (fwrite(data, 1, len, (FILE *)ctx) == len) ? 0 : 1;
}

static inline int good_twice(int x) { return x * 2; }

#ifdef __cplusplus
}
#endif

#endif /* GOOD_STATIC_INLINE_H */
