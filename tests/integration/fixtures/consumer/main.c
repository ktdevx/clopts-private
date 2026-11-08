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
 * public functions only.
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
    clopts_option_t *name;
    clopts_result_t *result;
    const char *value;
    int failed = 0;

    program = clopts_program_new("consumer");
    if (program == NULL)
    {
        return 1;
    }
    name = clopts_command_add_value(clopts_program_root(program), "name", '\0');
    if (name == NULL)
    {
        clopts_program_free(program);
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
    return fflush(stdout) == 0 ? 0 : 1;
}
