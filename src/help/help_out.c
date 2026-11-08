/*
 * HelpOut: output sink in write or measure mode, and the UTF-8 display width.
 *
 * Skeleton: the functions below do nothing yet.
 */

#include "help/help_internal.h"

void clopts_help_out_init(clopts_help_out_t *out, clopts_write_fn fn, void *ctx)
{
    (void)out;
    (void)fn;
    (void)ctx;
}

void clopts_help_out_bytes(clopts_help_out_t *out, const char *data, size_t len)
{
    (void)out;
    (void)data;
    (void)len;
}

void clopts_help_out_str(clopts_help_out_t *out, const char *s)
{
    (void)out;
    (void)s;
}

void clopts_help_out_char(clopts_help_out_t *out, char c)
{
    (void)out;
    (void)c;
}

void clopts_help_out_spaces(clopts_help_out_t *out, size_t count)
{
    (void)out;
    (void)count;
}

clopts_write_status_t clopts_help_out_status(const clopts_help_out_t *out)
{
    (void)out;
    return CLOPTS_WRITE_OK;
}

size_t clopts_help_width(const char *data, size_t len)
{
    (void)data;
    (void)len;
    return 0;
}
