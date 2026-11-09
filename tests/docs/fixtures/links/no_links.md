<!-- Self-test input of check_docs.cmake MODE=links. Fails with 1 failure:
     "no relative link found in: no_links.md" (requirement 8.8). -->
# No relative links
Only [external](https://example.com), [mail](mailto:a@example.com),
[anchor](#no-relative-links) and `[code](target.md)`.
