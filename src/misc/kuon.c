/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_kuon.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

#define SF_KUON_BND_MAGIC "BND\0"
#define SF_KUON_BND_HEADER_SIZE 16u

struct sf_kuon_bnd {
    const sf_allocator_t *alloc;
    int32_t file_version;
    int32_t file_size;
    size_t file_count;
};

bool sf_kuon_bnd_is(const void *bytes, size_t size) {
    if (!bytes || size < SF_KUON_BND_HEADER_SIZE) return false;
    return memcmp(bytes, SF_KUON_BND_MAGIC, 4) == 0;
}

void sf_kuon_bnd_destroy(sf_kuon_bnd_t *bnd) {
    if (!bnd) return;
    sf_xfree(bnd->alloc, bnd);
}

size_t sf_kuon_bnd_file_count(const sf_kuon_bnd_t *bnd) {
    return bnd ? bnd->file_count : 0u;
}

int32_t sf_kuon_bnd_file_version(const sf_kuon_bnd_t *bnd) {
    return bnd ? bnd->file_version : 0;
}

sf_result_t sf_kuon_bnd_read_from_memory(sf_kuon_bnd_t **out, const void *bytes,
                                         size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < SF_KUON_BND_HEADER_SIZE) return SF_ERR_TRUNCATED;
    if (memcmp(bytes, SF_KUON_BND_MAGIC, 4) != 0) return SF_ERR_BAD_MAGIC;

    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_kuon_bnd_t *bnd = NULL;
    sf_result_t e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_binary_reader_assert_u8_one(r, (uint8_t)'B'); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, (uint8_t)'N'); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, (uint8_t)'D'); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, 0x00);         if (e != SF_OK) goto done;

    int32_t file_version = 0;
    int32_t file_size = 0;
    int32_t file_count = 0;
    e = sf_binary_reader_read_i32(r, &file_version); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &file_size);    if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &file_count);   if (e != SF_OK) goto done;

    if (file_count < 0) { e = SF_ERR_TRUNCATED; goto done; }

    bnd = (sf_kuon_bnd_t *)sf_xalloc(alloc, sizeof(*bnd));
    if (!bnd) { e = SF_ERR_OOM; goto done; }
    memset(bnd, 0, sizeof(*bnd));
    bnd->alloc = alloc;
    bnd->file_version = file_version;
    bnd->file_size = file_size;
    bnd->file_count = (size_t)file_count;

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_kuon_bnd_destroy(bnd); return e; }
    *out = bnd;
    return SF_OK;
}
