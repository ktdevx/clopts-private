/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * TextOut: output sink in write or measure mode.
 *
 * Write mode passes the bytes to the callback and stops calling it after the
 * first failure. Measure mode writes nothing and
 * adds the display width of the bytes instead, so the same sequence of calls
 * can first measure a row and then write it.
 */

#include "text/text_internal.h"

#include "base/utf8.h"

#include <assert.h>
#include <string.h>

/* Spaces written per callback call by clopts_text_out_spaces. */
#define TEXT_OUT_SPACES_CHUNK 32

static const char text_out_spaces_buf[TEXT_OUT_SPACES_CHUNK + 1] =
    "                                ";

void clopts_text_out_init(clopts_text_out_t *out, clopts_write_fn fn, void *ctx)
{
    assert(out != NULL);

    out->fn = fn;
    out->ctx = ctx;
    out->failed = false;
    out->width = 0;
}

void clopts_text_out_bytes(clopts_text_out_t *out, const char *data, size_t len)
{
    assert(out != NULL);
    assert((len == 0) || (data != NULL));

    if (len == 0)
    {
        return;
    }
    if (out->fn == NULL)
    {
        out->width += clopts_utf8_count(data, len);
        return;
    }
    if (out->failed)
    {
        return;
    }
    if (out->fn(out->ctx, data, len) != 0)
    {
        out->failed = true;
    }
}

void clopts_text_out_str(clopts_text_out_t *out, const char *s)
{
    assert(s != NULL);

    clopts_text_out_bytes(out, s, strlen(s));
}

void clopts_text_out_char(clopts_text_out_t *out, char c)
{
    clopts_text_out_bytes(out, &c, 1);
}

void clopts_text_out_spaces(clopts_text_out_t *out, size_t count)
{
    while (count > 0)
    {
        size_t chunk = (count < TEXT_OUT_SPACES_CHUNK) ? count : TEXT_OUT_SPACES_CHUNK;

        clopts_text_out_bytes(out, text_out_spaces_buf, chunk);
        count -= chunk;
    }
}

clopts_write_status_t clopts_text_out_status(const clopts_text_out_t *out)
{
    assert(out != NULL);

    return out->failed ? CLOPTS_WRITE_FAILED : CLOPTS_WRITE_OK;
}
