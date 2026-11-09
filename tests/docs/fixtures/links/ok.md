<!-- Self-test input of check_docs.cmake MODE=links. Every relative link
     resolves: the check succeeds with "docs_check: checked 6 link(s)". -->
# Links that resolve

A file, [a directory](sub), a directory with a slash [again](sub/) and a file
with an anchor: [target](target.md), [page](sub/page.md#section).
An image: ![image](image.svg "a title") and a link in angle brackets:
[angle](<target.md>).

Skipped: [web](https://example.com/missing.md), [plain](http://example.com),
[mail](mailto:someone@example.com) and [anchor](#links-that-resolve).
