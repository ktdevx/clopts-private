<!-- Self-test input (REQUIRE empty). The program prints no newline at the end
     but every line of a block ends with one: fails with "first difference at
     no_trailing_newline.md:8" and "(no newline at end)". -->
# No newline at the end of the output

<!-- clopts-check: -- cmake -E echo_append no-newline -->
```
no-newline
```
