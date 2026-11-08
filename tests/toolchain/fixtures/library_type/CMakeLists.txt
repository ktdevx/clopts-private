# Fixture for toolchain.library_type.<case> (EXPECT=configure_ok).
#
# Pulls CLOpts in with add_subdirectory after setting the variables of the
# case, and fails the configure step unless the form of clopts and its
# form-specific properties are as expected:
#
#   CASE               set before add_subdirectory                 TYPE
#   default            nothing                                     STATIC_LIBRARY
#   build_shared_libs  BUILD_SHARED_LIBS ON                        SHARED_LIBRARY
#   override           BUILD_SHARED_LIBS ON, CLOPTS_BUILD_SHARED   STATIC_LIBRARY
#                      OFF (normal variable)
#
# It also checks clopts_compute_soversion for the current version and for
# hypothetical 0.x / 1.x versions (the >= 1.0 rule cannot be reached with the
# current version).
#
# Expects -DCLOPTS_SOURCE_DIR=... and -DCASE=... The form of the parent build
# tree is deliberately NOT passed: a cached CLOPTS_BUILD_SHARED would change
# the meaning of the cases.

cmake_minimum_required(VERSION 3.21)
project(fixture_library_type C)

if("${CLOPTS_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "library_type: CLOPTS_SOURCE_DIR is not set")
endif()
if(DEFINED CACHE{CLOPTS_BUILD_SHARED} OR DEFINED CACHE{BUILD_SHARED_LIBS})
    message(FATAL_ERROR
        "library_type: CLOPTS_BUILD_SHARED / BUILD_SHARED_LIBS must not be passed "
        "on the command line; the case sets them")
endif()

if(CASE STREQUAL "default")
    set(expected_type STATIC_LIBRARY)
elseif(CASE STREQUAL "build_shared_libs")
    set(BUILD_SHARED_LIBS ON)
    set(expected_type SHARED_LIBRARY)
elseif(CASE STREQUAL "override")
    set(BUILD_SHARED_LIBS ON)
    set(CLOPTS_BUILD_SHARED OFF)
    set(expected_type STATIC_LIBRARY)
else()
    message(FATAL_ERROR
        "library_type: CASE must be default, build_shared_libs or override (got '${CASE}')")
endif()

add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts)

# fixture_expect(<what> <actual> <expected>)
function(fixture_expect what actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "library_type (${CASE}): ${what} is '${actual}'; expected '${expected}'")
    endif()
endfunction()

# fixture_get(<out> <property>): target property of clopts, "" when unset.
function(fixture_get out prop)
    get_target_property(value clopts ${prop})
    if(value MATCHES "-NOTFOUND$")
        set(value "")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# ---- Both forms ------------------------------------------------------------
if((NOT TARGET clopts) OR (NOT TARGET CLOpts::clopts))
    message(FATAL_ERROR "library_type (${CASE}): targets clopts / CLOpts::clopts are missing")
endif()
get_target_property(aliased CLOpts::clopts ALIASED_TARGET)
fixture_expect("CLOpts::clopts ALIASED_TARGET" "${aliased}" "clopts")

get_target_property(type clopts TYPE)
fixture_expect("clopts TYPE" "${type}" "${expected_type}")

# The version of CLOpts' own project definition, and the SOVERSION expected
# for it, derived here independently of clopts_compute_soversion.
get_directory_property(clopts_version DIRECTORY "${CLOPTS_SOURCE_DIR}" DEFINITION PROJECT_VERSION)
if(NOT clopts_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "library_type (${CASE}): unexpected CLOpts version '${clopts_version}'")
endif()
if(CMAKE_MATCH_1 EQUAL 0)
    set(expected_soversion "0.${CMAKE_MATCH_2}")
    set(expected_compat "SameMinorVersion")
else()
    set(expected_soversion "${CMAKE_MATCH_1}")
    set(expected_compat "SameMajorVersion")
endif()

# ---- Form-specific properties ------------------------------------------------
fixture_get(interface_defs INTERFACE_COMPILE_DEFINITIONS)
fixture_get(defs COMPILE_DEFINITIONS)
fixture_get(output_name OUTPUT_NAME)
fixture_get(version VERSION)
fixture_get(soversion SOVERSION)
fixture_get(define_symbol DEFINE_SYMBOL)
fixture_get(visibility C_VISIBILITY_PRESET)

if(type STREQUAL "STATIC_LIBRARY")
    # 12.4: position-independent, so that it can go into a consumer's shared library.
    get_target_property(pic clopts POSITION_INDEPENDENT_CODE)
    if(NOT pic)
        message(FATAL_ERROR
            "library_type (${CASE}): static clopts POSITION_INDEPENDENT_CODE is '${pic}'; expected ON")
    endif()
    # 12.5: CLOPTS_STATIC reaches the library itself and its consumers.
    fixture_expect("static clopts INTERFACE_COMPILE_DEFINITIONS" "${interface_defs}" "CLOPTS_STATIC")
    if(NOT "CLOPTS_STATIC" IN_LIST defs)
        message(FATAL_ERROR
            "library_type (${CASE}): static clopts COMPILE_DEFINITIONS is '${defs}'; "
            "expected it to contain CLOPTS_STATIC")
    endif()
    # 12.6: with MSVC the static library must not collide with the import library.
    if(MSVC)
        fixture_expect("static clopts OUTPUT_NAME (MSVC)" "${output_name}" "clopts_static")
    else()
        fixture_expect("static clopts OUTPUT_NAME" "${output_name}" "")
    endif()
else()
    # 15.1, 15.2: file version and SOVERSION.
    fixture_expect("shared clopts VERSION" "${version}" "${clopts_version}")
    fixture_expect("shared clopts SOVERSION" "${soversion}" "${expected_soversion}")
    # Exports are switched on only while building the library itself.
    fixture_expect("shared clopts DEFINE_SYMBOL" "${define_symbol}" "CLOPTS_BUILDING")
    fixture_expect("shared clopts INTERFACE_COMPILE_DEFINITIONS" "${interface_defs}" "")
    if(NOT WIN32)
        fixture_expect("shared clopts C_VISIBILITY_PRESET" "${visibility}" "hidden")
    endif()
    fixture_expect("shared clopts OUTPUT_NAME" "${output_name}" "")
endif()

# ---- SOVERSION rule (15.2 - 15.4) ------------------------------------------------
if(NOT COMMAND clopts_compute_soversion)
    message(FATAL_ERROR "library_type (${CASE}): clopts_compute_soversion is not defined")
endif()

# The results CLOpts computed for itself (normal variables in its directory).
get_directory_property(clopts_soversion DIRECTORY "${CLOPTS_SOURCE_DIR}" DEFINITION CLOPTS_SOVERSION)
get_directory_property(clopts_compat DIRECTORY "${CLOPTS_SOURCE_DIR}" DEFINITION CLOPTS_VERSION_COMPATIBILITY)
fixture_expect("CLOPTS_SOVERSION" "${clopts_soversion}" "${expected_soversion}")
fixture_expect("CLOPTS_VERSION_COMPATIBILITY" "${clopts_compat}" "${expected_compat}")

# <version> <expected SOVERSION> <expected compatibility>
set(soversion_cases
    "${clopts_version}" "${expected_soversion}" "${expected_compat}"
    "0.1.0" "0.1" "SameMinorVersion"
    "0.12.3" "0.12" "SameMinorVersion"
    "1.2.3" "1" "SameMajorVersion"
    "2.0.0" "2" "SameMajorVersion"
)
list(LENGTH soversion_cases count)
math(EXPR last "${count} - 1")
foreach(i RANGE 0 ${last} 3)
    math(EXPR j "${i} + 1")
    math(EXPR k "${i} + 2")
    list(GET soversion_cases ${i} case_version)
    list(GET soversion_cases ${j} case_soversion)
    list(GET soversion_cases ${k} case_compat)
    clopts_compute_soversion("${case_version}" got_soversion got_compat)
    fixture_expect("clopts_compute_soversion(${case_version}) SOVERSION"
        "${got_soversion}" "${case_soversion}")
    fixture_expect("clopts_compute_soversion(${case_version}) compatibility"
        "${got_compat}" "${case_compat}")
endforeach()

message(STATUS "library_type (${CASE}): ${type}, all checks passed")
