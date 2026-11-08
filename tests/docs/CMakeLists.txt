# Documentation tests (label: docs) are registered here.
#
# Programs referenced by the documentation are created with
# clopts_add_doc_program (cmake/CloptsExamples.cmake); the examples under
# examples/ are registered before this directory is processed.
#
# Self-tests of the check scripts (requirement 8.1, design "DocsTests"):
#
#   docs_selftest.outputs.<case>       check_docs.cmake MODE=outputs on
#                                      fixtures/outputs/<case>.md, with cmake
#                                      itself as the only program
#   docs_selftest.sources.<case>       MODE=sources on fixtures/sources/
#   docs_selftest.links.<case>         MODE=links on fixtures/links/
#   docs_selftest.api_coverage.<case>  MODE=api_coverage on
#                                      fixtures/api_coverage/
#   docs_selftest.<mode>.missing_document
#                                      a document that does not exist
#   docs_selftest.changelog.<case>     check_changelog.cmake MODE=format and
#                                      MODE=release on fixtures/changelog/
#   docs_selftest.changelog.release_ok / release_last_section
#                                      run_release_selftest.cmake: the release
#                                      succeeds, prints "version X.Y.Z" and
#                                      writes the notes of
#                                      fixtures/changelog/expected_X.Y.Z.md
#
# Every other self-test uses PASS_REGULAR_EXPRESSION built from the lines of
# the expected report, so that it passes only when exactly that report is
# printed (a failure for any other reason does not match). Each fixture states
# its expected result in a header comment.

clopts_add_doc_program(hello SOURCES readme_minimal.c)

set(_clopts_docs_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(_clopts_docs_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
# Each self-test writes only below its own directory here, so that parallel
# runs do not collide.
set(_clopts_docs_selftest_work "${CMAKE_CURRENT_BINARY_DIR}/selftest")

# _clopts_docs_add_selftest(<case> <script> [ARGS <-Dvar=value>...]
#                           [EXPECT <line>...])
#
# Registers docs_selftest.<case>, which runs tests/docs/<script> with
# `cmake -P`. EXPECT gives the lines the script must print, in order, as
# literal text (without the one-space indentation of a report line): each one
# must be a whole line of the output; other lines may come between them. The
# first EXPECT line is the summary ("docs_check (<mode>): <n> failure(s)" or
# the success message), which fixes the number of failures. A ";" cannot be
# written in an EXPECT line (it separates the arguments): write <semicolon>,
# which matches any one character. Without EXPECT the exit code decides.
function(_clopts_docs_add_selftest case script)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "ARGS;EXPECT")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "_clopts_docs_add_selftest(${case}): unknown arguments '${ARG_UNPARSED_ARGUMENTS}' "
            "(see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    set(name "docs_selftest.${case}")
    add_test(NAME "${name}"
        COMMAND "${CMAKE_COMMAND}" ${ARG_ARGS} -P "${_clopts_docs_dir}/${script}")
    set_tests_properties("${name}" PROPERTIES LABELS docs)
    if(ARG_EXPECT)
        set(regex "")
        set(separator "(^|\n)")
        foreach(line IN LISTS ARG_EXPECT)
            string(REGEX REPLACE "([][\\\\.*+?^$|()])" "\\\\\\1" line "${line}")
            string(REPLACE "<semicolon>" "." line "${line}")
            string(APPEND regex "${separator} *${line}\r?\n")
            set(separator "(.*\n)?")
        endforeach()
        set_tests_properties("${name}" PROPERTIES PASS_REGULAR_EXPRESSION "${regex}")
    endif()
endfunction()

# _clopts_docs_add_check_selftest(<mode> <case> [FILES <file>...]
#                                 [ARGS <-Dvar=value>...] EXPECT <line>...)
#
# docs_selftest.<mode>.<case>: check_docs.cmake MODE=<mode> with
# SOURCE_DIR=fixtures/<mode> and FILES (default <case>.md). MODE=outputs also
# gets PROGRAMS=cmake=<this cmake> and its own WORK directory.
function(_clopts_docs_add_check_selftest mode case)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "FILES;ARGS;EXPECT")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "_clopts_docs_add_check_selftest(${mode} ${case}): unknown arguments "
            "'${ARG_UNPARSED_ARGUMENTS}' (see ${CMAKE_CURRENT_LIST_FILE})")
    endif()
    if(NOT ARG_FILES)
        set(ARG_FILES "${case}.md")
    endif()
    string(REPLACE ";" "\\;" files "${ARG_FILES}")
    set(args
        "-DMODE=${mode}"
        "-DSOURCE_DIR=${_clopts_docs_fixtures}/${mode}"
        "-DFILES=${files}")
    if(mode STREQUAL "outputs")
        list(APPEND args
            "-DPROGRAMS=cmake=${CMAKE_COMMAND}"
            "-DWORK=${_clopts_docs_selftest_work}/outputs.${case}")
    endif()
    _clopts_docs_add_selftest("${mode}.${case}" check_docs.cmake
        ARGS ${args} ${ARG_ARGS}
        EXPECT ${ARG_EXPECT})
endfunction()

# ---- check_docs.cmake MODE=outputs (requirements 8.2-8.5, 8.8) -------------
set(_clopts_docs_out "${_clopts_docs_fixtures}/outputs")

_clopts_docs_add_check_selftest(outputs match
    ARGS "-DREQUIRE=cmake"
    EXPECT "-- docs_check: checked 6 output marker(s)")

# The expected and actual output of a mismatch are written below WORK.
set(_clopts_docs_files "${_clopts_docs_selftest_work}/outputs.stdout_mismatch/outputs/stdout_mismatch.md-8")
_clopts_docs_add_check_selftest(outputs stdout_mismatch
    EXPECT
        "docs_check (outputs): 1 failure(s)"
        "stdout_mismatch.md:8: output of 'cmake -E echo hello world' differs (first difference at stdout_mismatch.md:11)"
        "stdout_mismatch.md:8: expected (document): and more"
        "stdout_mismatch.md:8: actual (stdout): <end of output>"
        "stdout_mismatch.md:8: files: ${_clopts_docs_files}.expected ${_clopts_docs_files}.actual")

set(_clopts_docs_files "${_clopts_docs_selftest_work}/outputs.stderr_mismatch/outputs/stderr_mismatch.md-9")
_clopts_docs_add_check_selftest(outputs stderr_mismatch
    EXPECT
        "docs_check (outputs): 1 failure(s)"
        "stderr_mismatch.md:9: output of 'cmake -E cat missing.txt' differs (first difference at stderr_mismatch.md:11)"
        "stderr_mismatch.md:9: expected (document): CMake Error: other.txt: no such file or directory (ignoring)"
        "stderr_mismatch.md:9: actual (stderr): CMake Error: missing.txt: no such file or directory (ignoring)"
        "stderr_mismatch.md:9: files: ${_clopts_docs_files}.expected ${_clopts_docs_files}.actual")

_clopts_docs_add_check_selftest(outputs no_trailing_newline
    EXPECT
        "docs_check (outputs): 1 failure(s)"
        "no_trailing_newline.md:6: output of 'cmake -E echo_append no-newline' differs (first difference at no_trailing_newline.md:8)"
        "no_trailing_newline.md:6: expected (document): no-newline"
        "no_trailing_newline.md:6: actual (stdout): no-newline (no newline at end)")

_clopts_docs_add_check_selftest(outputs exit_mismatch
    EXPECT
        "docs_check (outputs): 1 failure(s)"
        "exit_mismatch.md:5: exit code 0 (expected 3)")

# Every failure is collected, the matching marker between them is not one.
_clopts_docs_add_check_selftest(outputs collect_all
    EXPECT
        "docs_check (outputs): 3 failure(s)"
        "collect_all.md:6: output of 'cmake -E echo one' differs (first difference at collect_all.md:8)"
        "collect_all.md:6: expected (document): two"
        "collect_all.md:6: actual (stdout): one"
        "collect_all.md:16: exit code 0 (expected 2)"
        "collect_all.md:21: malformed marker: stream 'stdin' is not stdout or stderr")

# Malformed markers: fixtures/outputs/<case>.md, reported at line 5.
foreach(_clopts_docs_case_reason IN ITEMS
        "malformed_bad_stream|stream 'both' is not stdout or stderr"
        "malformed_env_empty_value|env 'CLOPTS_SELFTEST=' has an empty value"
        "malformed_env_without_equals|env 'CLOPTS_SELFTEST' has no '='"
        "malformed_exit_not_integer|exit 'zero' is not an integer"
        "malformed_no_fence|the next line is not a fence of 3 or more backticks"
        "malformed_no_program|no program after '--'"
        "malformed_no_separator|no '--' before the program"
        "malformed_not_closed|the marker does not end with '-->' on the same line"
        "malformed_semicolon|'<semicolon>' is not supported in a marker"
        "malformed_unclosed_block|the code block is not closed"
        "malformed_unknown_attribute|unknown attribute 'code=0'"
        "unknown_program|unknown program 'textcount' (registered: cmake)")
    string(FIND "${_clopts_docs_case_reason}" "|" _clopts_docs_bar)
    string(SUBSTRING "${_clopts_docs_case_reason}" 0 ${_clopts_docs_bar} _clopts_docs_case)
    math(EXPR _clopts_docs_bar "${_clopts_docs_bar} + 1")
    string(SUBSTRING "${_clopts_docs_case_reason}" ${_clopts_docs_bar} -1 _clopts_docs_reason)
    _clopts_docs_add_check_selftest(outputs "${_clopts_docs_case}"
        EXPECT
            "docs_check (outputs): 1 failure(s)"
            "${_clopts_docs_case}.md:5: malformed marker: ${_clopts_docs_reason}")
endforeach()

_clopts_docs_add_check_selftest(outputs no_markers
    EXPECT
        "docs_check (outputs): 1 failure(s)"
        "no clopts-check marker found in: no_markers.md")

_clopts_docs_add_check_selftest(outputs require_missing
    ARGS "-DREQUIRE=cmake"
    EXPECT
        "docs_check (outputs): 1 failure(s)"
        "cmake: no output marker with a nonzero exit code")

_clopts_docs_add_check_selftest(outputs missing_document
    FILES no_such.md
    EXPECT
        "docs_check (outputs): 2 failure(s)"
        "no_such.md: file not found (${_clopts_docs_out}/no_such.md)"
        "no clopts-check marker found in: no_such.md")

# ---- check_docs.cmake MODE=sources (requirements 8.6, 8.8) -----------------
_clopts_docs_add_check_selftest(sources match
    EXPECT "-- docs_check: checked 2 source marker(s)")

_clopts_docs_add_check_selftest(sources mismatch
    EXPECT
        "docs_check (sources): 1 failure(s)"
        "mismatch.md:6: code block differs from hello.c (first difference at line 5)"
        "mismatch.md:6: document:     puts(\"hello, world\")<semicolon>"
        "mismatch.md:6: file:     puts(\"hello\")<semicolon>")

_clopts_docs_add_check_selftest(sources mismatch_length
    EXPECT
        "docs_check (sources): 2 failure(s)"
        "mismatch_length.md:8: code block differs from hello.c (first difference at line 7)"
        "mismatch_length.md:8: document: <end of block>"
        "mismatch_length.md:8: file: }"
        "mismatch_length.md:18: code block differs from no_newline.c (first difference at line 2)"
        "mismatch_length.md:18: document: /* no newline at end */"
        "mismatch_length.md:18: file: /* no newline at end */ (no newline at end)")

_clopts_docs_add_check_selftest(sources missing_source
    EXPECT
        "docs_check (sources): 1 failure(s)"
        "missing_source.md:5: source file not found: missing.c")

_clopts_docs_add_check_selftest(sources malformed
    EXPECT
        "docs_check (sources): 4 failure(s)"
        "malformed.md:8: malformed marker: the next line is not a fence of 3 or more backticks"
        "malformed.md:12: malformed marker: no file after 'clopts-source:'"
        "malformed.md:16: malformed marker: the marker does not end with '-->' on the same line"
        "malformed.md:19: malformed marker: the code block is not closed")

_clopts_docs_add_check_selftest(sources no_markers
    EXPECT
        "docs_check (sources): 1 failure(s)"
        "no clopts-source marker found in: no_markers.md")

_clopts_docs_add_check_selftest(sources missing_document
    FILES no_such.md
    EXPECT
        "docs_check (sources): 2 failure(s)"
        "no_such.md: file not found (${_clopts_docs_fixtures}/sources/no_such.md)"
        "no clopts-source marker found in: no_such.md")

# ---- check_docs.cmake MODE=links (requirements 6.2, 6.5, 8.8) --------------
_clopts_docs_add_check_selftest(links ok
    EXPECT "-- docs_check: checked 6 link(s)")

# Links inside code spans and code blocks are ignored.
_clopts_docs_add_check_selftest(links code
    EXPECT "-- docs_check: checked 2 link(s)")

_clopts_docs_add_check_selftest(links broken
    EXPECT
        "docs_check (links): 3 failure(s)"
        "broken.md:6: broken link: missing.md"
        "broken.md:7: broken link: sub/missing.png"
        "broken.md:7: broken link: nowhere/#section")

_clopts_docs_add_check_selftest(links maintainer
    EXPECT
        "docs_check (links): 4 failure(s)"
        "maintainer.md:6: link to maintainer docs: docs/dev/notes.md"
        "maintainer.md:7: link to maintainer docs: ./docs/dev/"
        "maintainer.md:8: link to maintainer docs: docs/dev/missing.md"
        "maintainer.md:8: broken link: docs/dev/missing.md")

# Links are relative to the document, which may be in a subdirectory.
_clopts_docs_add_check_selftest(links guide
    FILES docs/guide.md
    EXPECT
        "docs_check (links): 1 failure(s)"
        "docs/guide.md:5: link to maintainer docs: dev/notes.md")

_clopts_docs_add_check_selftest(links no_links
    EXPECT
        "docs_check (links): 1 failure(s)"
        "no relative link found in: no_links.md")

_clopts_docs_add_check_selftest(links missing_document
    FILES no_such.md
    EXPECT
        "docs_check (links): 2 failure(s)"
        "no_such.md: file not found (${_clopts_docs_fixtures}/links/no_such.md)"
        "no relative link found in: no_such.md")

# ---- check_docs.cmake MODE=api_coverage (requirements 2.9, 8.7, 8.8) -------
_clopts_docs_add_check_selftest(api_coverage complete
    ARGS "-DSYMBOLS=symbols.txt"
    EXPECT "-- docs_check: checked 4 symbol(s)")

# Every missing name is listed.
_clopts_docs_add_check_selftest(api_coverage missing
    ARGS "-DSYMBOLS=symbols.txt"
    EXPECT
        "docs_check (api_coverage): 3 failure(s)"
        "symbols.txt:4: not documented in missing.md: clopts_alpha"
        "symbols.txt:6: not documented in missing.md: clopts_beta"
        "symbols.txt:9: not documented in missing.md: clopts_gamma")

_clopts_docs_add_check_selftest(api_coverage comments_only
    FILES complete.md
    ARGS "-DSYMBOLS=comments_only.txt"
    EXPECT
        "docs_check (api_coverage): 1 failure(s)"
        "no symbol found in: comments_only.txt")

_clopts_docs_add_check_selftest(api_coverage no_such_list
    FILES complete.md
    ARGS "-DSYMBOLS=no_such_list.txt"
    EXPECT
        "docs_check (api_coverage): 1 failure(s)"
        "no_such_list.txt: file not found (${_clopts_docs_fixtures}/api_coverage/no_such_list.txt)")

_clopts_docs_add_check_selftest(api_coverage missing_document
    FILES no_such.md
    ARGS "-DSYMBOLS=symbols.txt"
    EXPECT
        "docs_check (api_coverage): 1 failure(s)"
        "no_such.md: file not found (${_clopts_docs_fixtures}/api_coverage/no_such.md)")

# ---- check_changelog.cmake (requirements 10.2-10.4, 11.2-11.4) -------------
set(_clopts_docs_cl "${_clopts_docs_fixtures}/changelog")
set(_clopts_docs_categories "互換性のない変更, 追加, 変更, 非推奨, 削除, 修正, セキュリティ")

# _clopts_docs_add_changelog_selftest(<case> <changelog dir> EXPECT <line>...)
#
# docs_selftest.changelog.<case>: MODE=format on
# fixtures/changelog/<changelog dir>/CHANGELOG.md.
function(_clopts_docs_add_changelog_selftest case dir)
    _clopts_docs_add_selftest("changelog.${case}" check_changelog.cmake
        ARGS "-DMODE=format" "-DCHANGELOG=${_clopts_docs_cl}/${dir}/CHANGELOG.md"
        ${ARGN})
endfunction()

_clopts_docs_add_changelog_selftest(format_valid valid
    EXPECT "-- changelog: 2 release section(s)")

_clopts_docs_add_changelog_selftest(not_unreleased_first not_unreleased_first
    EXPECT
        "changelog_check (format): 2 failure(s)"
        "CHANGELOG.md:7: first '## ' heading must be '## [Unreleased]' (found '## [0.2.0] - 2027-01-15')"
        "CHANGELOG.md:12: '## [Unreleased]' must be the first '## ' heading")

_clopts_docs_add_changelog_selftest(duplicate_unreleased duplicate_unreleased
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md:9: '## [Unreleased]' must be the first '## ' heading")

_clopts_docs_add_changelog_selftest(bad_heading bad_heading
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md:7: malformed heading '## 0.2.0 - 2027-01-15' (expected '## [Unreleased]' or '## [X.Y.Z] - YYYY-MM-DD')")

_clopts_docs_add_changelog_selftest(bad_date bad_date
    EXPECT
        "changelog_check (format): 2 failure(s)"
        "CHANGELOG.md:7: malformed date '2027/01/15' (expected YYYY-MM-DD)"
        "CHANGELOG.md:11: malformed date '2026-13-01' (expected YYYY-MM-DD)")

_clopts_docs_add_changelog_selftest(wrong_order wrong_order
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md:9: version 0.10.0 is newer than 0.9.0 at line 7 (sections must be newest first)")

_clopts_docs_add_changelog_selftest(duplicate_version duplicate_version
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md:11: duplicate version 0.2.0 (also at line 7)")

_clopts_docs_add_changelog_selftest(unknown_category unknown_category
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md:9: unknown category 'その他' (expected one of: ${_clopts_docs_categories})")

_clopts_docs_add_changelog_selftest(breaking_not_first breaking_not_first
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md:13: '互換性のない変更' must be the first category of its section")

_clopts_docs_add_changelog_selftest(no_headings no_headings
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md:1: no '## [Unreleased]' heading (no '## ' heading found)")

# Every failure is collected.
_clopts_docs_add_changelog_selftest(collect_all collect_all
    EXPECT
        "changelog_check (format): 4 failure(s)"
        "CHANGELOG.md:7: malformed date '2027-1-15' (expected YYYY-MM-DD)"
        "CHANGELOG.md:9: unknown category '雑記' (expected one of: ${_clopts_docs_categories})"
        "CHANGELOG.md:11: version 0.2.0 is newer than 0.1.0 at line 7 (sections must be newest first)"
        "CHANGELOG.md:13: duplicate version 0.2.0 (also at line 11)")

# fixtures/changelog/missing does not exist.
_clopts_docs_add_changelog_selftest(missing missing
    EXPECT
        "changelog_check (format): 1 failure(s)"
        "CHANGELOG.md: file not found (${_clopts_docs_cl}/missing/CHANGELOG.md)")

# MODE=release on a valid input: run_release_selftest.cmake checks the exit
# code, the "version X.Y.Z" line and the extracted notes.
_clopts_docs_add_selftest(changelog.release_ok run_release_selftest.cmake
    ARGS
        "-DTAG=v0.2.0"
        "-DSOURCE_DIR=${_clopts_docs_cl}/valid"
        "-DOUTPUT=${_clopts_docs_selftest_work}/changelog.release_ok/notes.md"
        "-DEXPECTED=${_clopts_docs_cl}/expected_0.2.0.md"
        "-DVERSION=0.2.0")

# The last section of the file ends at the end of the file.
_clopts_docs_add_selftest(changelog.release_last_section run_release_selftest.cmake
    ARGS
        "-DTAG=v0.1.0"
        "-DSOURCE_DIR=${_clopts_docs_cl}/project_0.1.0"
        "-DCHANGELOG=${_clopts_docs_cl}/valid/CHANGELOG.md"
        "-DOUTPUT=${_clopts_docs_selftest_work}/changelog.release_last_section/notes.md"
        "-DEXPECTED=${_clopts_docs_cl}/expected_0.1.0.md"
        "-DVERSION=0.1.0")

# _clopts_docs_add_release_selftest(<case> <tag> <project dir> [<changelog>]
#                                   EXPECT <line>...)
#
# docs_selftest.changelog.<case>: MODE=release that is expected to fail.
# <changelog> is a fixture directory (default: the CHANGELOG.md of
# <project dir>).
function(_clopts_docs_add_release_selftest case tag project)
    cmake_parse_arguments(PARSE_ARGV 3 ARG "" "" "EXPECT")
    set(args
        "-DMODE=release" "-DTAG=${tag}"
        "-DSOURCE_DIR=${_clopts_docs_cl}/${project}"
        "-DOUTPUT=${_clopts_docs_selftest_work}/changelog.${case}/notes.md")
    if(ARG_UNPARSED_ARGUMENTS)
        list(APPEND args "-DCHANGELOG=${_clopts_docs_cl}/${ARG_UNPARSED_ARGUMENTS}/CHANGELOG.md")
    endif()
    _clopts_docs_add_selftest("changelog.${case}" check_changelog.cmake
        ARGS ${args}
        EXPECT ${ARG_EXPECT})
endfunction()

_clopts_docs_add_release_selftest(release_version_mismatch v0.2.0 project_0.3.0 valid
    EXPECT
        "changelog_check (release): 1 failure(s)"
        "tag v0.2.0 does not match project VERSION 0.3.0")

_clopts_docs_add_release_selftest(release_no_section v0.3.0 project_0.3.0 valid
    EXPECT
        "changelog_check (release): 1 failure(s)"
        "no section for version 0.3.0 in CHANGELOG.md")

_clopts_docs_add_release_selftest(release_empty_section v0.2.0 empty_section
    EXPECT
        "changelog_check (release): 1 failure(s)"
        "section for version 0.2.0 is empty")

_clopts_docs_add_release_selftest(release_bad_tag 0.2.0 valid
    EXPECT
        "changelog_check (release): 1 failure(s)"
        "tag '0.2.0' is not of the form vX.Y.Z")

_clopts_docs_add_release_selftest(release_project_none v0.2.0 project_none valid
    EXPECT
        "changelog_check (release): 1 failure(s)"
        "CMakeLists.txt: expected exactly one 'project(clopts VERSION X.Y.Z', found 0 (${_clopts_docs_cl}/project_none/CMakeLists.txt)")

_clopts_docs_add_release_selftest(release_project_twice v0.2.0 project_twice valid
    EXPECT
        "changelog_check (release): 1 failure(s)"
        "CMakeLists.txt: expected exactly one 'project(clopts VERSION X.Y.Z', found 2 (${_clopts_docs_cl}/project_twice/CMakeLists.txt)")

# A format error fails the release too.
_clopts_docs_add_release_selftest(release_format_error v0.2.0 valid bad_heading
    EXPECT
        "changelog_check (release): 2 failure(s)"
        "CHANGELOG.md:7: malformed heading '## 0.2.0 - 2027-01-15' (expected '## [Unreleased]' or '## [X.Y.Z] - YYYY-MM-DD')"
        "no section for version 0.2.0 in CHANGELOG.md")
