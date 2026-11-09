<!-- Self-test input (REQUIRE empty). Every failure is reported: fails with
     "docs_check (outputs): 3 failure(s)" (one output difference, one exit code
     and one malformed marker), and the matching marker in between passes. -->
# All failures are collected

<!-- clopts-check: -- cmake -E echo one -->
```
two
```

<!-- clopts-check: -- cmake -E echo same -->
```
same
```

<!-- clopts-check: exit=2 -- cmake -E echo three -->
```
three
```

<!-- clopts-check: stream=stdin -- cmake -E echo four -->
```
four
```
