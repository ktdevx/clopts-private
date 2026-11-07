/*
 * FILE* writer. This is the only translation unit in src/core/ that
 * includes <stdio.h>.
 */

#include "core/core_internal.h"

#include <assert.h>
#include <stdio.h>

static int core_writer_file_write(void *ctx, const char *data, size_t len)
{
    FILE *stream = (FILE *)ctx;

    assert(stream != NULL);

    if (len == 0)
    {
        return 0;
    }

    assert(data != NULL);

    if (fwrite(data, 1, len, stream) < len)
    {
        return 1;
    }

    return 0;
}

clopts_writer_t clopts_writer_file(FILE *stream)
{
    clopts_writer_t writer;

    assert(stream != NULL);

    writer.write = core_writer_file_write;
    writer.ctx = stream;

    return writer;
}
