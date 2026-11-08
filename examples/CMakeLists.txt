# CLOpts examples (CLOPTS_BUILD_EXAMPLES). Each one is also registered for the
# docs check through clopts_add_doc_program (cmake/CloptsExamples.cmake).

clopts_add_doc_program(taskbook SOURCES taskbook/taskbook.c ENV TASKBOOK_FORMAT EXAMPLE)
clopts_add_doc_program(textcount SOURCES textcount/textcount.c ENV TEXTCOUNT_ENCODING EXAMPLE)
