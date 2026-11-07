#include "core/core_internal.h"

#include <assert.h>
#include <string.h>

int clopts_core_write(const clopts_writer_t *writer, const char *data, size_t len)
{
    assert(writer != NULL);
    assert(writer->write != NULL);
    assert((len == 0) || (data != NULL));

    return writer->write(writer->ctx, data, len);
}

int clopts_core_write_str(const clopts_writer_t *writer, const char *s)
{
    assert(s != NULL);

    return clopts_core_write(writer, s, strlen(s));
}
