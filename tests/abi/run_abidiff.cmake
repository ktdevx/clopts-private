# Compares the ABI of the current sources with the latest release of the same
# SOVERSION using abidiff. Run with `cmake -P`.
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
# none. <current_version> is "x.y.z"; <tags> is a list
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
# exit code. libabigail's bits: 1 = error,
# 2 = usage error, 4 = ABI change, 8 = incompatible ABI change. The rules are
# applied in this order:
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

# Prints the status line "abi-compare: <state>: <reason>", then <details>
# (if any) and fails. Nothing else is printed before the status line, so it is
# the first line of the output.
function(_run_abidiff_fail state reason details)
    message("abi-compare: ${state}: ${reason}")
    if(NOT "${details}" STREQUAL "")
        message("${details}")
    endif()
    message(FATAL_ERROR " abi-compare: ${state}: ${reason}")
endfunction()

# Runs a command and stores its exit status and combined output.
function(_run_abidiff_run out_result out_output)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_output} "${output}" PARENT_SCOPE)
endfunction()

# _run_abidiff_build(<side> <source_dir> <work_dir> <out_prefix>)
#
# Configures, builds and installs <source_dir> as a shared library with debug
# information into <work_dir>/<side>-prefix. Both sides use
# the same generator (GENERATOR) and C compiler (C_COMPILER, else env CC).
# Fails with the output of the failing step.
function(_run_abidiff_build side source_dir work_dir out_prefix)
    set(build_dir "${work_dir}/${side}-build")
    set(prefix_dir "${work_dir}/${side}-prefix")

    set(configure_args
        -S "${source_dir}" -B "${build_dir}"
        -DCLOPTS_BUILD_SHARED=ON
        -DCLOPTS_BUILD_TESTING=OFF
        -DCLOPTS_INSTALL=ON
        -DCMAKE_BUILD_TYPE=Debug
        "-DCMAKE_INSTALL_PREFIX=${prefix_dir}")
    if(NOT "${GENERATOR}" STREQUAL "")
        list(APPEND configure_args -G "${GENERATOR}")
    endif()
    set(compiler "${C_COMPILER}")
    if("${compiler}" STREQUAL "")
        set(compiler "$ENV{CC}")
    endif()
    if(NOT "${compiler}" STREQUAL "")
        list(APPEND configure_args "-DCMAKE_C_COMPILER=${compiler}")
    endif()

    _run_abidiff_run(result output "${CMAKE_COMMAND}" ${configure_args})
    if(NOT result EQUAL 0)
        _run_abidiff_fail("tool error"
            "configuring the ${side} sources failed (exit status ${result})" "${output}")
    endif()
    _run_abidiff_run(result output
        "${CMAKE_COMMAND}" --build "${build_dir}" --config Debug --parallel)
    if(NOT result EQUAL 0)
        _run_abidiff_fail("tool error"
            "building the ${side} sources failed (exit status ${result})" "${output}")
    endif()
    _run_abidiff_run(result output
        "${CMAKE_COMMAND}" --install "${build_dir}" --config Debug)
    if(NOT result EQUAL 0)
        _run_abidiff_fail("tool error"
            "installing the ${side} build failed (exit status ${result})" "${output}")
    endif()
    set(${out_prefix} "${prefix_dir}" PARENT_SCOPE)
endfunction()

# _run_abidiff_find_library(<side> <prefix> <out_var>)
#
# Sets <out_var> to the one real (non-symlink) libclopts.so.* under
# <prefix>/lib*/. Fails when there is none or more than one.
function(_run_abidiff_find_library side prefix out_var)
    file(GLOB candidates LIST_DIRECTORIES false "${prefix}/lib*/libclopts.so.*")
    set(found "")
    foreach(candidate IN LISTS candidates)
        if(NOT IS_SYMLINK "${candidate}")
            list(APPEND found "${candidate}")
        endif()
    endforeach()
    list(LENGTH found found_count)
    if(NOT found_count EQUAL 1)
        string(REPLACE ";" "\n " found_text "${found}")
        _run_abidiff_fail("tool error"
            "expected exactly one non-symlink libclopts.so.* under ${prefix}/lib*/ for the ${side} build, found ${found_count}"
            " ${found_text}")
    endif()
    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

function(_run_abidiff_compare)
    foreach(var IN ITEMS SOURCE_DIR WORK_DIR)
        if("${${var}}" STREQUAL "")
            _run_abidiff_fail("tool error"
                "required variable ${var} is not set (see tests/abi/run_abidiff.cmake)" "")
        endif()
    endforeach()
    get_filename_component(source_dir "${SOURCE_DIR}" ABSOLUTE)
    get_filename_component(work_dir "${WORK_DIR}" ABSOLUTE)

    # WORK_DIR is removed below; never let that touch the repository.
    file(RELATIVE_PATH source_from_work "${work_dir}" "${source_dir}")
    file(RELATIVE_PATH work_from_source "${source_dir}" "${work_dir}")
    if(NOT source_from_work MATCHES "^\\.\\.(/|$)" OR work_from_source STREQUAL "")
        _run_abidiff_fail("tool error"
            "WORK_DIR '${work_dir}' must not contain or be SOURCE_DIR '${source_dir}'" "")
    endif()

    file(REMOVE_RECURSE "${work_dir}")
    file(MAKE_DIRECTORY "${work_dir}")

    # 1. The current version from project(clopts VERSION x.y.z ...).
    set(lists_file "${source_dir}/CMakeLists.txt")
    if(NOT EXISTS "${lists_file}")
        _run_abidiff_fail("tool error"
            "cannot read the version: ${lists_file} does not exist" "")
    endif()
    file(READ "${lists_file}" lists_text)
    if(NOT lists_text MATCHES
            "project[ \t\r\n]*\\([ \t\r\n]*clopts[ \t\r\n]+VERSION[ \t\r\n]+([0-9]+)\\.([0-9]+)\\.([0-9]+)[ \t\r\n)]")
        _run_abidiff_fail("tool error"
            "cannot read the version from project(clopts VERSION x.y.z ...) in ${lists_file}" "")
    endif()
    set(current_major "${CMAKE_MATCH_1}")
    set(current_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

    # 2. Nothing to compare against before 1.0.0.
    if(current_major EQUAL 0)
        message("abi-compare: skipped: version ${current_version} is before 1.0.0")
        return()
    endif()

    # 3. The baseline tag.
    find_program(_run_abidiff_git git)
    if(NOT _run_abidiff_git)
        _run_abidiff_fail("tool error" "git was not found" "")
    endif()
    execute_process(
        COMMAND "${_run_abidiff_git}" -C "${source_dir}" tag --list "v*"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE tag_output
        ERROR_VARIABLE tag_error
    )
    if(NOT result EQUAL 0)
        _run_abidiff_fail("tool error"
            "git tag --list failed in ${source_dir} (exit status ${result})"
            "${tag_output}${tag_error}")
    endif()
    # One tag per line. A line with ";" would split into several list
    # elements, so drop it (no such name can be a release tag anyway).
    string(REGEX REPLACE "[^\n]*;[^\n]*" "" tag_output "${tag_output}")
    string(REGEX REPLACE "[\r\n]+" ";" tags "${tag_output}")
    select_baseline("${current_version}" "${tags}" baseline_tag)
    if("${baseline_tag}" STREQUAL "")
        message("abi-compare: skipped: no release tag with SOVERSION ${current_major}")
        return()
    endif()

    # abidiff is required from here on; check it before
    # spending time on the builds.
    find_program(_run_abidiff_abidiff abidiff)
    if(NOT _run_abidiff_abidiff)
        _run_abidiff_fail("tool error"
            "abidiff was not found (install libabigail, e.g. the abigail-tools package)" "")
    endif()

    # 4. Export the baseline sources without touching the work tree or tags.
    set(archive "${work_dir}/baseline.tar")
    set(baseline_src "${work_dir}/baseline-src")
    _run_abidiff_run(result output
        "${_run_abidiff_git}" -C "${source_dir}" archive --format=tar
        "--output=${archive}" "refs/tags/${baseline_tag}")
    if(NOT result EQUAL 0)
        _run_abidiff_fail("tool error"
            "git archive of ${baseline_tag} failed (exit status ${result})" "${output}")
    endif()
    file(MAKE_DIRECTORY "${baseline_src}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
        WORKING_DIRECTORY "${baseline_src}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    if(NOT result EQUAL 0)
        _run_abidiff_fail("tool error"
            "extracting ${archive} failed (exit status ${result})" "${output}")
    endif()

    # 5. Build and install both sides the same way.
    _run_abidiff_build(baseline "${baseline_src}" "${work_dir}" baseline_prefix)
    _run_abidiff_build(current "${source_dir}" "${work_dir}" current_prefix)

    # 6. The real shared library of each side.
    _run_abidiff_find_library(baseline "${baseline_prefix}" baseline_lib)
    _run_abidiff_find_library(current "${current_prefix}" current_lib)

    # 7. Compare with the public headers as the API scope.
    _run_abidiff_run(code report
        "${_run_abidiff_abidiff}"
        --headers-dir1 "${baseline_prefix}/include"
        --headers-dir2 "${current_prefix}/include"
        --drop-private-types
        "${baseline_lib}" "${current_lib}")

    # 8. Classify and always show the report.
    classify_exit_code("${code}" verdict)
    set(report_text
        "---- abidiff ${baseline_tag} (${baseline_lib}) -> current ${current_version} (${current_lib}): exit status ${code} ----\n${report}---- end of abidiff output ----")
    if(verdict STREQUAL "compatible")
        message("abi-compare: compatible: no incompatible ABI change from ${baseline_tag} to ${current_version} (abidiff exit status ${code})")
        message("${report_text}")
    elseif(verdict STREQUAL "incompatible")
        _run_abidiff_fail("incompatible"
            "incompatible ABI change from ${baseline_tag} to ${current_version} (abidiff exit status ${code})"
            "${report_text}")
    else()
        _run_abidiff_fail("tool error"
            "abidiff failed (exit status ${code})" "${report_text}")
    endif()
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
