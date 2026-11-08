# Checks the public symbol list (abi/clopts.symbols) and compares it with the
# symbols the shared library exports (requirements 2.3, 2.4, 3.4-3.7, 3.9).
# Run with `cmake -P`; registered by tests/abi/CMakeLists.txt (label: abi).
#
# Variables (-D):
#   SYMBOLS_FILE  Always. Path of the public symbol list.
#   FORMAT        When DUMP_FILE (or LIBRARY) is given. The format of the
#                 export dump: nm-elf (nm-macho, dumpbin, objdump-pe: not yet).
#   DUMP_FILE     A file holding the recorded output of the export dump tool
#                 (used by the self-tests in place of LIBRARY and TOOL).
#   LIBRARY       The shared library to check (running TOOL on it is not
#                 supported yet; pass DUMP_FILE).
#
# Without DUMP_FILE and LIBRARY only the format of the list is checked.
#
# List format: one name per line; leading/trailing whitespace, blank lines and
# lines starting with "#" are ignored. A name must match ^clopts_[a-z0-9_]+$
# and must not repeat. A name may carry one platform tag after it; the only tag
# is "windows" (the name is expected only by the Windows formats dumpbin and
# objdump-pe). Every format error is collected and reported as
# <SYMBOLS_FILE>:<line>: <reason> before the script fails.
#
# The script only reads files; it has no side effects.

cmake_minimum_required(VERSION 3.21)

# ---- Exclusion lists (names removed from the dump before comparing) -------
# Each list holds names the toolchain adds to the shared library and that do
# not come from the CLOpts sources (requirement 3.4). A listed name is removed
# when present and is not required to be present. Add a name here only with
# the reason it is not a CLOpts symbol.
set(_check_symbols_exclude_nm-elf
    _init        # ELF: DT_INIT function emitted by crti.o / the linker
    _fini        # ELF: DT_FINI function emitted by crti.o / the linker
    _edata       # ELF: end-of-data marker defined by the linker script
    _end         # ELF: end-of-bss marker defined by the linker script
    __bss_start  # ELF: start-of-bss marker defined by the linker script
)

# Formats the comparison knows about, and those of them whose exports are
# Windows ones (names with the "windows" tag are expected only there).
set(_check_symbols_formats nm-elf nm-macho dumpbin objdump-pe)
set(_check_symbols_windows_formats dumpbin objdump-pe)

# Characters that cannot appear in the files stand in for ";", "[", "]" and
# "\" while a file is handled as a CMake list of lines.
string(ASCII 1 _check_symbols_semicolon)
string(ASCII 2 _check_symbols_open_bracket)
string(ASCII 3 _check_symbols_close_bracket)
string(ASCII 4 _check_symbols_backslash)

# _check_symbols_lines(<out_var> <file>)
#
# Sets <out_var> to the lines of <file> as a list (empty lines kept, so that
# the position in the list is the line number minus one), with the special
# characters replaced by the stand-ins above.
function(_check_symbols_lines out_var file)
    file(READ "${file}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE "\\" "${_check_symbols_backslash}" content "${content}")
    string(REPLACE ";" "${_check_symbols_semicolon}" content "${content}")
    string(REPLACE "[" "${_check_symbols_open_bracket}" content "${content}")
    string(REPLACE "]" "${_check_symbols_close_bracket}" content "${content}")
    string(REPLACE "\n" ";" content "${content}")
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

# _check_symbols_restore(<out_var> <text>)
#
# Sets <out_var> to <text> with the stand-ins turned back into the original
# characters. Used only on the final report text (a restored ";" would split a
# list element).
function(_check_symbols_restore out_var text)
    string(REPLACE "${_check_symbols_backslash}" "\\" text "${text}")
    string(REPLACE "${_check_symbols_semicolon}" ";" text "${text}")
    string(REPLACE "${_check_symbols_open_bracket}" "[" text "${text}")
    string(REPLACE "${_check_symbols_close_bracket}" "]" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# _check_symbols_fail(<report>)
#
# Fails with the multi-line <report> printed as written: the stand-ins are
# turned back into the original characters, and every line is prefixed with
# one space, which keeps message() from re-wrapping it.
function(_check_symbols_fail report)
    _check_symbols_restore(report "${report}")
    string(REPLACE "\n" "\n " report "${report}")
    message(FATAL_ERROR " ${report}")
endfunction()

# ---- Extraction, one function per FORMAT ----------------------------------
# Each _check_symbols_extract_<format>(<out_var> <lines>) sets <out_var> to the
# exported names found in <lines> (the dump as a list of lines), before the
# exclusion list is applied.

# nm-elf: output of `nm -D --defined-only <library>`. Each symbol is a line of
# three columns "<value> <type> <name>"; the name is the third column. Lines
# of any other shape are ignored.
function(_check_symbols_extract_nm-elf out_var lines)
    set(names "")
    foreach(line IN LISTS lines)
        string(REGEX MATCHALL "[^ \t]+" words "${line}")
        list(LENGTH words count)
        if(count EQUAL 3)
            list(GET words 2 name)
            list(APPEND names "${name}")
        endif()
    endforeach()
    set(${out_var} "${names}" PARENT_SCOPE)
endfunction()

# ---- Inputs ----------------------------------------------------------------
if("${SYMBOLS_FILE}" STREQUAL "" OR NOT EXISTS "${SYMBOLS_FILE}" OR IS_DIRECTORY "${SYMBOLS_FILE}")
    message(FATAL_ERROR "check_symbols: SYMBOLS_FILE is not a file ('${SYMBOLS_FILE}')")
endif()

set(_compare FALSE)
if(NOT "${DUMP_FILE}" STREQUAL "" OR NOT "${LIBRARY}" STREQUAL "")
    set(_compare TRUE)
    if("${FORMAT}" STREQUAL "")
        message(FATAL_ERROR "check_symbols: FORMAT is required when DUMP_FILE or LIBRARY is given")
    endif()
    if(NOT FORMAT IN_LIST _check_symbols_formats)
        list(JOIN _check_symbols_formats ", " _known)
        message(FATAL_ERROR "check_symbols: unknown FORMAT '${FORMAT}' (expected one of: ${_known})")
    endif()
endif()

# ---- The public symbol list ------------------------------------------------
# _listed:  every valid name in the list (tagged or not)
# _tagged:  the names that carry the "windows" tag
set(_listed "")
set(_tagged "")
set(_list_errors "")
_check_symbols_lines(_lines "${SYMBOLS_FILE}")
set(_line_number 0)
foreach(_line IN LISTS _lines)
    math(EXPR _line_number "${_line_number} + 1")
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "" OR _line MATCHES "^#")
        continue()
    endif()
    string(REGEX MATCHALL "[^ \t]+" _words "${_line}")
    list(GET _words 0 _name)
    list(LENGTH _words _word_count)
    set(_where "${SYMBOLS_FILE}:${_line_number}")
    set(_valid TRUE)
    if(NOT _name MATCHES "^clopts_[a-z0-9_]+$")
        list(APPEND _list_errors "${_where}: invalid symbol name '${_name}'")
        set(_valid FALSE)
    elseif(_name IN_LIST _listed)
        list(APPEND _list_errors "${_where}: duplicate symbol '${_name}'")
        set(_valid FALSE)
    endif()
    if(_word_count GREATER 1)
        list(SUBLIST _words 1 -1 _tag)
        list(JOIN _tag " " _tag)
        if(NOT _tag STREQUAL "windows")
            list(APPEND _list_errors "${_where}: invalid platform tag '${_tag}'")
            set(_valid FALSE)
        endif()
    endif()
    if(_valid)
        list(APPEND _listed "${_name}")
        if(_word_count GREATER 1)
            list(APPEND _tagged "${_name}")
        endif()
    endif()
endforeach()

list(LENGTH _list_errors _count)
if(_count GREATER 0)
    list(JOIN _list_errors "\n  " _report)
    string(CONCAT _report
        "check_symbols: ${_count} error(s) in the public symbol list ${SYMBOLS_FILE}:\n"
        "  ${_report}\n"
        "Names must match ^clopts_[a-z0-9_]+$, appear once, and carry at most the\n"
        "platform tag 'windows'. See docs/dev/abi-checks.md.")
    _check_symbols_fail("${_report}")
endif()

list(LENGTH _listed _listed_count)
if(NOT _compare)
    message(STATUS "check_symbols: ${SYMBOLS_FILE} is well-formed (${_listed_count} symbols)")
    return()
endif()

# ---- The expected set for FORMAT --------------------------------------------
set(_expected "${_listed}")
list(LENGTH _tagged _tagged_count)
if(NOT FORMAT IN_LIST _check_symbols_windows_formats AND _tagged_count GREATER 0)
    list(REMOVE_ITEM _expected ${_tagged})
endif()

# ---- The exported set --------------------------------------------------------
if(NOT "${DUMP_FILE}" STREQUAL "")
    if(NOT EXISTS "${DUMP_FILE}" OR IS_DIRECTORY "${DUMP_FILE}")
        message(FATAL_ERROR "check_symbols: DUMP_FILE is not a file ('${DUMP_FILE}')")
    endif()
    set(_source "${DUMP_FILE}")
    _check_symbols_lines(_dump "${DUMP_FILE}")
else()
    message(FATAL_ERROR
        "check_symbols: running an export dump tool on LIBRARY ('${LIBRARY}') is not "
        "supported by this script yet; pass DUMP_FILE with the recorded tool output")
endif()

if(NOT COMMAND _check_symbols_extract_${FORMAT})
    message(FATAL_ERROR "check_symbols: FORMAT '${FORMAT}' is not supported by this script yet")
endif()
cmake_language(CALL _check_symbols_extract_${FORMAT} _exported "${_dump}")
list(LENGTH _exported _exported_count)
if(_exported_count EQUAL 0)
    message(FATAL_ERROR
        "check_symbols: no symbol names found in the ${FORMAT} output of ${_source}; "
        "the output does not have the expected format")
endif()
list(LENGTH _check_symbols_exclude_${FORMAT} _exclude_count)
if(_exclude_count GREATER 0)
    list(REMOVE_ITEM _exported ${_check_symbols_exclude_${FORMAT}})
endif()
list(REMOVE_DUPLICATES _exported)

# ---- Compare and report --------------------------------------------------
set(_missing "")
foreach(_name IN LISTS _expected)
    if(NOT _name IN_LIST _exported)
        list(APPEND _missing "${_name}")
    endif()
endforeach()
set(_extra "")
foreach(_name IN LISTS _exported)
    if(NOT _name IN_LIST _expected)
        list(APPEND _extra "${_name}")
    endif()
endforeach()

# Emptiness is decided by list length, never by if(<list>): a name such as
# "off" or "no" would read as a false constant and hide the mismatch.
list(LENGTH _missing _missing_count)
list(LENGTH _extra _extra_count)
if(_missing_count GREATER 0 OR _extra_count GREATER 0)
    set(_report "abi symbol check failed (${FORMAT}, ${_source})\n")
    if(_missing_count GREATER 0)
        list(SORT _missing)
        list(JOIN _missing "\n    " _names)
        string(APPEND _report "  missing (in the list but not exported):\n    ${_names}\n")
    endif()
    if(_extra_count GREATER 0)
        list(SORT _extra)
        list(JOIN _extra "\n    " _names)
        string(APPEND _report "  extra (exported but not in the list):\n    ${_names}\n")
    endif()
    string(APPEND _report
        "The public symbol list is ${SYMBOLS_FILE}.\n"
        "If the change is intended, update the list; otherwise stop exporting the symbol\n"
        "(internal functions must not carry CLOPTS_API). See docs/dev/abi-checks.md.")
    _check_symbols_fail("${_report}")
endif()

list(LENGTH _expected _expected_count)
message(STATUS "check_symbols: ${_expected_count} symbols match (${FORMAT}, ${_source})")
