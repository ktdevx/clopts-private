<!-- Self-test input of check_docs.cmake MODE=sources. Every marker matches:
     the check succeeds with "docs_check: checked 2 source marker(s)". -->
# Matching sources

<!-- clopts-source: hello.c -->
```c
#include <stdio.h>

int main(void)
{
    puts("hello");
    return 0;
}
```

Lines with ";", brackets and backslashes, in a longer fence; trailing
whitespace after the marker is ignored:

<!-- clopts-source: special.c -->   
````c
/* Characters CMake lists treat specially: ; [x] \n */
static const char *items[2] = { "a;b", "c\\d" };
````

A marker inside a fenced code block is an example of the syntax and is not
checked:

````markdown
<!-- clopts-source: missing.c -->
```c
```
````
