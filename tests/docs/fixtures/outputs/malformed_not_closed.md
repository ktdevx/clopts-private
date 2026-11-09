<!-- Self-test input (REQUIRE empty). Fails with
     "malformed_not_closed.md:5: malformed marker: the marker does not end with '-->' on the same line". -->
# Malformed marker

<!-- clopts-check: -- cmake -E echo hi
-->
```
hi
```
