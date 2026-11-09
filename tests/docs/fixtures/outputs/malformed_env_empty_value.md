<!-- Self-test input (REQUIRE empty). Fails with
     "malformed_env_empty_value.md:5: malformed marker: env 'CLOPTS_SELFTEST=' has an empty value". -->
# Malformed marker

<!-- clopts-check: env=CLOPTS_SELFTEST= -- cmake -E echo hi -->
```
hi
```
