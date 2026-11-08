<!-- Self-test input of check_docs.cmake MODE=links. Links inside code spans
     and code blocks are not checked: the check succeeds with
     "docs_check: checked 2 link(s)" (target.md and sub/page.md). -->
# Links in code

In a code span: `[x](missing.md)`, in a double one: ``a ` [y](missing2.md)``,
and a span between two links: [ok](target.md) `![z](missing3.png)`.
An unmatched backtick does not open a span: ` is literal, so
[this](sub/page.md) is checked.

```markdown
[in a block](missing4.md)
```

~~~
![in a tilde block](missing5.png)
~~~
