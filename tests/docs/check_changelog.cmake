# Checks the format of CHANGELOG.md and, for a release, matches the tag
# against the project version and extracts the release notes (requirements
# 10.2-10.4, 11.2-11.4). Run with `cmake -P`; MODE=format is registered by
# tests/docs/CMakeLists.txt (label: docs) and MODE=release is run by
# .github/workflows/release.yml.
#
# Variables (-D):
#   MODE        Always. One of:
#                 format   check the format of CHANGELOG.
#                 release  the format check, then the release checks below.
#   CHANGELOG   The changelog to read (a relative path is taken from the
#               current directory). Required for MODE=format; for
#               MODE=release it defaults to <SOURCE_DIR>/CHANGELOG.md.
#
#   MODE=release:
#   TAG         The release tag, of the form vX.Y.Z.
#   SOURCE_DIR  The source tree; <SOURCE_DIR>/CMakeLists.txt must contain
#               exactly one "project(clopts VERSION X.Y.Z" at the start of a
#               line (after indentation; a comment does not count) with X.Y.Z
#               equal to the tag's version.
#   OUTPUT      The file the release notes are written to: the body of the
#               "## [X.Y.Z]" section, from the line after its heading up to
#               (not including) the next "## " heading or the end of the file,
#               as is (CRLF line endings become LF). It is removed first and
#               written only on success.
#
# Format (design "CHANGELOG の書式"):
#   - The "## " headings are "## [Unreleased]" (exactly one, the first) and
#     "## [X.Y.Z] - YYYY-MM-DD".
#   - Release sections are ordered newest first (semver order) and no version
#     appears twice.
#   - The "### " headings are only the 7 categories below; "互換性のない変更",
#     when present, is the first "### " heading of its "## " section.
#   Other lines (including "# " and "#### " headings) are not checked. Code
#   blocks are not recognized: a "## " line inside one is a heading too.
#
# Every failure is collected; the script then fails once with
#   changelog_check (<MODE>): <n> failure(s)
# followed by one line per failure. <name> is the file name of CHANGELOG
# (CHANGELOG.md for the real file):
#   <name>:<line>: first '## ' heading must be '## [Unreleased]' (found '<line>')
#   <name>:<line>: '## [Unreleased]' must be the first '## ' heading
#   <name>:<line>: malformed heading '<line>' (expected '## [Unreleased]' or '## [X.Y.Z] - YYYY-MM-DD')
#   <name>:<line>: malformed date '<date>' (expected YYYY-MM-DD)
#   <name>:<line>: version <X.Y.Z> is newer than <A.B.C> at line <n> (sections must be newest first)
#   <name>:<line>: duplicate version <X.Y.Z> (also at line <n>)
#   <name>:<line>: unknown category '<text>' (expected one of: <categories>)
#   <name>:<line>: '互換性のない変更' must be the first category of its section
#   <name>:1: no '## [Unreleased]' heading (no '## ' heading found)
#   <name>: file not found (<path>)
#   MODE=release:
#   tag '<TAG>' is not of the form vX.Y.Z
#   CMakeLists.txt: expected exactly one 'project(clopts VERSION X.Y.Z', found <n> (<path>)
#   tag <TAG> does not match project VERSION <A.B.C>
#   no section for version <X.Y.Z> in <name>
#   section for version <X.Y.Z> is empty          (only blank lines)
# On success MODE=format prints (as a STATUS message)
#   changelog: <n> release section(s)
# and MODE=release prints exactly one line to standard output:
#   version X.Y.Z
#
# The script never writes to CHANGELOG or CMakeLists.txt.

cmake_minimum_required(VERSION 3.21)

set(_cl_categories
    "互換性のない変更" "追加" "変更" "非推奨" "削除" "修正" "セキュリティ")
set(_cl_breaking "互換性のない変更")
set(_cl_version_regex "(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)")

# Characters that cannot appear in the changelog stand in for ";", "[", "]"
# and "\" while its text is handled as a CMake list of lines.
string(ASCII 1 _cl_semicolon)
string(ASCII 2 _cl_open_bracket)
string(ASCII 3 _cl_close_bracket)
string(ASCII 4 _cl_backslash)

# ---- Helpers ---------------------------------------------------------------

# _cl_split(<out_var> <text>)
#
# Sets <out_var> to the lines of <text> as a list, with CRLF turned into LF
# and the special characters replaced by the stand-ins above. Element <i> is
# line <i + 1>; a final LF does not add an empty last element.
function(_cl_split out_var text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REGEX REPLACE "\n$" "" text "${text}")
    string(REPLACE "\\" "${_cl_backslash}" text "${text}")
    string(REPLACE ";" "${_cl_semicolon}" text "${text}")
    string(REPLACE "[" "${_cl_open_bracket}" text "${text}")
    string(REPLACE "]" "${_cl_close_bracket}" text "${text}")
    string(REPLACE "\n" ";" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# _cl_restore(<out_var> <text>)
#
# Sets <out_var> to <text> with the stand-ins turned back into the original
# characters.
function(_cl_restore out_var text)
    string(REPLACE "${_cl_backslash}" "\\" text "${text}")
    string(REPLACE "${_cl_semicolon}" ";" text "${text}")
    string(REPLACE "${_cl_open_bracket}" "[" text "${text}")
    string(REPLACE "${_cl_close_bracket}" "]" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# _cl_add_failure(<text>)
#
# Records one failure; <text> becomes one report line and may hold the
# stand-ins (it must not hold a raw ";").
function(_cl_add_failure text)
    set_property(GLOBAL APPEND_STRING PROPERTY _CL_REPORT "\n${text}")
    get_property(count GLOBAL PROPERTY _CL_FAILURES)
    math(EXPR count "${count} + 1")
    set_property(GLOBAL PROPERTY _CL_FAILURES "${count}")
endfunction()

# _cl_fail_if_any()
#
# Fails with every recorded failure. The report is printed as written: the
# stand-ins are turned back, and every line is prefixed with one space, which
# keeps message() from re-wrapping it.
function(_cl_fail_if_any)
    get_property(count GLOBAL PROPERTY _CL_FAILURES)
    if(count GREATER 0)
        get_property(report GLOBAL PROPERTY _CL_REPORT)
        set(report "changelog_check (${MODE}): ${count} failure(s)${report}")
        _cl_restore(report "${report}")
        string(REPLACE "\n" "\n " report "${report}")
        message(FATAL_ERROR " ${report}")
    endif()
endfunction()

# _cl_valid_date(<out_var> <date>)
#
# Sets <out_var> to TRUE if <date> is YYYY-MM-DD with a month of 01-12 and a
# day of 01-31.
function(_cl_valid_date out_var date)
    set(valid FALSE)
    if(date MATCHES "^[0-9][0-9][0-9][0-9]-([0-9][0-9])-([0-9][0-9])$")
        set(month "${CMAKE_MATCH_1}")
        set(day "${CMAKE_MATCH_2}")
        if(month GREATER_EQUAL 1 AND month LESS_EQUAL 12
                AND day GREATER_EQUAL 1 AND day LESS_EQUAL 31)
            set(valid TRUE)
        endif()
    endif()
    set(${out_var} ${valid} PARENT_SCOPE)
endfunction()

# _cl_check_format(<lines_var>)
#
# Checks the lines in <lines_var> (from _cl_split) and records every format
# failure. Sets, in the caller's scope:
#   _cl_versions          the release versions, in file order.
#   _cl_version_lines     the line number of each one's heading.
#   _cl_heading_lines     the line number of every "## " line.
function(_cl_check_format lines_var)
    set(open "${_cl_open_bracket}")
    set(close "${_cl_close_bracket}")
    set(versions "")
    set(version_lines "")
    set(heading_lines "")
    set(seen_heading FALSE)
    set(first_category TRUE)
    set(number 0)
    foreach(line IN LISTS ${lines_var})
        math(EXPR number "${number} + 1")
        _cl_restore(shown "${line}")
        string(REPLACE ";" "${_cl_semicolon}" shown "${shown}")
        if(line MATCHES "^## ")
            list(APPEND heading_lines ${number})
            set(first_category TRUE)
            if(seen_heading)
                set(is_first FALSE)
            else()
                set(is_first TRUE)
            endif()
            set(seen_heading TRUE)
            if(line STREQUAL "## ${open}Unreleased${close}")
                if(NOT is_first)
                    _cl_add_failure("${_cl_name}:${number}: '## [Unreleased]' must be the first '## ' heading")
                endif()
                continue()
            endif()
            if(is_first)
                _cl_add_failure("${_cl_name}:${number}: first '## ' heading must be '## [Unreleased]' (found '${shown}')")
            endif()
            if(NOT line MATCHES "^## ${open}(${_cl_version_regex})${close} - (.*)$")
                _cl_add_failure("${_cl_name}:${number}: malformed heading '${shown}' (expected '## [Unreleased]' or '## [X.Y.Z] - YYYY-MM-DD')")
                continue()
            endif()
            set(version "${CMAKE_MATCH_1}")
            set(date "${CMAKE_MATCH_5}")
            _cl_valid_date(valid "${date}")
            if(NOT valid)
                _cl_restore(date "${date}")
                string(REPLACE ";" "${_cl_semicolon}" date "${date}")
                _cl_add_failure("${_cl_name}:${number}: malformed date '${date}' (expected YYYY-MM-DD)")
            endif()
            list(FIND versions "${version}" index)
            list(LENGTH versions count)
            if(index GREATER_EQUAL 0)
                list(GET version_lines ${index} other)
                _cl_add_failure("${_cl_name}:${number}: duplicate version ${version} (also at line ${other})")
            elseif(count GREATER 0)
                list(GET versions -1 previous)
                list(GET version_lines -1 previous_line)
                if(version VERSION_GREATER previous)
                    _cl_add_failure("${_cl_name}:${number}: version ${version} is newer than ${previous} at line ${previous_line} (sections must be newest first)")
                endif()
            endif()
            list(APPEND versions "${version}")
            list(APPEND version_lines ${number})
        elseif(line MATCHES "^### ")
            string(SUBSTRING "${shown}" 4 -1 category)
            if(NOT category IN_LIST _cl_categories)
                string(REPLACE ";" ", " expected "${_cl_categories}")
                _cl_add_failure("${_cl_name}:${number}: unknown category '${category}' (expected one of: ${expected})")
            elseif(category STREQUAL _cl_breaking AND NOT first_category)
                _cl_add_failure("${_cl_name}:${number}: '${_cl_breaking}' must be the first category of its section")
            endif()
            set(first_category FALSE)
        endif()
    endforeach()
    if(NOT seen_heading)
        _cl_add_failure("${_cl_name}:1: no '## [Unreleased]' heading (no '## ' heading found)")
    endif()
    set(_cl_versions "${versions}" PARENT_SCOPE)
    set(_cl_version_lines "${version_lines}" PARENT_SCOPE)
    set(_cl_heading_lines "${heading_lines}" PARENT_SCOPE)
endfunction()

# _cl_read_changelog(<lines_var>)
#
# Reads CHANGELOG into <lines_var> (from _cl_split). A missing file is a
# failure and leaves <lines_var> empty.
function(_cl_read_changelog lines_var)
    if(NOT EXISTS "${CHANGELOG}" OR IS_DIRECTORY "${CHANGELOG}")
        _cl_add_failure("${_cl_name}: file not found (${CHANGELOG})")
        set(${lines_var} "" PARENT_SCOPE)
        return()
    endif()
    file(READ "${CHANGELOG}" text)
    _cl_split(lines "${text}")
    set(${lines_var} "${lines}" PARENT_SCOPE)
endfunction()

# ---- Modes -----------------------------------------------------------------

function(_cl_mode_format)
    _cl_read_changelog(lines)
    if(EXISTS "${CHANGELOG}")
        _cl_check_format(lines)
    endif()
    _cl_fail_if_any()
    list(LENGTH _cl_versions count)
    message(STATUS "changelog: ${count} release section(s)")
endfunction()

function(_cl_mode_release)
    file(REMOVE "${OUTPUT}")

    # The changelog's format.
    _cl_read_changelog(lines)
    set(have_changelog FALSE)
    if(EXISTS "${CHANGELOG}")
        set(have_changelog TRUE)
        _cl_check_format(lines)
    endif()

    # The tag.
    set(tag_version "")
    if(TAG MATCHES "^v(${_cl_version_regex})$")
        set(tag_version "${CMAKE_MATCH_1}")
    else()
        string(REPLACE ";" "${_cl_semicolon}" shown "${TAG}")
        _cl_add_failure("tag '${shown}' is not of the form vX.Y.Z")
    endif()

    # The project version.
    set(cmakelists "${SOURCE_DIR}/CMakeLists.txt")
    set(project_version "")
    if(NOT EXISTS "${cmakelists}" OR IS_DIRECTORY "${cmakelists}")
        _cl_add_failure("CMakeLists.txt: file not found (${cmakelists})")
    else()
        file(READ "${cmakelists}" text)
        # Only a call at the start of a line (after indentation) counts, so a
        # commented-out call is not one.
        string(REGEX MATCHALL "(^|\n)[ \t]*project\\(clopts[ \t]+VERSION[ \t]+[0-9.]*"
            calls "${text}")
        list(LENGTH calls count)
        if(NOT count EQUAL 1)
            _cl_add_failure("CMakeLists.txt: expected exactly one 'project(clopts VERSION X.Y.Z', found ${count} (${cmakelists})")
        else()
            string(REGEX REPLACE ".*[ \t]" "" project_version "${calls}")
            if(NOT project_version MATCHES "^${_cl_version_regex}$")
                _cl_add_failure("CMakeLists.txt: project VERSION '${project_version}' is not of the form X.Y.Z (${cmakelists})")
                set(project_version "")
            elseif(NOT tag_version STREQUAL "" AND NOT tag_version STREQUAL project_version)
                _cl_add_failure("tag ${TAG} does not match project VERSION ${project_version}")
            endif()
        endif()
    endif()

    # The section of the tag's version.
    set(notes "")
    if(have_changelog AND NOT tag_version STREQUAL "")
        list(FIND _cl_versions "${tag_version}" index)
        if(index LESS 0)
            _cl_add_failure("no section for version ${tag_version} in ${_cl_name}")
        else()
            list(GET _cl_version_lines ${index} heading)
            # Line numbers are 1-based; list indexes are 0-based, so the
            # body starts at index <heading> and ends before the index of the
            # next "## " line.
            list(LENGTH lines end)
            foreach(other IN LISTS _cl_heading_lines)
                if(other GREATER heading)
                    math(EXPR end "${other} - 1")
                    break()
                endif()
            endforeach()
            set(nonblank FALSE)
            set(i ${heading})
            while(i LESS end)
                list(GET lines ${i} line)
                string(APPEND notes "${line}\n")
                if(line MATCHES "[^ \t]")
                    set(nonblank TRUE)
                endif()
                math(EXPR i "${i} + 1")
            endwhile()
            if(NOT nonblank)
                _cl_add_failure("section for version ${tag_version} is empty")
            endif()
        endif()
    endif()

    _cl_fail_if_any()
    _cl_restore(notes "${notes}")
    file(WRITE "${OUTPUT}" "${notes}")
    # message() writes to standard error (or adds "-- " on standard output),
    # so the one line the release workflow reads is printed by a child process
    # that inherits this process's standard output.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "version ${tag_version}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "changelog_check: could not print the version (${result})")
    endif()
endfunction()

# ---- Main ------------------------------------------------------------------

set_property(GLOBAL PROPERTY _CL_FAILURES 0)
set_property(GLOBAL PROPERTY _CL_REPORT "")

if(MODE STREQUAL "format")
    if(NOT DEFINED CHANGELOG OR CHANGELOG STREQUAL "")
        message(FATAL_ERROR "changelog_check: CHANGELOG is required for MODE=format")
    endif()
elseif(MODE STREQUAL "release")
    foreach(var IN ITEMS TAG SOURCE_DIR OUTPUT)
        if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
            message(FATAL_ERROR "changelog_check: ${var} is required for MODE=release")
        endif()
    endforeach()
    if(NOT IS_DIRECTORY "${SOURCE_DIR}")
        message(FATAL_ERROR "changelog_check: SOURCE_DIR is not a directory ('${SOURCE_DIR}')")
    endif()
    if(NOT DEFINED CHANGELOG OR CHANGELOG STREQUAL "")
        set(CHANGELOG "${SOURCE_DIR}/CHANGELOG.md")
    endif()
else()
    message(FATAL_ERROR "changelog_check: unknown MODE '${MODE}' (expected one of: format, release)")
endif()

get_filename_component(_cl_name "${CHANGELOG}" NAME)
if(MODE STREQUAL "format")
    _cl_mode_format()
else()
    _cl_mode_release()
endif()
