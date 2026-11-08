# Documentation tests (label: docs) are registered here.
#
# Programs referenced by the documentation are created with
# clopts_add_doc_program (cmake/CloptsExamples.cmake); the examples under
# examples/ are registered before this directory is processed.

clopts_add_doc_program(hello SOURCES readme_minimal.c)
