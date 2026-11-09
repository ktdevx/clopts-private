<!-- Self-test input (REQUIRE empty). Fails with
     "malformed_env_without_equals.md:5: malformed marker: env 'CLOPTS_SELFTEST' has no '='". -->
# Malformed marker

<!-- clopts-check: env=CLOPTS_SELFTEST -- cmake -E echo hi -->
```
hi
```
