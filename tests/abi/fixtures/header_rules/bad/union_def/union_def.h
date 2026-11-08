/* Bad: a union definition. */
#include "clopts/export.h"

#ifdef __cplusplus
extern "C" {
#endif

union bad_value { int i; double d; };

#ifdef __cplusplus
}
#endif
