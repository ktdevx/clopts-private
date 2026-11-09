<!-- Self-test input of check_docs.cmake MODE=sources. Fails with 4 failures:
     "malformed.md:8: malformed marker: the next line is not a fence of 3 or more backticks",
     "malformed.md:12: malformed marker: no file after 'clopts-source:'",
     "malformed.md:16: malformed marker: the marker does not end with '-->' on the same line",
     "malformed.md:19: malformed marker: the code block is not closed". -->
# Malformed markers

<!-- clopts-source: hello.c -->

    int main(void) { return 0; }

<!-- clopts-source: -->
```c
```

<!-- clopts-source: hello.c

-->
<!-- clopts-source: hello.c -->
```c
#include <stdio.h>
