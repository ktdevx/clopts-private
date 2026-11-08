<!-- Self-test input of check_docs.cmake MODE=sources. Fails with 1 failure:
     "mismatch.md:6: code block differs from hello.c (first difference at line 5)"
     followed by the two differing lines. -->
# A changed line

<!-- clopts-source: hello.c -->
```c
#include <stdio.h>

int main(void)
{
    puts("hello, world");
    return 0;
}
```
