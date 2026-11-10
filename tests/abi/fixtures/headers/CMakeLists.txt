# Fixture for abi.headers_c / abi.headers_cxx (EXPECT=build_ok).
#
# Compiles every public header of CLOpts on its own, as C99 or as C++11, with
# warnings as errors, and (C++ only) links a program that takes the address of
# every function in the public symbol list.
#
# At configure time one translation unit is generated per header
# ${CLOPTS_SOURCE_DIR}/include/clopts/<header>.h, named check_<header>.c or
# check_<header>.cpp so that a failing header and language can be read from
# the build output. Each one includes only that header, plus one typedef so
# that the translation unit is never empty (ISO C forbids an empty one).
#
#   HEADER_CHECK_LANG=C    C-only project. The generated .c files form an
#                          object library with CLOpts' own compiler options
#                          (clopts_apply_compiler_options: C99 without
#                          extensions, CLOpts' warnings, -Werror / /WX with
#                          CLOPTS_WERROR=ON).
#   HEADER_CHECK_LANG=CXX  C and C++ project. The generated .cpp files form an
#                          object library compiled as C++11 without
#                          extensions, with the C++ counterpart of CLOpts'
#                          warnings as errors (GCC / Clang / AppleClang:
#                          -Wall -Wextra -Wpedantic -Werror; MSVC: /W4 /WX
#                          /utf-8 /Zc:preprocessor /Zc:__cplusplus /EHsc).
#                          In addition, link_check.cpp is generated from
#                          SYMBOLS_FILE and linked against CLOpts::clopts as
#                          the executable abi_link_check: a function declared
#                          without C linkage fails to link (its reference is
#                          mangled), and a listed name that no header declares
#                          fails to compile. Names tagged "windows" are wrapped
#                          in #ifdef _WIN32. The program is never run.
#
# Inputs (-D): CLOPTS_SOURCE_DIR (passed by tests/cmake/run_fixture.cmake),
# HEADER_CHECK_LANG, SYMBOLS_FILE, CLOPTS_BUILD_SHARED, CLOPTS_WERROR=ON and,
# for CXX, CMAKE_CXX_COMPILER.
#
# The headers are compiled with CLOpts' usage requirements (include directory,
# CLOPTS_STATIC for the static form), exactly as a consumer sees them. CLOpts
# is added EXCLUDE_FROM_ALL: in C mode nothing of it is built, in C++ mode only
# the library abi_link_check needs.

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HEADER_CHECK_LANG OR NOT HEADER_CHECK_LANG MATCHES "^(C|CXX)$")
    message(FATAL_ERROR "headers fixture: HEADER_CHECK_LANG must be C or CXX (got '${HEADER_CHECK_LANG}')")
endif()

if(HEADER_CHECK_LANG STREQUAL "C")
    project(fixture_headers_c LANGUAGES C)
    set(fixture_ext "c")
else()
    project(fixture_headers_cxx LANGUAGES C CXX)
    set(fixture_ext "cpp")
endif()

foreach(fixture_var IN ITEMS CLOPTS_SOURCE_DIR SYMBOLS_FILE)
    if("${${fixture_var}}" STREQUAL "")
        message(FATAL_ERROR "headers fixture: ${fixture_var} is not set")
    endif()
endforeach()
if(NOT DEFINED CLOPTS_BUILD_SHARED)
    message(FATAL_ERROR "headers fixture: CLOPTS_BUILD_SHARED (form under test) is not set")
endif()
if(NOT CLOPTS_WERROR)
    message(FATAL_ERROR "headers fixture: run with -DCLOPTS_WERROR=ON")
endif()

add_subdirectory("${CLOPTS_SOURCE_DIR}" clopts EXCLUDE_FROM_ALL)
include("${CLOPTS_SOURCE_DIR}/cmake/CloptsCompilerOptions.cmake")

# ---- One translation unit per public header ----------------------------------
file(GLOB_RECURSE fixture_headers LIST_DIRECTORIES false "${CLOPTS_SOURCE_DIR}/include/clopts/*.h")
list(LENGTH fixture_headers fixture_header_count)
if(fixture_header_count EQUAL 0)
    message(FATAL_ERROR "headers fixture: no public headers in ${CLOPTS_SOURCE_DIR}/include/clopts")
endif()

# Order: by the number of clopts/ headers a header includes, then by name. The
# build compiles the translation units in this order (and stops at the first
# error with most generators), so a broken header is reported from its own
# check_<header> before a header that includes it (e.g. clopts.h).
set(fixture_keyed "")
foreach(fixture_header IN LISTS fixture_headers)
    file(STRINGS "${fixture_header}" fixture_includes REGEX "^[ \t]*#[ \t]*include[ \t]*\"clopts/")
    list(LENGTH fixture_includes fixture_include_count)
    string(LENGTH "${fixture_include_count}" fixture_digits)
    while(fixture_digits LESS 4)
        string(PREPEND fixture_include_count "0")
        math(EXPR fixture_digits "${fixture_digits} + 1")
    endwhile()
    list(APPEND fixture_keyed "${fixture_include_count}|${fixture_header}")
endforeach()
list(SORT fixture_keyed)
list(TRANSFORM fixture_keyed REPLACE "^[0-9]+\\|" "" OUTPUT_VARIABLE fixture_headers)

set(fixture_sources "")
foreach(fixture_header IN LISTS fixture_headers)
    # fixture_name: the path below include/clopts/ without ".h" (base/allocator).
    file(RELATIVE_PATH fixture_name "${CLOPTS_SOURCE_DIR}/include/clopts" "${fixture_header}")
    string(REGEX REPLACE "\\.h$" "" fixture_name "${fixture_name}")
    string(MAKE_C_IDENTIFIER "${fixture_name}" fixture_id)
    set(fixture_source "${CMAKE_CURRENT_BINARY_DIR}/check_${fixture_id}.${fixture_ext}")
    file(CONFIGURE OUTPUT "${fixture_source}" CONTENT
"/* Generated by tests/abi/fixtures/headers/CMakeLists.txt: compiles
 * clopts/${fixture_name}.h on its own. */
#include \"clopts/${fixture_name}.h\"

/* Keeps the translation unit non-empty. */
typedef int clopts_header_check_${fixture_id};
" @ONLY)
    list(APPEND fixture_sources "${fixture_source}")
endforeach()

add_library(abi_header_check OBJECT ${fixture_sources})
# Only CLOpts' usage requirements, not a link: target_link_libraries would make
# the library a build dependency, its sources would be compiled first, and a
# broken header would be reported from a CLOpts source instead of from
# check_<header>.
target_include_directories(abi_header_check PRIVATE
    "$<TARGET_PROPERTY:clopts,INTERFACE_INCLUDE_DIRECTORIES>")
target_compile_definitions(abi_header_check PRIVATE
    "$<TARGET_PROPERTY:clopts,INTERFACE_COMPILE_DEFINITIONS>")

if(HEADER_CHECK_LANG STREQUAL "C")
    clopts_apply_compiler_options(abi_header_check)
    message(STATUS "headers fixture: ${fixture_header_count} headers as C99")
    return()
endif()

# ---- C++ -------------------------------------------------------------------------
# fixture_apply_cxx_options(<target>)
#
# C++11 without extensions and the C++ counterpart of CLOpts' warnings, always
# as errors.
function(fixture_apply_cxx_options target)
    set_target_properties("${target}" PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options("${target}" PRIVATE
            /W4 /WX /utf-8 /Zc:preprocessor /Zc:__cplusplus /EHsc)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

fixture_apply_cxx_options(abi_header_check)

# The public symbol list (format: abi/clopts.symbols). Blank and "#" lines are
# skipped; a name may carry the platform tag "windows". The list itself is
# checked by abi.symbol_list; here a malformed line only stops the configure.
file(READ "${SYMBOLS_FILE}" fixture_content)
string(REPLACE "\r" "" fixture_content "${fixture_content}")
# Characters with a meaning in CMake lists become "?" (never a valid name).
string(REGEX REPLACE "[][;\\\\]" "?" fixture_content "${fixture_content}")
string(REPLACE "\n" ";" fixture_lines "${fixture_content}")

set(fixture_entries "")
set(fixture_untagged 0)
set(fixture_line_number 0)
foreach(fixture_line IN LISTS fixture_lines)
    math(EXPR fixture_line_number "${fixture_line_number} + 1")
    string(STRIP "${fixture_line}" fixture_line)
    if(fixture_line STREQUAL "" OR fixture_line MATCHES "^#")
        continue()
    endif()
    string(REGEX MATCHALL "[^ \t]+" fixture_words "${fixture_line}")
    list(LENGTH fixture_words fixture_word_count)
    list(GET fixture_words 0 fixture_symbol)
    set(fixture_where "${SYMBOLS_FILE}:${fixture_line_number}")
    if(NOT fixture_symbol MATCHES "^clopts_[a-z0-9_]+$")
        message(FATAL_ERROR "headers fixture: ${fixture_where}: invalid symbol name '${fixture_symbol}'")
    endif()
    set(fixture_entry
        "    reinterpret_cast<void (*)(void)>(&${fixture_symbol}),\n")
    if(fixture_word_count EQUAL 1)
        math(EXPR fixture_untagged "${fixture_untagged} + 1")
    else()
        list(GET fixture_words 1 fixture_tag)
        if(NOT fixture_word_count EQUAL 2 OR NOT fixture_tag STREQUAL "windows")
            message(FATAL_ERROR "headers fixture: ${fixture_where}: invalid platform tag in '${fixture_line}'")
        endif()
        set(fixture_entry "#ifdef _WIN32\n${fixture_entry}#endif\n")
    endif()
    string(APPEND fixture_entries "${fixture_entry}")
endforeach()

# An array may not be empty in C++, and the untagged names are the ones present
# on every OS.
if(fixture_untagged EQUAL 0)
    message(FATAL_ERROR "headers fixture: ${SYMBOLS_FILE} lists no symbol without a platform tag")
endif()

# The array has external linkage so that it, and every reference in it, is
# emitted even when optimizing.
set(fixture_link_check "${CMAKE_CURRENT_BINARY_DIR}/link_check.cpp")
file(CONFIGURE OUTPUT "${fixture_link_check}" CONTENT
"/* Generated by tests/abi/fixtures/headers/CMakeLists.txt from
 * @SYMBOLS_FILE@: references every public function, so that a function
 * without C linkage fails to link and a listed name that no header declares
 * fails to compile. Never run. */
#include \"clopts/clopts.h\"

extern void (*const abi_link_check_symbols[])(void);
void (*const abi_link_check_symbols[])(void) = {
@fixture_entries@};

int main()
{
    return static_cast<int>(sizeof(abi_link_check_symbols) / sizeof(abi_link_check_symbols[0]));
}
" @ONLY)

add_executable(abi_link_check "${fixture_link_check}")
target_link_libraries(abi_link_check PRIVATE CLOpts::clopts)
fixture_apply_cxx_options(abi_link_check)
# The per-header translation units are compiled before CLOpts' own sources
# (with any generator), so that a broken header is reported from
# check_<header>.cpp first.
add_dependencies(clopts abi_header_check)

message(STATUS "headers fixture: ${fixture_header_count} headers as C++11, link check of ${SYMBOLS_FILE}")
