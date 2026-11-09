<!-- Self-test input (REQUIRE empty). Fails with exactly 1 failure on 1 line:
     "malformed_semicolon.md:5: malformed marker: ';' is not supported in a marker". -->
# Malformed marker

<!-- clopts-check: -- cmake -E echo "a;b" -->
```
a;b
```
