# Self-test input of check_changelog.cmake MODE=release (not a real project):
# the version does not match the tag v0.2.0, and valid/CHANGELOG.md has no
# section for 0.3.0.
project(clopts VERSION 0.3.0 LANGUAGES C)
