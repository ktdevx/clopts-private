<!-- Self-test input (REQUIRE empty). Fails with
     "stderr_mismatch.md:9: output of 'cmake -E cat missing.txt' differs
     (first difference at stderr_mismatch.md:11)" and "actual (stderr): ...".
     The cmake -E cat marker depends on the exact standard error text of
     `cmake -E cat missing.txt` and its exit code 1 (checked with CMake 3.28;
     it may change in other CMake versions). -->
# Standard error differs

<!-- clopts-check: exit=1 stream=stderr -- cmake -E cat missing.txt -->
```
CMake Error: other.txt: no such file or directory (ignoring)
```
