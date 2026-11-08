/*
 * C++ translation unit of the consumer fixture: the public umbrella header
 * must be usable from C++, and the public declarations must have C linkage.
 *
 * consumer_cpp_check() is called from main.c. It stores the ready-made
 * callback in a clopts_write_fn variable and writes one line through it.
 * clopts_file_write is static inline and never reaches the linker, so the
 * address of the exported clopts_parse is also stored in a volatile pointer:
 * that reference must resolve against the library's C symbol (through the
 * DLL import on Windows shared builds). The unused parameter guards against
 * CLOpts' warning flags leaking into the consumer, as in main.c.
 */
#include <clopts/clopts.h>

#include <cstdio>
#include <cstring>

#ifdef _CRT_SECURE_NO_WARNINGS
#error "_CRT_SECURE_NO_WARNINGS leaked from CLOpts into a consumer"
#endif

extern "C" int consumer_cpp_check(int unused);

extern "C" int consumer_cpp_check(int unused)
{
    static const char line[] = "clopts consumer (C++): ok\n";
    const clopts_write_fn fn = clopts_file_write;
    decltype(&clopts_parse) volatile parse_fn = &clopts_parse;

    if (parse_fn == nullptr)
    {
        return 1;
    }
    return fn(stdout, line, std::strlen(line));
}
