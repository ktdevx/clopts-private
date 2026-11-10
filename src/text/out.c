/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Output: write mode and measure mode.
 *
 * Write mode passes the bytes to the callback and stops calling it after the
 * first failure. Measure mode writes nothing and
 * adds the display width of the bytes instead, so the same sequence of calls
 * can first measure a row and then write it.
 */

#include "text/out.h"

#include "base/utf8.h"

#include <assert.h>
#include <string.h>

/* Spaces written per callback call by clopts_out_spaces. */
#define OUT_SPACES_CHUNK 32

static const char spaces[OUT_SPACES_CHUNK + 1] =
    "                                ";

void clopts_out_init(clopts_out_t *out, clopts_write_fn fn, void *ctx)
{
    assert(out != NULL);

    out->fn = fn;
    out->ctx = ctx;
    out->failed = false;
    out->width = 0;
}

void clopts_out_bytes(clopts_out_t *out, const char *data, size_t len)
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

void clopts_out_str(clopts_out_t *out, const char *s)
{
    assert(s != NULL);

    clopts_out_bytes(out, s, strlen(s));
}

void clopts_out_char(clopts_out_t *out, char c)
{
    clopts_out_bytes(out, &c, 1);
}

void clopts_out_spaces(clopts_out_t *out, size_t count)
{
    while (count > 0)
    {
        size_t chunk = (count < OUT_SPACES_CHUNK) ? count : OUT_SPACES_CHUNK;

        clopts_out_bytes(out, spaces, chunk);
        count -= chunk;
    }
}

clopts_write_status_t clopts_out_status(const clopts_out_t *out)
{
    assert(out != NULL);

    return out->failed ? CLOPTS_WRITE_FAILED : CLOPTS_WRITE_OK;
}
