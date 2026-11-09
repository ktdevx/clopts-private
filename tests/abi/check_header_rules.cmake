# Checks the rules for the public headers:
# CLOPTS_API at the start of every function declaration. Struct and union
# definitions are allowed (a public struct is
# frozen instead, docs/abi.md). Run with `cmake -P`; registered by
# tests/abi/CMakeLists.txt (label: abi).
#
# Variables (-D):
#   HEADER_DIR  Required. The directory of the public headers. Every *.h
#               directly in it is checked (listed again on every run, in name
#               order), so a new header needs no registration.
#
# Each header is normalized first; removed text leaves its line breaks, so line
# numbers stay those of the file:
#   1. /* ... */ comments (also across lines) and // comments up to the end of
#      the line are removed.
#   2. Preprocessor lines (the first non-blank character is "#") are removed,
#      with the lines they continue onto through a trailing backslash.
# The rest is cut into top-level statements by tracking the brace depth:
#   - `extern "C" {` and its matching `}` are transparent (what is inside is
#     top level).
#   - A statement ends at a ";" at depth 0. A statement that starts with
#     "static", or whose text before its body contains "(" (a function
#     definition), ends where its body { ... } closes.
# Each statement is classified by its head (the text before its first "{", or
# the whole statement when it has no body):
#   - starting with typedef (forward declarations, struct / union / enum
#     definitions, function pointer types)
#                                      -> allowed
#   - otherwise starting with static (static inline definitions)
#                                      -> not checked for CLOPTS_API
#   - otherwise containing "<identifier> (" (a function declaration): the name
#     is the identifier just before the first "("; the first word must be
#     CLOPTS_API                       -> else "function '<name>' lacks CLOPTS_API"
#   - anything else (struct / union / enum definitions without typedef,
#     struct forward declarations)     -> not checked
# Every violation is collected and reported as <file>:<line>: <reason> (the
# line where the statement starts) before the script fails. Without violations
# the numbers of headers and function declarations checked are printed.
#
# Not handled (extend the scanner if one of these appears in a public header):
#   - functions returning a function pointer, e.g. `int (*f(void))(int);`
#     (the name found is the return type) and extern variables of function
#     pointer type (reported as a function without CLOPTS_API);
#   - extern variables, which are not checked at all;
#   - declarations wrapped in macros, e.g. `DECLARE_FN(name);` (taken as a
#     function named DECLARE_FN);
#   - "{", "}", ";", "/*" or "//" inside string or character literals (only
#     extern "C" is expected to hold a string literal);
#   - a backslash-newline that continues a // comment or splits a token, and
#     trigraphs / digraphs;
#   - braces that only balance across #if / #else branches.
#
# The script only reads files; it has no side effects.

cmake_minimum_required(VERSION 3.21)

if("${HEADER_DIR}" STREQUAL "" OR NOT IS_DIRECTORY "${HEADER_DIR}")
    message(FATAL_ERROR " check_header_rules: HEADER_DIR is not a directory ('${HEADER_DIR}')")
endif()

# Characters that cannot appear in a header stand in for ";", "[", "]" and
# "\" while a file is handled as a CMake list of lines.
string(ASCII 1 _semicolon)
string(ASCII 2 _open_bracket)
string(ASCII 3 _close_bracket)
string(ASCII 4 _backslash)

set(_ident "[A-Za-z_][A-Za-z0-9_]*")

# _check_header_rules_code_lines(<out_var> <file>)
#
# Sets <out_var> to the lines of <file> as a list (the position in the list is
# the line number minus one), with comments and preprocessor lines blanked and
# the stand-ins above in place of ";", "[", "]" and "\".
function(_check_header_rules_code_lines out_var file)
    file(READ "${file}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE "\\" "${_backslash}" content "${content}")
    string(REPLACE ";" "${_semicolon}" content "${content}")
    string(REPLACE "[" "${_open_bracket}" content "${content}")
    string(REPLACE "]" "${_close_bracket}" content "${content}")
    string(REPLACE "\n" ";" content "${content}")

    set(result "")
    set(in_block_comment FALSE)
    set(in_directive FALSE)
    foreach(line IN LISTS content)
        # 1. Comments.
        set(code "")
        set(rest "${line}")
        while(NOT "${rest}" STREQUAL "")
            if(in_block_comment)
                string(FIND "${rest}" "*/" end)
                if(end EQUAL -1)
                    set(rest "")
                else()
                    math(EXPR end "${end} + 2")
                    string(SUBSTRING "${rest}" ${end} -1 rest)
                    set(in_block_comment FALSE)
                endif()
            else()
                string(FIND "${rest}" "/*" block)
                string(FIND "${rest}" "//" line_comment)
                if(NOT line_comment EQUAL -1 AND (block EQUAL -1 OR line_comment LESS block))
                    string(SUBSTRING "${rest}" 0 ${line_comment} head)
                    string(APPEND code "${head}")
                    set(rest "")
                elseif(NOT block EQUAL -1)
                    string(SUBSTRING "${rest}" 0 ${block} head)
                    string(APPEND code "${head} ")
                    math(EXPR block "${block} + 2")
                    string(SUBSTRING "${rest}" ${block} -1 rest)
                    set(in_block_comment TRUE)
                else()
                    string(APPEND code "${rest}")
                    set(rest "")
                endif()
            endif()
        endwhile()

        # 2. Preprocessor lines and their continuation lines.
        if(NOT in_directive AND code MATCHES "^[ \t]*#")
            set(in_directive TRUE)
        endif()
        if(in_directive)
            if(NOT code MATCHES "${_backslash}[ \t]*$")
                set(in_directive FALSE)
            endif()
            set(code "")
        endif()

        # The leading space keeps a blank first line from being dropped by
        # list(APPEND) on an empty list.
        list(APPEND result " ${code}")
    endforeach()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

set(_violations "")
set(_function_count 0)

# _check_header_rules_classify(<file>)
#
# Classifies the statement held in _stmt_head / _stmt_has_body / _stmt_line
# (variables of the calling scope) and records a violation or a function.
macro(_check_header_rules_classify file)
    string(REGEX REPLACE "[ \t]+" " " _head "${_stmt_head}")
    string(STRIP "${_head}" _head)
    if(_head MATCHES "^typedef([^A-Za-z0-9_]|$)")
        # Forward declarations, struct / union / enum definitions and function pointer types.
    elseif(_head MATCHES "^static([^A-Za-z0-9_]|$)")
        # static inline definitions are not exported.
    elseif(_head MATCHES "${_ident} ?\\(")
        math(EXPR _function_count "${_function_count} + 1")
        string(FIND "${_head}" "(" _paren)
        string(SUBSTRING "${_head}" 0 ${_paren} _before)
        set(_name "")
        if(_before MATCHES "(${_ident}) ?$")
            set(_name "${CMAKE_MATCH_1}")
        endif()
        if(NOT _head MATCHES "^CLOPTS_API([^A-Za-z0-9_]|$)")
            list(APPEND _violations "${file}:${_stmt_line}: function '${_name}' lacks CLOPTS_API")
        endif()
    endif()
endmacro()

file(GLOB _headers LIST_DIRECTORIES false "${HEADER_DIR}/*.h")
list(SORT _headers)
list(LENGTH _headers _header_count)
if(_header_count EQUAL 0)
    message(FATAL_ERROR " check_header_rules: no *.h in ${HEADER_DIR}")
endif()

foreach(_file IN LISTS _headers)
    _check_header_rules_code_lines(_lines "${_file}")

    set(_extern_c_open 0)    # number of open extern "C" { blocks
    set(_depth 0)            # brace depth inside the current statement
    set(_stmt_head "")       # statement text before its first "{"
    set(_stmt_line 0)        # line where the statement starts
    set(_stmt_has_body FALSE)
    set(_line_number 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _line_number "${_line_number} + 1")
        if(_line MATCHES "^[ \t]*$")
            continue()
        endif()
        string(REGEX MATCHALL "[^{}${_semicolon}]+|[{}${_semicolon}]" _tokens "${_line}")
        foreach(_token IN LISTS _tokens)
            if(_depth GREATER 0)
                # Inside a body: only the depth matters.
                if(_token STREQUAL "{")
                    math(EXPR _depth "${_depth} + 1")
                elseif(_token STREQUAL "}")
                    math(EXPR _depth "${_depth} - 1")
                    if(_depth EQUAL 0)
                        string(STRIP "${_stmt_head}" _h)
                        if(_h MATCHES "^static([^A-Za-z0-9_]|$)" OR _h MATCHES "\\(")
                            _check_header_rules_classify("${_file}")
                            set(_stmt_head "")
                            set(_stmt_has_body FALSE)
                        endif()
                    endif()
                endif()
            elseif(_token STREQUAL "{")
                string(REGEX REPLACE "[ \t]+" " " _h "${_stmt_head}")
                string(STRIP "${_h}" _h)
                if(NOT _stmt_has_body AND _h MATCHES "^extern ?\"C\"$")
                    math(EXPR _extern_c_open "${_extern_c_open} + 1")
                    set(_stmt_head "")
                else()
                    if(_stmt_head MATCHES "^[ \t]*$")
                        set(_stmt_line ${_line_number})
                    endif()
                    set(_stmt_has_body TRUE)
                    set(_depth 1)
                endif()
            elseif(_token STREQUAL "}")
                if(_extern_c_open GREATER 0 AND _stmt_head MATCHES "^[ \t]*$" AND NOT _stmt_has_body)
                    math(EXPR _extern_c_open "${_extern_c_open} - 1")
                else()
                    list(APPEND _violations "${_file}:${_line_number}: unbalanced '}'")
                endif()
            elseif(_token STREQUAL "${_semicolon}")
                if(_stmt_has_body OR NOT _stmt_head MATCHES "^[ \t]*$")
                    _check_header_rules_classify("${_file}")
                endif()
                set(_stmt_head "")
                set(_stmt_has_body FALSE)
            elseif(NOT _stmt_has_body)
                if(_stmt_head MATCHES "^[ \t]*$" AND NOT _token MATCHES "^[ \t]*$")
                    set(_stmt_line ${_line_number})
                endif()
                string(APPEND _stmt_head " ${_token}")
            endif()
        endforeach()
    endforeach()
    if(_depth GREATER 0 OR _stmt_has_body OR NOT _stmt_head MATCHES "^[ \t]*$")
        list(APPEND _violations "${_file}:${_stmt_line}: unterminated statement")
    endif()
    if(_extern_c_open GREATER 0)
        list(APPEND _violations "${_file}:${_line_number}: unclosed extern \"C\" block")
    endif()
endforeach()

list(LENGTH _violations _violation_count)
if(_violation_count GREATER 0)
    list(JOIN _violations "\n " _report)
    string(REPLACE "${_semicolon}" ";" _report "${_report}")
    string(REPLACE "${_open_bracket}" "[" _report "${_report}")
    string(REPLACE "${_close_bracket}" "]" _report "${_report}")
    string(REPLACE "${_backslash}" "\\" _report "${_report}")
    message(FATAL_ERROR
        " check_header_rules: ${_violation_count} violation(s) in the public headers under ${HEADER_DIR}"
        " (CLOPTS_API on every function):\n ${_report}")
endif()
message(STATUS
    "check_header_rules: ${_header_count} header(s), ${_function_count} function declaration(s) checked"
    " in ${HEADER_DIR}; no violation")
