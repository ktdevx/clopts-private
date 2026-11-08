<!-- Self-test input of check_docs.cmake MODE=sources. Fails with 2 failures:
     "mismatch_length.md:8: code block differs from hello.c (first difference at line 7)"
     (the block ends early) and
     "mismatch_length.md:18: code block differs from no_newline.c (first difference at line 2)"
     (the file does not end with a newline, every line of a block does). -->
# Different lengths

<!-- clopts-source: hello.c -->
```c
#include <stdio.h>

int main(void)
{
    puts("hello");
    return 0;
```

<!-- clopts-source: no_newline.c -->
```c
int answer(void) { return 42; }
/* no newline at end */
```
