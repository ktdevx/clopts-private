<!-- Self-test input of check_docs.cmake MODE=api_coverage
     (SYMBOLS=symbols.txt). Fails with 3 failures, every missing name listed:
     "symbols.txt:4: not documented in missing.md: clopts_alpha" (only a longer
     name contains it), "symbols.txt:6: not documented in missing.md: clopts_beta"
     (not in backticks) and
     "symbols.txt:9: not documented in missing.md: clopts_gamma" (absent). -->
# API

- `clopts_alpha_get()` returns the value.
- clopts_beta is mentioned without backticks, and so is `clopts_beta(x)`.
