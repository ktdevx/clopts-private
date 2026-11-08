// Allowed: text in comments is not checked (requirement 4.6), and a
// CLOPTS_API declaration may continue over several lines.
#ifndef GOOD_COMMENTS_AND_MULTILINE_H
#define GOOD_COMMENTS_AND_MULTILINE_H

#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A layout that must stay private:
 *
 * struct good_private { int a; };
 * union good_bits { int i; float f; };
 * int good_unmarked(void);
 */

// struct good_line_comment { int a; };

typedef struct good_program good_program_t; /* struct x { } */

CLOPTS_API int good_multiline(good_program_t *program,
                              const char *name,
                              int flags);
CLOPTS_API
good_program_t *
good_return_type_on_its_own_line(void);

#ifdef __cplusplus
}
#endif

#endif /* GOOD_COMMENTS_AND_MULTILINE_H */
