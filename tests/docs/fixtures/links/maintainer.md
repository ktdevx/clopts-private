<!-- Self-test input of check_docs.cmake MODE=links. Fails with 4 failures:
     "maintainer.md:6: link to maintainer docs: docs/dev/notes.md",
     "maintainer.md:7: link to maintainer docs: ./docs/dev/" and
     "maintainer.md:8: link to maintainer docs: docs/dev/missing.md" (the
     target is also reported as "maintainer.md:8: broken link: docs/dev/missing.md"). -->
[notes](docs/dev/notes.md)
[dev](./docs/dev/)
[missing](docs/dev/missing.md)
[docs](docs) is fine.
