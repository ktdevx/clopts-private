<!-- Self-test input of check_docs.cmake MODE=sources. Fails with 1 failure:
     "missing_source.md:5: source file not found: missing.c". -->
# A source file that does not exist

<!-- clopts-source: missing.c -->
```c
int main(void) { return 0; }
```
