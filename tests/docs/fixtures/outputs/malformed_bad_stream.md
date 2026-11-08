<!-- Self-test input (REQUIRE empty). Fails with
     "malformed_bad_stream.md:5: malformed marker: stream 'both' is not stdout or stderr". -->
# Malformed marker

<!-- clopts-check: stream=both -- cmake -E echo hi -->
```
hi
```
