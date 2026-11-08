<!-- Self-test input (REQUIRE empty). Fails with
     "stdout_mismatch.md:8: output of 'cmake -E echo hello world' differs
     (first difference at stdout_mismatch.md:11)". -->
# Standard output differs

The second line of the block is not what the program prints.

<!-- clopts-check: -- cmake -E echo hello world -->
```
hello world
and more
```
