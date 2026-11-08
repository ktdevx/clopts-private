/*
 * HelpOut: output sink in write or measure mode, and the UTF-8 display width.
 *
 * Write mode passes the bytes to the callback through clopts_core_write and
 * stops calling it after the first failure. Measure mode writes nothing and
 * adds the display width of the bytes instead, so the same sequence of calls
 * can first measure a row and then write it.
 */

#include "help/help_internal.h"

#include <assert.h>
#include <string.h>

/* Spaces written per callback call by clopts_help_out_spaces. */
#define HELP_OUT_SPACES_CHUNK 32

static const char help_out_spaces_buf[HELP_OUT_SPACES_CHUNK + 1] =
    "                                ";

void clopts_help_out_init(clopts_help_out_t *out, clopts_write_fn fn, void *ctx)
{
    assert(out != NULL);

    out->fn = fn;
    out->ctx = ctx;
    out->failed = false;
    out->width = 0;
}

void clopts_help_out_bytes(clopts_help_out_t *out, const char *data, size_t len)
{
    assert(out != NULL);
    assert((len == 0) || (data != NULL));

    if (len == 0)
    {
        return;
    }
    if (out->fn == NULL)
    {
        out->width += clopts_help_width(data, len);
        return;
    }
    if (out->failed)
    {
        return;
    }
    if (clopts_core_write(out->fn, out->ctx, data, len) != 0)
    {
        out->failed = true;
    }
}

void clopts_help_out_str(clopts_help_out_t *out, const char *s)
{
    assert(s != NULL);

    clopts_help_out_bytes(out, s, strlen(s));
}

void clopts_help_out_char(clopts_help_out_t *out, char c)
{
    clopts_help_out_bytes(out, &c, 1);
}

void clopts_help_out_spaces(clopts_help_out_t *out, size_t count)
{
    while (count > 0)
    {
        size_t chunk = (count < HELP_OUT_SPACES_CHUNK) ? count : HELP_OUT_SPACES_CHUNK;

        clopts_help_out_bytes(out, help_out_spaces_buf, chunk);
        count -= chunk;
    }
}

clopts_write_status_t clopts_help_out_status(const clopts_help_out_t *out)
{
    assert(out != NULL);

    return out->failed ? CLOPTS_WRITE_FAILED : CLOPTS_WRITE_OK;
}

/*
 * Length of the valid UTF-8 sequence starting at p (at most avail bytes), or
 * 0 when no valid sequence starts there. The ranges of the second byte rule
 * out overlong forms (E0, F0), surrogates (ED) and code points above U+10FFFF
 * (F4); C0, C1 and F5..FF never start a sequence.
 */
static size_t help_utf8_sequence(const unsigned char *p, size_t avail)
{
    unsigned char lead = p[0];
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;
    size_t n;
    size_t i;

    if (lead < 0x80)
    {
        return 1;
    }
    if ((lead >= 0xC2) && (lead <= 0xDF))
    {
        n = 2;
    }
    else if ((lead >= 0xE0) && (lead <= 0xEF))
    {
        n = 3;
        if (lead == 0xE0)
        {
            lo = 0xA0;
        }
        else if (lead == 0xED)
        {
            hi = 0x9F;
        }
    }
    else if ((lead >= 0xF0) && (lead <= 0xF4))
    {
        n = 4;
        if (lead == 0xF0)
        {
            lo = 0x90;
        }
        else if (lead == 0xF4)
        {
            hi = 0x8F;
        }
    }
    else
    {
        return 0;
    }

    if (avail < n)
    {
        return 0;
    }
    if ((p[1] < lo) || (p[1] > hi))
    {
        return 0;
    }
    for (i = 2; i < n; i++)
    {
        if ((p[i] < 0x80) || (p[i] > 0xBF))
        {
            return 0;
        }
    }
    return n;
}

size_t clopts_help_width(const char *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t width = 0;
    size_t i = 0;

    assert((len == 0) || (data != NULL));

    while (i < len)
    {
        size_t n = help_utf8_sequence(p + i, len - i);

        /* an invalid byte is one column on its own; resume at the next byte */
        i += (n != 0) ? n : 1;
        width++;
    }
    return width;
}
