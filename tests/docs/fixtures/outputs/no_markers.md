<!-- Self-test input (REQUIRE empty). The only marker is inside a code block,
     so no marker is checked: fails with "no clopts-check marker found". -->
# No markers

````markdown
<!-- clopts-check: -- cmake -E echo hello -->
```
hello
```
````
