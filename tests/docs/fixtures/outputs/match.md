<!-- Self-test input of check_docs.cmake MODE=outputs (PROGRAMS=cmake=<cmake>,
     REQUIRE=cmake). Every marker matches: the check succeeds with
     "docs_check: checked 6 output marker(s)".
     The cmake -E cat marker depends on the exact standard error text of
     `cmake -E cat missing.txt` and its exit code 1 (checked with CMake 3.28;
     it may change in other CMake versions). -->
# Matching markers

Standard output with the default attributes:

<!-- clopts-check: -- cmake -E echo hello -->
```console
hello
```

Standard error and a nonzero exit code (the work directory is empty):

<!-- clopts-check: exit=1 stream=stderr -- cmake -E cat missing.txt -->
```console
CMake Error: missing.txt: no such file or directory (ignoring)
```

An empty block means no output; attributes may come in any order and env= may
repeat:

<!-- clopts-check: env=CLOPTS_SELFTEST_A=1 stream=stdout env=CLOPTS_SELFTEST_B=x exit=0 -- cmake -E true -->
```
```

Quoted arguments keep their spaces, and a longer fence may hold a shorter one:

<!-- clopts-check: -- cmake -E echo "a  b" c -->
````text
a  b c
````

<!-- clopts-check: -- cmake -E echo ``` -->
````
```
````

Trailing whitespace after a marker is ignored:

<!-- clopts-check: -- cmake -E echo trailing-space -->   
```
trailing-space
```

A marker inside a fenced code block is an example of the syntax and is not
checked:

````markdown
<!-- clopts-check: -- cmake -E echo this would not match -->
```
something else
```
````
