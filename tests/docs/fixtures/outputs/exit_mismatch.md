<!-- Self-test input (REQUIRE empty). The output matches but the exit code does
     not: fails with "exit_mismatch.md:5: exit code 0 (expected 3)". -->
# Exit code differs

<!-- clopts-check: exit=3 -- cmake -E echo hi -->
```
hi
```
