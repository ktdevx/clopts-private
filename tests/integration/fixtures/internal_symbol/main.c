/*
 * References an internal CLOpts function that is not marked with CLOPTS_API.
 * The declaration is written here on purpose, as a consumer depending on an
 * internal function would do; the internal header is not available to
 * consumers. Linking against the shared library must fail with an
 * unresolved reference to clopts_core_size_mul.
 */
#include <stddef.h>

int clopts_core_size_mul(size_t a, size_t b, size_t *out);

int main(void)
{
    size_t bytes = 0;

    if (clopts_core_size_mul(3, 4, &bytes) != 0)
    {
        return 1;
    }
    return bytes == 12 ? 0 : 1;
}
