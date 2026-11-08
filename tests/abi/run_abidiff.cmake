# Compares the ABI of the current sources with the latest release of the same
# SOVERSION using abidiff (requirements 6.2-6.8). Run with `cmake -P`.
#
# Variables (-D):
#   MODE         compare (the default) or self_test.
#   SOURCE_DIR   MODE=compare. The repository to compare.
#   WORK_DIR     MODE=compare. Scratch directory; removed and re-created.
#   C_COMPILER   MODE=compare, optional. The C compiler (default: env CC).
#   GENERATOR    MODE=compare, optional. The CMake generator.
#
# MODE=self_test checks select_baseline and classify_exit_code against the
# tables in _run_abidiff_self_test. It uses no git, no build and no abidiff;
# it is registered as the CTest test abi.compare_selftest.
#
# Output: the first line is "abi-compare: <state>: <reason>". Success exits
# with 0; every failure is reported with message(FATAL_ERROR).

cmake_minimum_required(VERSION 3.21)

# select_baseline(<current_version> <tags> <out_var>)
#
# Sets <out_var> to the release tag to compare against, or to "" when there is
# none (requirements 6.2, 6.3). <current_version> is "x.y.z"; <tags> is a list
# of tag names. Only tags matching ^v([0-9]+)\.([0-9]+)\.([0-9]+)$ are
# candidates (so "v1.2.0-rc1" is not). Among the candidates with the same
# major as the current version (the same SOVERSION) and a version not above
# the current one, the highest version wins; versions compare numerically
# (v1.10.0 > v1.9.0). A current version before 1.0.0 always gives "".
function(select_baseline current_version tags out_var)
    if(NOT current_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        message(FATAL_ERROR
            "abi-compare: tool error: invalid current version '${current_version}' (expected x.y.z)")
    endif()
    set(current_major "${CMAKE_MATCH_1}")
    set(best_tag "")
    set(best_version "")
    if(current_major GREATER 0)
        foreach(tag IN LISTS tags)
            if(NOT tag MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
                continue()
            endif()
            if(NOT CMAKE_MATCH_1 EQUAL current_major)
                continue()
            endif()
            set(version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
            if(version VERSION_GREATER current_version)
                continue()
            endif()
            if(best_version STREQUAL "" OR version VERSION_GREATER best_version)
                set(best_tag "${tag}")
                set(best_version "${version}")
            endif()
        endforeach()
    endif()
    set(${out_var} "${best_tag}" PARENT_SCOPE)
endfunction()

# classify_exit_code(<code> <out_var>)
#
# Sets <out_var> to compatible, incompatible or tool_error from an abidiff
# exit code (requirements 6.6-6.8). libabigail's bits: 1 = error,
# 2 = usage error, 4 = ABI change, 8 = incompatible ABI change. The rules are
# applied in this order (design "AbiCompareScript", exit-code table):
#   code & 1 or code & 2   -> tool_error
#   otherwise code & 8     -> incompatible (also when unknown bits are set,
#                             e.g. 24 = 8|16; the job fails either way)
#   code is 0 or 4         -> compatible
#   otherwise (unknown bit)-> tool_error
# A code that is not a non-negative integer is a tool_error.
function(classify_exit_code code out_var)
    if(NOT code MATCHES "^[0-9]+$")
        set(${out_var} "tool_error" PARENT_SCOPE)
        return()
    endif()
    math(EXPR tool_bits "${code} & 3")
    math(EXPR incompatible_bit "${code} & 8")
    if(NOT tool_bits EQUAL 0)
        set(result "tool_error")
    elseif(NOT incompatible_bit EQUAL 0)
        set(result "incompatible")
    elseif(code EQUAL 0 OR code EQUAL 4)
        set(result "compatible")
    else()
        set(result "tool_error")
    endif()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# ---- MODE=self_test --------------------------------------------------------

# Rows are "<name>|<current version>|<tags, comma-separated>|<expected tag>"
# and "<name>|<exit code>|<expected result>". "," cannot appear in a tag name
# that select_baseline accepts, so it is a safe separator here.
set(_run_abidiff_baseline_cases
    "0.x gives empty|0.9.0|v0.1.0,v0.8.0,v0.9.0|"
    "same major, highest|1.4.2|v1.0.0,v1.2.0,v1.3.5,v1.1.9|v1.3.5"
    "numeric compare v1.10.0 > v1.9.0|1.11.0|v1.9.0,v1.10.0,v1.2.0|v1.10.0"
    "numeric compare in patch|1.0.20|v1.0.9,v1.0.10,v1.0.3|v1.0.10"
    "tag equal to current is allowed|1.2.0|v1.1.0,v1.2.0|v1.2.0"
    "newer tags excluded|1.2.0|v1.1.0,v1.2.1,v1.3.0,v1.10.0|v1.1.0"
    "other major excluded|2.1.0|v1.9.0,v3.0.0,v2.0.0|v2.0.0"
    "only other major gives empty|2.1.0|v1.9.0,v3.0.0|"
    "-rc excluded|1.3.0|v1.2.0,v1.3.0-rc1,v1.2.5-rc2|v1.2.0"
    "non-matching names excluded|1.5.0|1.4.0,v1.4,release-1.4.0,v1.4.0.1,V1.4.0,xv1.4.0,v1.1.0|v1.1.0"
    "no tags gives empty|1.0.0||"
    "no candidates gives empty|1.0.0|v1.0.0-rc1,v1.0.1,v0.9.0,latest|"
)
set(_run_abidiff_exit_code_cases
    "0|compatible"
    "4|compatible"
    "12|incompatible"
    "8|incompatible"
    "24|incompatible"
    "1|tool_error"
    "2|tool_error"
    "3|tool_error"
    "13|tool_error"
    "16|tool_error"
    "20|tool_error"
)

function(_run_abidiff_self_test)
    set(failures "")
    set(count 0)

    set(row 0)
    foreach(case IN LISTS _run_abidiff_baseline_cases)
        math(EXPR row "${row} + 1")
        math(EXPR count "${count} + 1")
        string(REPLACE "|" ";" fields "${case}")
        list(LENGTH fields field_count)
        # list(LENGTH) counts empty elements, so an empty expected value
        # still gives 4 fields.
        if(NOT field_count EQUAL 4)
            list(APPEND failures
                " select_baseline row ${row}: malformed row '${case}'")
            continue()
        endif()
        list(GET fields 0 name)
        list(GET fields 1 current)
        list(GET fields 2 tags)
        list(GET fields 3 expected)
        set(tag_text "${tags}")
        string(REPLACE "," ";" tags "${tags}")
        select_baseline("${current}" "${tags}" actual)
        if(NOT actual STREQUAL expected)
            list(APPEND failures
                " select_baseline row ${row} (${name}): current ${current}, tags [${tag_text}] -> got '${actual}', expected '${expected}'")
        endif()
    endforeach()

    set(row 0)
    foreach(case IN LISTS _run_abidiff_exit_code_cases)
        math(EXPR row "${row} + 1")
        math(EXPR count "${count} + 1")
        string(REPLACE "|" ";" fields "${case}")
        list(LENGTH fields field_count)
        if(NOT field_count EQUAL 2)
            list(APPEND failures
                " classify_exit_code row ${row}: malformed row '${case}'")
            continue()
        endif()
        list(GET fields 0 code)
        list(GET fields 1 expected)
        classify_exit_code("${code}" actual)
        if(NOT actual STREQUAL expected)
            list(APPEND failures
                " classify_exit_code row ${row}: exit code ${code} -> got '${actual}', expected '${expected}'")
        endif()
    endforeach()

    list(LENGTH failures failure_count)
    if(NOT failure_count EQUAL 0)
        string(REPLACE ";" "\n" report "${failures}")
        message(FATAL_ERROR
            " abi-compare self-test: ${failure_count} of ${count} cases failed:\n${report}")
    endif()
    message(STATUS "abi-compare self-test: all ${count} cases passed")
endfunction()

# ---- MODE=compare ----------------------------------------------------------

function(_run_abidiff_compare)
    message(FATAL_ERROR
        "abi-compare: tool error: MODE=compare is not implemented yet")
endfunction()

# ---- Entry point ------------------------------------------------------------

if(NOT DEFINED MODE OR MODE STREQUAL "")
    set(MODE "compare")
endif()
if(MODE STREQUAL "self_test")
    _run_abidiff_self_test()
elseif(MODE STREQUAL "compare")
    _run_abidiff_compare()
else()
    message(FATAL_ERROR
        "abi-compare: tool error: unknown MODE '${MODE}' (expected compare or self_test)")
endif()
