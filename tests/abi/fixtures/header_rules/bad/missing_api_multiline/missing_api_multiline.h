/* Bad: a function declared without CLOPTS_API, over several lines. */
#include <stddef.h>

#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bad_program bad_program_t;

const char *
bad_unmarked_multiline(bad_program_t *program,
                       size_t index);

#ifdef __cplusplus
}
#endif
