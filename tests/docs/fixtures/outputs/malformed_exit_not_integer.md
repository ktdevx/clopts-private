<!-- Self-test input (REQUIRE empty). Fails with
     "malformed_exit_not_integer.md:5: malformed marker: exit 'zero' is not an integer". -->
# Malformed marker

<!-- clopts-check: exit=zero -- cmake -E echo hi -->
```
hi
```
