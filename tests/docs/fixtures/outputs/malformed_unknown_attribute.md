<!-- Self-test input (REQUIRE empty). Fails with
     "malformed_unknown_attribute.md:5: malformed marker: unknown attribute 'code=0'". -->
# Malformed marker

<!-- clopts-check: code=0 -- cmake -E echo hi -->
```
hi
```
