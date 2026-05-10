/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Result code → string mapping, default allocator, and the generic sf_free
 * forwarder.
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include <stdlib.h>
#include <string.h>

/*  Keep this table in lock-step with sf_result_t in sf_common.h.
 *  Build-time assert below catches drift. */
static const char *const k_result_strings[] = {
    [SF_OK]                       = "ok",
    [SF_ERR_INVALID_ARG]          = "invalid argument",
    [SF_ERR_OOM]                  = "out of memory",
    [SF_ERR_IO]                   = "io error",
    [SF_ERR_BAD_MAGIC]            = "bad file magic",
    [SF_ERR_UNSUPPORTED_VERSION]  = "unsupported version",
    [SF_ERR_TRUNCATED]            = "truncated input",
    [SF_ERR_OUT_OF_RANGE]         = "value out of range",
    [SF_ERR_DECOMPRESS]           = "decompression error",
    [SF_ERR_OODLE_NOT_FOUND]      = "oodle DLL not found",
    [SF_ERR_CRYPTO]               = "crypto error",
    [SF_ERR_NOT_FOUND]            = "entry not found",
    [SF_ERR_ALREADY_EXISTS]       = "entry already exists",
    [SF_ERR_INTERNAL]             = "internal error",
};

_Static_assert(
    sizeof(k_result_strings) / sizeof(k_result_strings[0]) == SF_RESULT_COUNT_,
    "k_result_strings must have one entry per sf_result_t value");

const char *sf_result_str(sf_result_t r) {
    if ((unsigned)r >= (unsigned)SF_RESULT_COUNT_) {
        return "(unknown sf_result_t)";
    }
    const char *s = k_result_strings[r];
    return s ? s : "(unknown sf_result_t)";
}

/*  Thread-local detail buffer. Phase 0: read-only stub returning NULL.
 *  Phase 1 will add internal `sf_set_last_error_detail(fmt, ...)`. */
const char *sf_last_error_detail(void) {
    return NULL;
}

/*  Default allocator: malloc / realloc / free. */
static void *_sf_default_alloc(size_t size, void *user) {
    (void)user;
    return malloc(size);
}

static void *_sf_default_realloc(void *p, size_t old_size, size_t new_size, void *user) {
    (void)old_size;
    (void)user;
    return realloc(p, new_size);
}

static void _sf_default_free(void *p, void *user) {
    (void)user;
    free(p);
}

static const sf_allocator_t k_default_allocator = {
    .alloc   = _sf_default_alloc,
    .realloc = _sf_default_realloc,
    .free    = _sf_default_free,
    .user    = NULL,
};

const sf_allocator_t *sf_default_allocator(void) {
    return &k_default_allocator;
}

void sf_free(const sf_allocator_t *a, void *ptr) {
    if (!ptr) return;
    if (!a) a = sf_default_allocator();
    a->free(ptr, a->user);
}
