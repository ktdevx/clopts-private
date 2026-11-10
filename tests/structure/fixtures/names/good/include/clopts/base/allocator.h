/* Good: the public function is defined in base/allocator.c. */
#define CLOPTS_API
CLOPTS_API void clopts_set_allocator(const void *allocator);
typedef int (*clopts_ignored_fn)(void);
