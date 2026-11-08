/*
 * Allocation gateway and allocation probe.
 *
 * This is the only file of the parser that reads the buffer of a
 * clopts_core_vec_t (data / cap, to tell when a vec gets or loses its
 * buffer). Other files read only its element count (len).
 */

#include "parser/parser_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 * Passes one fault point. Returns true when the allocation may go ahead and
 * false when the probe makes it fail. Always true without a probe.
 */
static bool parser_alloc_pass(clopts_parser_probe_t *probe)
{
    if (probe == NULL)
    {
        return true;
    }

    probe->calls++;
    if ((probe->fail_at != 0) && (probe->calls >= probe->fail_at))
    {
        return false;
    }
    return true;
}

/* Counts ptr as a live block when it is not NULL. Returns ptr. */
static void *parser_alloc_track(clopts_parser_probe_t *probe, void *ptr)
{
    if ((probe != NULL) && (ptr != NULL))
    {
        probe->live++;
    }
    return ptr;
}

void *clopts_parser_malloc(clopts_parser_probe_t *probe, size_t size)
{
    if (!parser_alloc_pass(probe))
    {
        return NULL;
    }
    return parser_alloc_track(probe, malloc(size));
}

void *clopts_parser_calloc(clopts_parser_probe_t *probe, size_t count, size_t size)
{
    size_t bytes;

    if (!parser_alloc_pass(probe))
    {
        return NULL;
    }
    if (clopts_core_size_mul(count, size, &bytes) != 0)
    {
        return NULL;
    }
    return parser_alloc_track(probe, calloc(count, size));
}

char *clopts_parser_strdup(clopts_parser_probe_t *probe, const char *s)
{
    if (s == NULL)
    {
        return NULL;
    }
    return clopts_parser_strndup(probe, s, strlen(s));
}

char *clopts_parser_strndup(clopts_parser_probe_t *probe, const char *s, size_t len)
{
    size_t bytes;
    char *copy;

    assert(s != NULL);

    if (!parser_alloc_pass(probe))
    {
        return NULL;
    }
    if (clopts_core_size_add(len, 1, &bytes) != 0)
    {
        return NULL;
    }

    copy = (char *)malloc(bytes);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return (char *)parser_alloc_track(probe, copy);
}

int clopts_parser_vec_push(clopts_parser_probe_t *probe, clopts_core_vec_t *vec, const void *elem)
{
    bool had_buffer;

    assert(vec != NULL);

    if (!parser_alloc_pass(probe))
    {
        return -1;
    }

    had_buffer = (vec->data != NULL);
    if (clopts_core_vec_push(vec, elem) != 0)
    {
        return -1;
    }
    if ((!had_buffer) && (vec->data != NULL))
    {
        (void)parser_alloc_track(probe, vec->data);
    }
    return 0;
}

void clopts_parser_vec_free(clopts_parser_probe_t *probe, clopts_core_vec_t *vec)
{
    assert(vec != NULL);

    if ((probe != NULL) && (vec->data != NULL))
    {
        probe->live--;
    }
    clopts_core_vec_free(vec);
}

void clopts_parser_free(clopts_parser_probe_t *probe, void *ptr)
{
    if ((probe != NULL) && (ptr != NULL))
    {
        probe->live--;
    }
    free(ptr);
}
