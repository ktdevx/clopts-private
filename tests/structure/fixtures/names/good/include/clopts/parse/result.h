/* Good: a public struct and its public operations. */
typedef struct clopts_result clopts_result_t;

typedef enum clopts_source
{
    CLOPTS_SOURCE_NONE
} clopts_source_t;

CLOPTS_API int clopts_result_get_status(const clopts_result_t *result);
