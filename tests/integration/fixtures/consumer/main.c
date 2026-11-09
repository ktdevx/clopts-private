/*
 * Consumer of CLOpts for the integration tests. Uses only the public umbrella
 * header and CLOpts::clopts. Also calls consumer_cpp_check() from
 * consumer_cpp.cpp, which uses the same header from C++.
 *
 * Detects leaked build settings of CLOpts' own targets:
 *   - _CRT_SECURE_NO_WARNINGS is a PRIVATE definition of CLOpts (MSVC).
 *   - main's parameters are unused on purpose: with CLOpts' -Wall -Wextra -Werror
 *     (or /W4 /WX) leaking into this target, the build would fail.
 *
 * consumer_parse_check() runs declaration -> parse -> lookup -> release with
 * public functions only. consumer_help_check() calls the five text writers
 * of clopts/help.h and clopts_decl_error_message() of clopts/decl_types.h
 * through the umbrella header (in a shared build, each call
 * must resolve against an exported symbol). consumer_platform_check() calls
 * every function of clopts/platform.h (clopts_argv_from_wide only on Windows)
 * through the umbrella header and checks the results against the contract.
 */
#include <clopts/clopts.h>

#include <stdio.h>
#include <string.h>

#ifdef _CRT_SECURE_NO_WARNINGS
#error "_CRT_SECURE_NO_WARNINGS leaked from CLOpts into a consumer"
#endif

/* Defined in consumer_cpp.cpp with C linkage. */
int consumer_cpp_check(int unused);

/*
 * Declares VALUE --name on the program "consumer", parses the fixed argv
 * {"consumer", "--name", "ok"} and checks the status and the value.
 * Returns 0 on success, non-zero on any mismatch.
 */
static int consumer_parse_check(void)
{
    char arg0[] = "consumer";
    char arg1[] = "--name";
    char arg2[] = "ok";
    char *const args[] = { arg0, arg1, arg2 };
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_option_t *name;
    clopts_result_t *result;
    const char *value;
    int failed = 0;

    if (clopts_command_new("consumer", &root) != CLOPTS_DECL_NONE)
    {
        return 1;
    }
    if (clopts_option_create(CLOPTS_OPTION_VALUE, "name", '\0', &name) != CLOPTS_DECL_NONE)
    {
        clopts_command_free(root);
        return 1;
    }
    if (clopts_command_add_option(root, name) != CLOPTS_DECL_NONE)
    {
        clopts_option_destroy(name);
        clopts_command_free(root);
        return 1;
    }
    if (clopts_program_new(root, NULL, 0, &program) != CLOPTS_DECL_NONE)
    {
        clopts_command_free(root);
        return 1;
    }
    result = clopts_parse(program, 3, args);
    if (clopts_result_status(result) != CLOPTS_OK)
    {
        failed = 1;
    }
    else
    {
        value = clopts_result_value(result, name);
        if ((value == NULL) || (strcmp(value, "ok") != 0))
        {
            failed = 1;
        }
    }
    clopts_result_free(result);
    clopts_program_free(program);
    return failed;
}

/* Write callback that discards the bytes; ctx is unused. */
static int consumer_discard(void *ctx, const char *data, size_t len)
{
    (void)ctx;
    (void)data;
    (void)len;
    return 0;
}

/* Returns 1 when status is one of the three values of clopts_write_status_t. */
static int consumer_write_status_valid(clopts_write_status_t status)
{
    return (status == CLOPTS_WRITE_OK) || (status == CLOPTS_WRITE_FAILED) || (status == CLOPTS_WRITE_UNAVAILABLE);
}

/*
 * Calls each of the five text writers once on the program "consumer" and
 * checks that it returns a clopts_write_status_t value. The text itself is
 * not checked here. Returns 0 on success, non-zero on any mismatch.
 */
static int consumer_help_check(void)
{
    clopts_program_t *program;
    clopts_command_t *root;
    clopts_result_t *result;
    int failed = 0;

    if (clopts_command_new("consumer", &root) != CLOPTS_DECL_NONE)
    {
        return 1;
    }
    if (clopts_program_new(root, NULL, 0, &program) != CLOPTS_DECL_NONE)
    {
        clopts_command_free(root);
        return 1;
    }
    result = clopts_parse(program, 0, NULL);
    if (!consumer_write_status_valid(clopts_write_help(program, NULL, consumer_discard, NULL)) ||
        !consumer_write_status_valid(clopts_write_usage(program, NULL, consumer_discard, NULL)) ||
        !consumer_write_status_valid(clopts_write_version(program, "1.0", consumer_discard, NULL)) ||
        !consumer_write_status_valid(clopts_write_usage_hint(program, NULL, consumer_discard, NULL)) ||
        !consumer_write_status_valid(clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR,
                                                        consumer_discard, NULL)) ||
        (clopts_decl_error_message(CLOPTS_DECL_DUPLICATE_OPTION) == NULL))
    {
        failed = 1;
    }
    clopts_result_free(result);
    clopts_program_free(program);
    return failed;
}

/*
 * argv: clopts_argv_get on the fixed argv {"consumer", NULL} (main's own
 * parameters stay unused, see above). On Windows the handle holds the split
 * process command line instead, so only the contract is checked there: OK,
 * at least one element, values[count] == NULL, error index -1. Elsewhere the
 * very same argv comes back. Then clopts_argv_free on the handle and on NULL.
 * Windows only: clopts_argv_from_wide on {L"consumer", NULL} gives "consumer".
 * env: a new context looks up PATH (FOUND with a value, or UNSET; never
 * FAILED), after which the error is NONE and the error name NULL; a NULL
 * context gives FAILED. Returns 0 on success, non-zero on any mismatch.
 */
static int consumer_platform_check(void)
{
    char arg0[] = "consumer";
    char *const args[] = { arg0, NULL };
    clopts_argv_t *argv_handle;
    char *const *values;
    int count;
    clopts_env_t *env;
    clopts_env_status_t env_status;
    const char *value;
    int failed = 0;

    argv_handle = clopts_argv_get(1, args);
    count = clopts_argv_count(argv_handle);
    values = clopts_argv_values(argv_handle);
    if ((clopts_argv_status(argv_handle) != CLOPTS_ARGV_OK) || (count < 1) || (values == NULL) ||
        (values[count] != NULL) || (clopts_argv_error_index(argv_handle) != -1))
    {
        failed = 1;
    }
#ifndef _WIN32
    else if ((count != 1) || (values[0] != arg0))
    {
        failed = 1;
    }
#endif
    clopts_argv_free(argv_handle);
    clopts_argv_free(NULL);

#ifdef _WIN32
    {
        wchar_t warg0[] = L"consumer";
        wchar_t *const wargs[] = { warg0, NULL };

        argv_handle = clopts_argv_from_wide(1, wargs);
        values = clopts_argv_values(argv_handle);
        if ((clopts_argv_status(argv_handle) != CLOPTS_ARGV_OK) || (clopts_argv_count(argv_handle) != 1) ||
            (strcmp(values[0], "consumer") != 0) || (values[1] != NULL) ||
            (clopts_argv_error_index(argv_handle) != -1))
        {
            failed = 1;
        }
        clopts_argv_free(argv_handle);
    }
#endif

    env = clopts_env_new();
    if (env == NULL)
    {
        return 1;
    }
    value = NULL;
    env_status = clopts_env_lookup(env, "PATH", &value);
    if (((env_status != CLOPTS_ENV_FOUND) && (env_status != CLOPTS_ENV_UNSET)) ||
        ((env_status == CLOPTS_ENV_FOUND) && (value == NULL)) || (clopts_env_error(env) != CLOPTS_ENV_ERROR_NONE) ||
        (clopts_env_error_name(env) != NULL))
    {
        failed = 1;
    }
    if (clopts_env_lookup(NULL, "PATH", &value) != CLOPTS_ENV_FAILED)
    {
        failed = 1;
    }
    clopts_env_free(env);
    return failed;
}

int main(int argc, char **argv)
{
    static const char line[] = "clopts consumer: ok\n";

    if (clopts_file_write(stdout, line, strlen(line)) != 0)
    {
        return 1;
    }
    if (consumer_cpp_check(0) != 0)
    {
        return 1;
    }
    if (consumer_parse_check() != 0)
    {
        return 1;
    }
    if (consumer_help_check() != 0)
    {
        return 1;
    }
    if (consumer_platform_check() != 0)
    {
        return 1;
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
