# Checks the documentation against the programs, sources, files and public
# symbols it describes (requirements 2.9, 6.2, 6.5, 8.2-8.8). Run with
# `cmake -P`; registered by tests/docs/CMakeLists.txt (label: docs).
#
# Variables (-D):
#   MODE        Always. What to check:
#                 outputs       the output of every clopts-check marker.
#                 sources       the code block of every clopts-source marker.
#                 links         every relative link.
#                 api_coverage  that every public symbol is documented.
#   SOURCE_DIR  Always. The directory FILES are relative to (a relative path
#               is taken from the current directory).
#   FILES       Always. The documents to read, as a ";"-separated list of
#               paths relative to SOURCE_DIR. A missing file is a failure.
#
#   MODE=outputs:
#   PROGRAMS    The programs a marker may run, as a list of <name>=<path>.
#   ENV_VARS    The environment variables the programs read, as a list of
#               <name>=<var>,<var>... Every listed variable is removed from the
#               environment of every run; a marker sets only its env= ones.
#   REQUIRE     Program names that must have at least one marker with exit
#               code 0 and one with a nonzero exit code (requirement 8.3).
#   WORK        A directory the script may write to. The programs run in the
#               empty directory <WORK>/outputs/run with an empty standard
#               input, and the expected and actual output of every mismatch is
#               written to <WORK>/outputs/<document>-<line>.expected / .actual.
#               Only <WORK>/outputs is removed and recreated.
#
#   MODE=api_coverage:
#   SYMBOLS     The public symbol list (format of abi/clopts.symbols: one name
#               per line, optionally followed by a platform tag; blank lines
#               and "#" comments are skipped), relative to SOURCE_DIR or
#               absolute. A missing list is a failure.
#
# Marker formats (design "照合用の印の書式"): one line, starting at column 0,
#
#   <!-- clopts-check: [exit=<int>] [stream=stdout|stderr] [env=<NAME>=<VALUE>]... -- <program> [<arg>...] -->
#   <!-- clopts-source: <path relative to SOURCE_DIR> -->
#
# immediately followed by a code block fenced with 3 or more backticks that
# holds the expected output (an empty block means no output) or the content
# of the file. The block closes with a line, starting at column 0, of at least
# as many backticks. For clopts-check, the text after the marker's colon is
# split with separate_arguments(UNIX_COMMAND); the words before the first "--"
# are attributes, the first word after it is a name from PROGRAMS and the rest
# are its arguments. Defaults: exit=0, stream=stdout. An env= value must not
# be empty (CMake, like _putenv on Windows, removes a variable set to ""), and
# a marker must not contain ";".
# Markers and links inside other fenced code blocks are not checked; links
# inside code spans are not checked either.
#
# Comparison: CRLF is turned into LF on both sides and nothing else is
# normalized; the expected output (or source) is every line of the block
# followed by LF.
#
# Every failure is collected; the script then fails once with
#   docs_check (<MODE>): <n> failure(s)
# followed by one or more lines per failure, each starting with
# "<document>:<line>: " (api_coverage: "<SYMBOLS>:<line>: "). The failures:
#   outputs       output of '<command>' differs (first difference at <doc>:<line>)
#                 exit code <n> (expected <n>)
#                 malformed marker: <reason>
#                 <program>: no output marker with exit code 0 / a nonzero ...
#   sources       code block differs from <file> (first difference at line <n>)
#                 source file not found: <file>
#                 malformed marker: <reason>
#   links         broken link: <target>
#                 link to maintainer docs: <target>
#   api_coverage  not documented in <documents>: <name>   (every missing name)
#   all modes     <file>: file not found (<path>)
#                 no clopts-check / clopts-source marker found in: <documents>
#                 no relative link found in: <documents>
#                 no symbol found in: <SYMBOLS>
# On success it prints one of
#   docs_check: checked <n> output marker(s)
#   docs_check: checked <n> source marker(s)
#   docs_check: checked <n> link(s)
#   docs_check: checked <n> symbol(s)
#
# The script never writes to the documents.

cmake_minimum_required(VERSION 3.21)

set(_docs_modes outputs sources links api_coverage)

# Characters that cannot appear in the documents stand in for ";", "[", "]"
# and "\" while a text is handled as a CMake list of lines.
string(ASCII 1 _docs_semicolon)
string(ASCII 2 _docs_open_bracket)
string(ASCII 3 _docs_close_bracket)
string(ASCII 4 _docs_backslash)

# ---- Helpers ---------------------------------------------------------------

# _docs_split(<out_var> <text>)
#
# Sets <out_var> to the lines of <text> as a list, with CRLF turned into LF
# first and the special characters replaced by the stand-ins above. Empty
# lines are kept, so element <i> is line <i + 1>; a text that ends with LF has
# an empty last element.
function(_docs_split out_var text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REPLACE "\\" "${_docs_backslash}" text "${text}")
    string(REPLACE ";" "${_docs_semicolon}" text "${text}")
    string(REPLACE "[" "${_docs_open_bracket}" text "${text}")
    string(REPLACE "]" "${_docs_close_bracket}" text "${text}")
    string(REPLACE "\n" ";" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# _docs_restore(<out_var> <text> [KEEP_SEMICOLON])
#
# Sets <out_var> to <text> with the stand-ins turned back into the original
# characters. With KEEP_SEMICOLON the ";" stand-in is left in place (for text
# that is still handled as a list).
function(_docs_restore out_var text)
    string(REPLACE "${_docs_backslash}" "\\" text "${text}")
    if(NOT ARGV2 STREQUAL "KEEP_SEMICOLON")
        string(REPLACE "${_docs_semicolon}" ";" text "${text}")
    endif()
    string(REPLACE "${_docs_open_bracket}" "[" text "${text}")
    string(REPLACE "${_docs_close_bracket}" "]" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# _docs_add_failure(<doc> <line> <text>...)
#
# Records one failure. Each <text> becomes one report line prefixed with
# "<doc>:<line>: " (with an empty <line>, "<doc>: "; with an empty <doc>, no
# prefix). <text> may hold the stand-ins.
function(_docs_add_failure doc line)
    if(doc STREQUAL "")
        set(prefix "")
    elseif(line STREQUAL "")
        set(prefix "${doc}: ")
    else()
        set(prefix "${doc}:${line}: ")
    endif()
    foreach(text IN LISTS ARGN)
        set_property(GLOBAL APPEND_STRING PROPERTY _DOCS_REPORT "\n${prefix}${text}")
    endforeach()
    get_property(count GLOBAL PROPERTY _DOCS_FAILURES)
    math(EXPR count "${count} + 1")
    set_property(GLOBAL PROPERTY _DOCS_FAILURES "${count}")
endfunction()

# _docs_finish(<success message>)
#
# Fails with every recorded failure, or prints <success message>. The report
# is printed as written: the stand-ins are turned back, and every line is
# prefixed with one space, which keeps message() from re-wrapping it.
function(_docs_finish success)
    get_property(count GLOBAL PROPERTY _DOCS_FAILURES)
    if(count GREATER 0)
        get_property(report GLOBAL PROPERTY _DOCS_REPORT)
        set(report "docs_check (${MODE}): ${count} failure(s)${report}")
        _docs_restore(report "${report}")
        string(REPLACE "\n" "\n " report "${report}")
        message(FATAL_ERROR " ${report}")
    endif()
    message(STATUS "${success}")
endfunction()

# _docs_read(<out_var> <doc>)
#
# Sets <out_var> to the lines of ${SOURCE_DIR}/<doc> (see _docs_split), or to
# the value NOTFOUND after recording a failure when the file does not exist.
function(_docs_read out_var doc)
    if(NOT EXISTS "${SOURCE_DIR}/${doc}" OR IS_DIRECTORY "${SOURCE_DIR}/${doc}")
        _docs_add_failure("${doc}" "" "file not found (${SOURCE_DIR}/${doc})")
        set(${out_var} NOTFOUND PARENT_SCOPE)
        return()
    endif()
    file(READ "${SOURCE_DIR}/${doc}" content)
    _docs_split(content "${content}")
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

# _docs_fence_open(<out_char> <out_length> <line>)
#
# When <line> opens a fenced code block (up to 3 spaces, then 3 or more "`"
# or "~"; a "`" fence's info string has no "`"), sets <out_char> to "`" or
# "~" and <out_length> to the length of the fence. Otherwise sets both to "".
function(_docs_fence_open out_char out_length line)
    set(char "")
    set(length "")
    if(line MATCHES "^ ? ? ?(```+)(.*)$")
        set(fence "${CMAKE_MATCH_1}")
        if(NOT CMAKE_MATCH_2 MATCHES "`")
            set(char "`")
            string(LENGTH "${fence}" length)
        endif()
    elseif(line MATCHES "^ ? ? ?(~~~+)")
        set(char "~")
        string(LENGTH "${CMAKE_MATCH_1}" length)
    endif()
    set(${out_char} "${char}" PARENT_SCOPE)
    set(${out_length} "${length}" PARENT_SCOPE)
endfunction()

# _docs_fence_closes(<out_var> <char> <length> <line>)
#
# Sets <out_var> to TRUE when <line> closes a block opened by a fence of
# <length> characters <char> (up to 3 spaces, at least <length> of the same
# character, then only whitespace).
function(_docs_fence_closes out_var char length line)
    set(result FALSE)
    if(char STREQUAL "`")
        set(pattern "^ ? ? ?(```+)[ \t]*$")
    else()
        set(pattern "^ ? ? ?(~~~+)[ \t]*$")
    endif()
    if(line MATCHES "${pattern}")
        string(LENGTH "${CMAKE_MATCH_1}" found)
        if(found GREATER_EQUAL length)
            set(result TRUE)
        endif()
    endif()
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

# _docs_first_difference(<out_index> <out_a> <out_b> <lines_a> <lines_b>
#                        <end_a> <end_b>)
#
# Compares two texts split by _docs_split item by item (see _docs_items) and
# sets <out_index> to the 0-based index of the first item that differs, and
# <out_a> / <out_b> to that item of each side as shown in the report (<end_a>
# or <end_b> when that side has no more items). Call it only for texts that
# differ.
function(_docs_first_difference out_index out_a out_b lines_a lines_b end_a end_b)
    _docs_items(items_a count_a "${lines_a}")
    _docs_items(items_b count_b "${lines_b}")
    set(index 0)
    while(TRUE)
        if(index LESS count_a)
            list(GET items_a ${index} text_a)
            string(SUBSTRING "${text_a}" 1 -1 text_a)
        else()
            set(text_a "${end_a}")
        endif()
        if(index LESS count_b)
            list(GET items_b ${index} text_b)
            string(SUBSTRING "${text_b}" 1 -1 text_b)
        else()
            set(text_b "${end_b}")
        endif()
        if(NOT text_a STREQUAL text_b)
            break()
        endif()
        math(EXPR index "${index} + 1")
    endwhile()
    set(${out_index} ${index} PARENT_SCOPE)
    set(${out_a} "${text_a}" PARENT_SCOPE)
    set(${out_b} "${text_b}" PARENT_SCOPE)
endfunction()

# _docs_marker_block(<prefix> <lines> <i> <line>)
#
# Reads the code block that must follow a marker on line <line> of a document
# split into <lines>; <i> is the index of the line after the marker. The block
# opens with a fence of 3 or more backticks and closes with a line, starting
# at column 0, of at least as many backticks. Sets in the caller:
#   <prefix>_block       every line of the block followed by LF (stand-ins kept)
#   <prefix>_block_line  the line of the document the block starts at
#   <prefix>_next        the index to continue at (after the block, at the end
#                        of an unclosed one, or <i> when there is no block)
#   <prefix>_errors      reasons the block is malformed (empty if not)
function(_docs_marker_block prefix lines i line)
    list(LENGTH lines line_count)
    set(block "")
    set(block_line "")
    set(next_index ${i})
    set(errors "")
    set(next "")
    if(i LESS line_count)
        list(GET lines ${i} next)
    endif()
    if(NOT next MATCHES "^(```+)")
        list(APPEND errors "the next line is not a fence of 3 or more backticks")
    else()
        string(LENGTH "${CMAKE_MATCH_1}" open_length)
        math(EXPR block_line "${line} + 2")
        math(EXPR j "${i} + 1")
        set(closed FALSE)
        while(j LESS line_count)
            list(GET lines ${j} block_text)
            math(EXPR j "${j} + 1")
            _docs_fence_closes(closes "`" "${open_length}" "${block_text}")
            if(closes AND block_text MATCHES "^`")
                set(closed TRUE)
                break()
            endif()
            string(APPEND block "${block_text}\n")
        endwhile()
        set(next_index ${j})
        if(NOT closed)
            list(APPEND errors "the code block is not closed")
        endif()
    endif()
    set(${prefix}_block "${block}" PARENT_SCOPE)
    set(${prefix}_block_line "${block_line}" PARENT_SCOPE)
    set(${prefix}_next "${next_index}" PARENT_SCOPE)
    set(${prefix}_errors "${errors}" PARENT_SCOPE)
endfunction()

# ---- MODE=outputs ----------------------------------------------------------

# _docs_items(<out_texts> <out_count> <lines>)
#
# Splits <lines> (a text split by _docs_split) into the items the first
# difference is searched in: every complete line, plus the text after the last
# LF when it is not empty. Sets <out_texts> to the items as they are shown in
# the report (an empty line is "<empty line>", an item without LF gets
# " (no newline at end)"), each prefixed
# with "|" so that an empty line is still a list element, and <out_count> to
# their number.
function(_docs_items out_texts out_count lines)
    list(LENGTH lines n)
    set(texts "")
    set(count 0)
    math(EXPR last "${n} - 1")
    set(index 0)
    foreach(text IN LISTS lines)
        if(index LESS last)
            if(text STREQUAL "")
                set(text "<empty line>")
            endif()
            list(APPEND texts "|${text}")
            math(EXPR count "${count} + 1")
        elseif(NOT text STREQUAL "")
            list(APPEND texts "|${text} (no newline at end)")
            math(EXPR count "${count} + 1")
        endif()
        math(EXPR index "${index} + 1")
    endforeach()
    set(${out_texts} "${texts}" PARENT_SCOPE)
    set(${out_count} ${count} PARENT_SCOPE)
endfunction()

# _docs_parse_marker(<prefix> <body>)
#
# Parses the text of a marker after "clopts-check:" (with the stand-ins).
# Sets in the caller:
#   <prefix>_errors   list of reasons the marker is malformed (empty if not)
#   <prefix>_exit     expected exit code
#   <prefix>_stream   stdout or stderr
#   <prefix>_env      list of NAME=VALUE
#   <prefix>_program  program name
#   <prefix>_args     arguments
#   <prefix>_display  the command as shown in the report
function(_docs_parse_marker prefix body)
    set(errors "")
    set(exit 0)
    set(stream stdout)
    set(env "")
    set(program "")
    set(args "")
    set(display "")

    if(body MATCHES "${_docs_semicolon}")
        # The stand-in is turned back into ";" in the report; a raw ";" would
        # split the reason into two list elements.
        list(APPEND errors "'${_docs_semicolon}' is not supported in a marker")
    endif()
    # separate_arguments needs the real quotes and backslashes; ";" stays a
    # stand-in so that no word is split.
    _docs_restore(body "${body}" KEEP_SEMICOLON)
    separate_arguments(words UNIX_COMMAND "${body}")

    list(FIND words "--" separator)
    if(separator EQUAL -1)
        # Without "--" the words cannot be told apart: report only that.
        list(APPEND errors "no '--' before the program")
        set(attributes "")
        set(command "")
    else()
        set(attributes "")
        if(separator GREATER 0)
            list(SUBLIST words 0 ${separator} attributes)
        endif()
        math(EXPR first "${separator} + 1")
        set(command "")
        list(LENGTH words count)
        if(first LESS count)
            list(SUBLIST words ${first} -1 command)
        endif()
    endif()

    foreach(attribute IN LISTS attributes)
        if(attribute MATCHES "^exit=(.*)$")
            set(value "${CMAKE_MATCH_1}")
            if(value MATCHES "^-?[0-9]+$")
                set(exit "${value}")
            else()
                list(APPEND errors "exit '${value}' is not an integer")
            endif()
        elseif(attribute MATCHES "^stream=(.*)$")
            set(value "${CMAKE_MATCH_1}")
            if(value STREQUAL "stdout" OR value STREQUAL "stderr")
                set(stream "${value}")
            else()
                list(APPEND errors "stream '${value}' is not stdout or stderr")
            endif()
        elseif(attribute MATCHES "^env=(.*)$")
            set(value "${CMAKE_MATCH_1}")
            if(NOT value MATCHES "=")
                list(APPEND errors "env '${value}' has no '='")
            elseif(value MATCHES "^=")
                list(APPEND errors "env '${value}' has no variable name")
            elseif(value MATCHES "=$")
                # set(ENV{NAME} "") removes the variable (so does _putenv on
                # Windows): an empty value cannot be shown.
                list(APPEND errors "env '${value}' has an empty value")
            else()
                list(APPEND env "${value}")
            endif()
        else()
            list(APPEND errors "unknown attribute '${attribute}'")
        endif()
    endforeach()

    if(separator GREATER -1)
        list(LENGTH command count)
        if(count EQUAL 0)
            list(APPEND errors "no program after '--'")
        else()
            list(POP_FRONT command program)
            set(args "${command}")
            if(NOT program IN_LIST _docs_program_names)
                if(_docs_program_names)
                    list(JOIN _docs_program_names ", " registered)
                else()
                    set(registered "none")
                endif()
                list(APPEND errors "unknown program '${program}' (registered: ${registered})")
            endif()
            # The command as shown in the report: words with blanks are quoted.
            set(shown "")
            foreach(word IN ITEMS "${program}" ${args})
                if(word MATCHES "[ \t]" OR word STREQUAL "")
                    set(word "\"${word}\"")
                endif()
                list(APPEND shown "${word}")
            endforeach()
            list(JOIN shown " " display)
        endif()
    endif()

    foreach(name errors exit stream env program args display)
        set(${prefix}_${name} "${${name}}" PARENT_SCOPE)
    endforeach()
endfunction()

# _docs_run_marker(<doc> <line> <block_line> <block> <prefix>)
#
# Runs the program of a well-formed marker (parsed into <prefix>_*) and
# compares its output and exit code with <block> (the expected output: every
# line of the block followed by LF, with the stand-ins), whose first line is
# line <block_line> of <doc>.
# Records a failure for every difference.
function(_docs_run_marker doc line block_line block prefix)
    set(program "${${prefix}_program}")
    set(display "${${prefix}_display}")
    set(path "${_docs_program_path_${program}}")

    # Environment: remove every registered variable, then set only the
    # marker's env= ones. A variable that is not registered keeps its outer
    # value for the other markers: it is saved here and restored after the run.
    foreach(var IN LISTS _docs_all_env_vars)
        unset(ENV{${var}})
    endforeach()
    set(set_vars "")
    foreach(pair IN LISTS ${prefix}_env)
        string(FIND "${pair}" "=" eq)
        string(SUBSTRING "${pair}" 0 ${eq} name)
        math(EXPR eq "${eq} + 1")
        string(SUBSTRING "${pair}" ${eq} -1 value)
        _docs_restore(value "${value}")
        if(NOT name IN_LIST set_vars)
            list(APPEND set_vars "${name}")
            if(DEFINED ENV{${name}} AND NOT name IN_LIST _docs_all_env_vars)
                set(had_${name} TRUE)
                set(old_${name} "$ENV{${name}}")
            else()
                set(had_${name} FALSE)
            endif()
        endif()
        set(ENV{${name}} "${value}")
    endforeach()

    set(run_dir "${_docs_work}/run")
    file(REMOVE_RECURSE "${run_dir}")
    file(MAKE_DIRECTORY "${run_dir}")

    set(command "${path}")
    foreach(arg IN LISTS ${prefix}_args)
        _docs_restore(arg "${arg}" KEEP_SEMICOLON)
        list(APPEND command "${arg}")
    endforeach()

    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        set(result "program file not found (${path})")
    else()
        execute_process(
            COMMAND ${command}
            WORKING_DIRECTORY "${run_dir}"
            INPUT_FILE "${_docs_work}/empty-stdin"
            OUTPUT_VARIABLE out
            ERROR_VARIABLE err
            RESULT_VARIABLE result
        )
    endif()

    foreach(var IN LISTS set_vars)
        if(had_${var})
            set(ENV{${var}} "${old_${var}}")
        else()
            unset(ENV{${var}})
        endif()
    endforeach()

    if(NOT result MATCHES "^-?[0-9]+$")
        _docs_add_failure("${doc}" "${line}" "cannot run '${display}': ${result}")
        return()
    endif()

    if(${prefix}_stream STREQUAL "stdout")
        set(actual "${out}")
    else()
        set(actual "${err}")
    endif()
    string(REPLACE "\r\n" "\n" actual "${actual}")

    _docs_restore(expected "${block}")

    if(NOT actual STREQUAL expected)
        _docs_split(expected_lines "${expected}")
        _docs_split(actual_lines "${actual}")
        _docs_first_difference(index expected_text actual_text
            "${expected_lines}" "${actual_lines}" "<end of output>" "<end of output>")
        # Line of the document the difference is at: a line of the block, or
        # the closing fence when the program printed more.
        math(EXPR at "${block_line} + ${index}")

        string(REGEX REPLACE "[^A-Za-z0-9_.-]" "_" stem "${doc}")
        set(expected_file "${_docs_work}/${stem}-${line}.expected")
        set(actual_file "${_docs_work}/${stem}-${line}.actual")
        file(WRITE "${expected_file}" "${expected}")
        file(WRITE "${actual_file}" "${actual}")

        _docs_add_failure("${doc}" "${line}"
            "output of '${display}' differs (first difference at ${doc}:${at})"
            "expected (document): ${expected_text}"
            "actual (${${prefix}_stream}): ${actual_text}"
            "files: ${expected_file} ${actual_file}"
        )
    endif()

    if(NOT result EQUAL ${prefix}_exit)
        _docs_add_failure("${doc}" "${line}" "exit code ${result} (expected ${${prefix}_exit})")
    endif()
endfunction()

# _docs_check_outputs()
#
# MODE=outputs: checks every clopts-check marker of FILES.
function(_docs_check_outputs)
    if(NOT DEFINED WORK OR WORK STREQUAL "")
        message(FATAL_ERROR "docs_check: WORK is required for MODE=outputs")
    endif()
    get_filename_component(work "${WORK}" ABSOLUTE)
    set(_docs_work "${work}/outputs")
    file(REMOVE_RECURSE "${_docs_work}")
    file(MAKE_DIRECTORY "${_docs_work}")
    file(WRITE "${_docs_work}/empty-stdin" "")

    # PROGRAMS: <name>=<path>
    set(_docs_program_names "")
    foreach(entry IN LISTS PROGRAMS)
        if(NOT entry MATCHES "^([^=]+)=(.+)$")
            message(FATAL_ERROR "docs_check: PROGRAMS entry '${entry}' is not <name>=<path>")
        endif()
        list(APPEND _docs_program_names "${CMAKE_MATCH_1}")
        set(_docs_program_path_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endforeach()

    # ENV_VARS: <name>=<var>,<var>... Every variable is isolated in every run.
    set(_docs_all_env_vars "")
    foreach(entry IN LISTS ENV_VARS)
        if(NOT entry MATCHES "^([^=]+)=(.*)$")
            message(FATAL_ERROR "docs_check: ENV_VARS entry '${entry}' is not <name>=<var>,<var>...")
        endif()
        string(REPLACE "," ";" vars "${CMAKE_MATCH_2}")
        foreach(var IN LISTS vars)
            if(NOT var STREQUAL "")
                list(APPEND _docs_all_env_vars "${var}")
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _docs_all_env_vars)

    set(found 0)    # markers seen, well-formed or not
    set(checked 0)  # well-formed markers that were run
    set(exit_zero "")
    set(exit_nonzero "")

    foreach(doc IN LISTS FILES)
        _docs_read(lines "${doc}")
        if(lines STREQUAL "NOTFOUND")
            continue()
        endif()
        list(LENGTH lines line_count)
        set(fence_char "")
        set(fence_length "")
        set(i 0)
        while(i LESS line_count)
            list(GET lines ${i} text)
            math(EXPR line "${i} + 1")
            math(EXPR i "${i} + 1")

            # Inside a fenced code block: only look for its end.
            if(NOT fence_char STREQUAL "")
                _docs_fence_closes(closes "${fence_char}" "${fence_length}" "${text}")
                if(closes)
                    set(fence_char "")
                endif()
                continue()
            endif()
            _docs_fence_open(open_char open_length "${text}")
            if(NOT open_char STREQUAL "")
                set(fence_char "${open_char}")
                set(fence_length "${open_length}")
                continue()
            endif()

            if(NOT text MATCHES "^<!--[ \t]*clopts-check([: \t]|-->|$)")
                continue()
            endif()
            math(EXPR found "${found} + 1")

            set(errors "")
            string(REGEX REPLACE "[ \t]+$" "" text "${text}")
            if(NOT text MATCHES "-->$")
                # The block that may follow is not looked at: the marker may
                # go on over several lines.
                _docs_add_failure("${doc}" "${line}"
                    "malformed marker: the marker does not end with '-->' on the same line")
                continue()
            elseif(NOT text MATCHES "^<!-- clopts-check:(.*)-->$")
                list(APPEND errors "the marker does not start with '<!-- clopts-check:'")
            else()
                _docs_parse_marker(marker "${CMAKE_MATCH_1}")
                list(APPEND errors ${marker_errors})
            endif()

            # The expected output: the block right after the marker.
            _docs_marker_block(found_block "${lines}" ${i} ${line})
            set(block "${found_block_block}")
            set(block_line "${found_block_block_line}")
            list(APPEND errors ${found_block_errors})
            # Continue after the block (or at the end of an unclosed one).
            set(i ${found_block_next})

            if(errors)
                foreach(reason IN LISTS errors)
                    _docs_add_failure("${doc}" "${line}" "malformed marker: ${reason}")
                endforeach()
                continue()
            endif()

            math(EXPR checked "${checked} + 1")
            if(marker_exit EQUAL 0)
                list(APPEND exit_zero "${marker_program}")
            else()
                list(APPEND exit_nonzero "${marker_program}")
            endif()
            _docs_run_marker("${doc}" "${line}" "${block_line}" "${block}" marker)
        endwhile()
    endforeach()

    if(found EQUAL 0)
        list(JOIN FILES ", " files)
        _docs_add_failure("" "" "no clopts-check marker found in: ${files}")
    endif()

    # Requirement 8.3: help (exit code 0) and a usage error (nonzero).
    foreach(name IN LISTS REQUIRE)
        if(NOT name IN_LIST exit_zero)
            _docs_add_failure("" "" "${name}: no output marker with exit code 0")
        endif()
        if(NOT name IN_LIST exit_nonzero)
            _docs_add_failure("" "" "${name}: no output marker with a nonzero exit code")
        endif()
    endforeach()

    _docs_finish("docs_check: checked ${checked} output marker(s)")
endfunction()

# ---- MODE=sources ----------------------------------------------------------

# _docs_check_sources()
#
# MODE=sources: checks that the block after every clopts-source marker of
# FILES is the content of the file the marker names (requirement 8.6).
function(_docs_check_sources)
    set(found 0)    # markers seen, well-formed or not
    set(checked 0)  # well-formed markers whose file was compared

    foreach(doc IN LISTS FILES)
        _docs_read(lines "${doc}")
        if(lines STREQUAL "NOTFOUND")
            continue()
        endif()
        list(LENGTH lines line_count)
        set(fence_char "")
        set(fence_length "")
        set(i 0)
        while(i LESS line_count)
            list(GET lines ${i} text)
            math(EXPR line "${i} + 1")
            math(EXPR i "${i} + 1")

            # Inside a fenced code block: only look for its end.
            if(NOT fence_char STREQUAL "")
                _docs_fence_closes(closes "${fence_char}" "${fence_length}" "${text}")
                if(closes)
                    set(fence_char "")
                endif()
                continue()
            endif()
            _docs_fence_open(open_char open_length "${text}")
            if(NOT open_char STREQUAL "")
                set(fence_char "${open_char}")
                set(fence_length "${open_length}")
                continue()
            endif()

            if(NOT text MATCHES "^<!--[ \t]*clopts-source([: \t]|-->|$)")
                continue()
            endif()
            math(EXPR found "${found} + 1")

            set(errors "")
            set(source "")
            string(REGEX REPLACE "[ \t]+$" "" text "${text}")
            if(NOT text MATCHES "-->$")
                # The block that may follow is not looked at: the marker may
                # go on over several lines.
                _docs_add_failure("${doc}" "${line}"
                    "malformed marker: the marker does not end with '-->' on the same line")
                continue()
            elseif(NOT text MATCHES "^<!-- clopts-source:(.*)-->$")
                list(APPEND errors "the marker does not start with '<!-- clopts-source:'")
            else()
                string(STRIP "${CMAKE_MATCH_1}" source)
                if(source STREQUAL "")
                    list(APPEND errors "no file after 'clopts-source:'")
                endif()
            endif()

            _docs_marker_block(found_block "${lines}" ${i} ${line})
            list(APPEND errors ${found_block_errors})
            set(i ${found_block_next})

            if(errors)
                foreach(reason IN LISTS errors)
                    _docs_add_failure("${doc}" "${line}" "malformed marker: ${reason}")
                endforeach()
                continue()
            endif()

            # The file, relative to SOURCE_DIR. "source" holds the stand-ins.
            _docs_restore(path "${source}")
            if(NOT EXISTS "${SOURCE_DIR}/${path}" OR IS_DIRECTORY "${SOURCE_DIR}/${path}")
                _docs_add_failure("${doc}" "${line}" "source file not found: ${source}")
                continue()
            endif()
            math(EXPR checked "${checked} + 1")

            # Byte for byte, except that CRLF is turned into LF on both sides
            # (the document's lines are already split on LF).
            file(READ "${SOURCE_DIR}/${path}" content)
            _docs_split(file_lines "${content}")
            _docs_split(block_lines "${found_block_block}")
            if(NOT file_lines STREQUAL block_lines)
                # Both sides are compared with the stand-ins in place.
                _docs_first_difference(index block_text file_text
                    "${block_lines}" "${file_lines}" "<end of block>" "<end of file>")
                math(EXPR at "${index} + 1")
                _docs_add_failure("${doc}" "${line}"
                    "code block differs from ${source} (first difference at line ${at})"
                    "document: ${block_text}"
                    "file: ${file_text}"
                )
            endif()
        endwhile()
    endforeach()

    if(found EQUAL 0)
        list(JOIN FILES ", " files)
        _docs_add_failure("" "" "no clopts-source marker found in: ${files}")
    endif()

    _docs_finish("docs_check: checked ${checked} source marker(s)")
endfunction()

# ---- MODE=links ------------------------------------------------------------

# A character that cannot appear in the documents stands in for a link once
# it is checked, so that a link around an image ("[![a](i)](t)") is found.
string(ASCII 5 _docs_link_done)

# _docs_strip_code_spans(<out_var> <text>)
#
# Sets <out_var> to <text> with every code span replaced by one space and the
# LFs it contained, so that line numbers are kept. A code span opens with a run
# of backticks and closes with the next run of the same length before a blank
# line; a run without such a closing run is literal text.
function(_docs_strip_code_spans out_var text)
    set(result "")
    set(rest "${text}")
    while(rest MATCHES "^([^`]*)(`+)(.*)$")
        string(APPEND result "${CMAKE_MATCH_1}")
        set(run "${CMAKE_MATCH_2}")
        set(after "${CMAKE_MATCH_3}")
        string(LENGTH "${run}" run_length)
        set(scan "${after}")
        set(content "")
        set(closed FALSE)
        while(scan MATCHES "^([^`]*)(`+)(.*)$")
            set(before "${CMAKE_MATCH_1}")
            set(other "${CMAKE_MATCH_2}")
            set(scan_rest "${CMAKE_MATCH_3}")
            string(APPEND content "${before}")
            if(content MATCHES "\n[ \t]*\n")
                break()
            endif()
            string(LENGTH "${other}" other_length)
            if(other_length EQUAL run_length)
                set(closed TRUE)
                break()
            endif()
            string(APPEND content "${other}")
            set(scan "${scan_rest}")
        endwhile()
        if(closed)
            string(REGEX REPLACE "[^\n]" "" newlines "${content}")
            string(APPEND result " ${newlines}")
            set(rest "${scan_rest}")
        else()
            string(APPEND result "${run}")
            set(rest "${after}")
        endif()
    endwhile()
    string(APPEND result "${rest}")
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# _docs_links_in_text(<doc> <doc_dir> <text> <first_line>)
#
# Checks the links of <text>, a paragraph of <doc> (lines joined with LF, no
# code block, stand-ins in place) that starts at line <first_line>, and adds
# their number to the caller's "checked".
function(_docs_links_in_text doc doc_dir text first_line)
    set(link_pattern
        "${_docs_open_bracket}[^${_docs_open_bracket}${_docs_close_bracket}]*${_docs_close_bracket}\\(([^()]*)\\)")
    _docs_strip_code_spans(text "${text}")

    # The first link left is always checked next; it is then replaced by
    # _docs_link_done and the LFs it contained.
    while(text MATCHES "${link_pattern}")
        set(link "${CMAKE_MATCH_0}")
        string(STRIP "${CMAKE_MATCH_1}" target)
        string(FIND "${text}" "${link}" position)
        string(SUBSTRING "${text}" 0 ${position} before)
        string(REGEX REPLACE "[^\n]" "" newlines "${before}")
        string(LENGTH "${newlines}" line)
        math(EXPR line "${first_line} + ${line}")
        string(REGEX REPLACE "[^\n]" "" link_newlines "${link}")
        string(LENGTH "${link}" link_length)
        math(EXPR after_position "${position} + ${link_length}")
        string(SUBSTRING "${text}" ${after_position} -1 after)
        set(text "${before}${_docs_link_done}${link_newlines}${after}")

        # The destination: "<target>", or the first word ("target \"title\"").
        if(target MATCHES "^<([^>]*)>")
            set(target "${CMAKE_MATCH_1}")
        elseif(target MATCHES "^([^ \t\n]*)")
            set(target "${CMAKE_MATCH_1}")
        endif()
        string(TOLOWER "${target}" lower)
        if(lower MATCHES "^(https?://|mailto:)" OR target MATCHES "^#")
            continue()
        endif()
        math(EXPR checked "${checked} + 1")

        string(REGEX REPLACE "#.*$" "" path "${target}")
        _docs_restore(path "${path}")
        if(path STREQUAL "")
            _docs_add_failure("${doc}" "${line}" "broken link: ${target}")
            continue()
        endif()
        if(path MATCHES "^/")
            set(absolute "${SOURCE_DIR}${path}")
        else()
            set(absolute "${doc_dir}/${path}")
        endif()
        get_filename_component(absolute "${absolute}" ABSOLUTE)
        file(RELATIVE_PATH relative "${SOURCE_DIR}" "${absolute}")
        if(relative MATCHES "^docs/dev(/|$)")
            _docs_add_failure("${doc}" "${line}" "link to maintainer docs: ${target}")
        endif()
        if(NOT EXISTS "${absolute}")
            _docs_add_failure("${doc}" "${line}" "broken link: ${target}")
        endif()
    endwhile()
    set(checked ${checked} PARENT_SCOPE)
endfunction()

# _docs_check_links()
#
# MODE=links: checks every relative link ("[t](target)", "![a](target)") of
# FILES outside code spans and code blocks: its target must exist as a file or
# directory (requirement 6.5) and must not be under docs/dev/ (requirement
# 6.2). Links to http://, https:// and mailto: and links to an anchor of the
# same document ("#...") are skipped; the "#..." part of a target is ignored.
# A target starting with "/" is taken from SOURCE_DIR, any other from the
# directory of the document. A link is reported at the line its "[" is on.
function(_docs_check_links)
    set(checked 0)
    foreach(doc IN LISTS FILES)
        _docs_read(lines "${doc}")
        if(lines STREQUAL "NOTFOUND")
            continue()
        endif()
        get_filename_component(doc_dir "${SOURCE_DIR}/${doc}" DIRECTORY)

        # Links and code spans do not go past a blank line: the text is
        # checked a paragraph at a time. A code block (its fences included)
        # ends a paragraph and is not checked.
        list(APPEND lines "")
        set(paragraph "")
        set(paragraph_line 0)
        set(fence_char "")
        set(fence_length "")
        set(line 0)
        foreach(text IN LISTS lines)
            math(EXPR line "${line} + 1")
            set(blank FALSE)
            if(NOT fence_char STREQUAL "")
                _docs_fence_closes(closes "${fence_char}" "${fence_length}" "${text}")
                if(closes)
                    set(fence_char "")
                endif()
                set(blank TRUE)
            else()
                _docs_fence_open(open_char open_length "${text}")
                if(NOT open_char STREQUAL "")
                    set(fence_char "${open_char}")
                    set(fence_length "${open_length}")
                    set(blank TRUE)
                elseif(text MATCHES "^[ \t]*$")
                    set(blank TRUE)
                endif()
            endif()
            if(NOT blank)
                if(paragraph_line EQUAL 0)
                    set(paragraph "${text}")
                    set(paragraph_line ${line})
                else()
                    string(APPEND paragraph "\n${text}")
                endif()
            elseif(paragraph_line GREATER 0)
                _docs_links_in_text("${doc}" "${doc_dir}" "${paragraph}" ${paragraph_line})
                set(paragraph "")
                set(paragraph_line 0)
            endif()
        endforeach()
    endforeach()

    if(checked EQUAL 0)
        list(JOIN FILES ", " files)
        _docs_add_failure("" "" "no relative link found in: ${files}")
    endif()

    _docs_finish("docs_check: checked ${checked} link(s)")
endfunction()

# ---- MODE=api_coverage -----------------------------------------------------

# _docs_check_api_coverage()
#
# MODE=api_coverage: checks that every name of the symbol list SYMBOLS appears
# in FILES in backticks, as `name` or `name()` (requirements 2.9, 8.7). Every
# missing name is reported, at its line of SYMBOLS.
function(_docs_check_api_coverage)
    if(NOT DEFINED SYMBOLS OR SYMBOLS STREQUAL "")
        message(FATAL_ERROR "docs_check: SYMBOLS is required for MODE=api_coverage")
    endif()

    # The documents, as one text each. A missing document is reported by
    # _docs_read; the names are then not looked for (every one would be
    # reported).
    set(texts "")
    set(all_found TRUE)
    foreach(doc IN LISTS FILES)
        _docs_read(lines "${doc}")
        if(lines STREQUAL "NOTFOUND")
            set(all_found FALSE)
            continue()
        endif()
        string(APPEND texts "${lines}\n")
    endforeach()
    list(JOIN FILES ", " files)

    # The symbol list (relative to SOURCE_DIR, or absolute): one name per
    # line, after optional whitespace and before an optional platform tag;
    # blank lines and "#" comments are skipped.
    if(IS_ABSOLUTE "${SYMBOLS}")
        set(symbols_path "${SYMBOLS}")
    else()
        set(symbols_path "${SOURCE_DIR}/${SYMBOLS}")
    endif()
    if(NOT EXISTS "${symbols_path}" OR IS_DIRECTORY "${symbols_path}")
        _docs_add_failure("${SYMBOLS}" "" "file not found (${symbols_path})")
        _docs_finish("")
    endif()
    file(READ "${symbols_path}" content)
    _docs_split(symbol_lines "${content}")

    set(checked 0)
    set(line 0)
    foreach(entry IN LISTS symbol_lines)
        math(EXPR line "${line} + 1")
        string(STRIP "${entry}" entry)
        if(entry STREQUAL "" OR entry MATCHES "^#")
            continue()
        endif()
        string(REGEX REPLACE "[ \t].*$" "" name "${entry}")
        math(EXPR checked "${checked} + 1")
        if(NOT all_found)
            continue()
        endif()
        string(FIND "${texts}" "`${name}`" plain)
        string(FIND "${texts}" "`${name}()`" call)
        if(plain EQUAL -1 AND call EQUAL -1)
            _docs_add_failure("${SYMBOLS}" "${line}" "not documented in ${files}: ${name}")
        endif()
    endforeach()

    if(checked EQUAL 0)
        _docs_add_failure("" "" "no symbol found in: ${SYMBOLS}")
    endif()

    _docs_finish("docs_check: checked ${checked} symbol(s)")
endfunction()

# ---- Main ------------------------------------------------------------------

set_property(GLOBAL PROPERTY _DOCS_REPORT "")
set_property(GLOBAL PROPERTY _DOCS_FAILURES 0)

if(NOT MODE IN_LIST _docs_modes)
    list(JOIN _docs_modes ", " _docs_known)
    message(FATAL_ERROR "docs_check: unknown MODE '${MODE}' (expected one of: ${_docs_known})")
endif()
if(NOT DEFINED SOURCE_DIR OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "docs_check: SOURCE_DIR is not a directory ('${SOURCE_DIR}')")
endif()
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)
if(NOT FILES)
    message(FATAL_ERROR "docs_check: FILES is empty")
endif()

cmake_language(CALL _docs_check_${MODE})
