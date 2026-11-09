<!-- Self-test input of check_docs.cmake MODE=sources. Fails with 1 failure:
     "no clopts-source marker found in: no_markers.md". The
     marker below is inside a code block and is not counted. -->
# No source marker

```markdown
<!-- clopts-source: hello.c -->
```
