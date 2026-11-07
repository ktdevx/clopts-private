/*
 * Consumer of CLOpts for the integration tests. Uses only the public umbrella
 * header and CLOpts::clopts.
 *
 * Detects leaked build settings of CLOpts' own targets:
 *   - _CRT_SECURE_NO_WARNINGS is a PRIVATE definition of CLOpts (MSVC).
 *   - main's parameters are unused on purpose: with CLOpts' -Wall -Wextra -Werror
 *     (or /W4 /WX) leaking into this target, the build would fail.
 */
#include <clopts/clopts.h>

#include <stdio.h>
#include <string.h>

#ifdef _CRT_SECURE_NO_WARNINGS
#error "_CRT_SECURE_NO_WARNINGS leaked from CLOpts into a consumer"
#endif

int main(int argc, char **argv)
{
    static const char line[] = "clopts consumer: ok\n";
    clopts_writer_t writer = clopts_writer_file(stdout);

    if (writer.write(writer.ctx, line, strlen(line)) != 0)
    {
        return 1;
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
