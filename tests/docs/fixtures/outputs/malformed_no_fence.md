<!-- Self-test input (REQUIRE empty). Fails with
     "malformed_no_fence.md:5: malformed marker: the next line is not a fence of 3 or more backticks". -->
# Malformed marker

<!-- clopts-check: -- cmake -E echo hi -->

```
hi
```
