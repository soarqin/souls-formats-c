/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_aip.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

struct sf_aip {
    const sf_allocator_t *alloc;
    uint32_t version;
    sf_aip_block_id_t block_id;
    sf_aip_point_t *points;
    size_t point_count;
    size_t point_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static sf_result_t aip_push_point(sf_aip_t *a, sf_aip_point_t p) {
    if (a->point_count >= a->point_cap) {
        size_t new_cap = a->point_cap == 0 ? 8u : a->point_cap * 2u;
        sf_aip_point_t *np = (sf_aip_point_t *)sf_xalloc(a->alloc, new_cap * sizeof(*np));
        if (!np) return SF_ERR_OOM;
        if (a->points) {
            memcpy(np, a->points, a->point_count * sizeof(*np));
            sf_xfree(a->alloc, a->points);
        }
        a->points = np;
        a->point_cap = new_cap;
    }
    a->points[a->point_count++] = p;
    return SF_OK;
}

sf_result_t sf_aip_create(sf_aip_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_aip_t *a = (sf_aip_t *)sf_xalloc(alloc, sizeof(*a));
    if (!a) return SF_ERR_OOM;
    memset(a, 0, sizeof(*a));
    a->alloc = alloc;
    *out = a;
    return SF_OK;
}

void sf_aip_destroy(sf_aip_t *a) {
    if (!a) return;
    sf_xfree(a->alloc, a->points);
    sf_xfree(a->alloc, a);
}

bool sf_aip_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    return memcmp(bytes, "FPIA", 4) == 0;
}

uint32_t sf_aip_version(const sf_aip_t *a) { return a ? a->version : 0u; }
void sf_aip_set_version(sf_aip_t *a, uint32_t version) {
    if (a) a->version = version;
}

sf_aip_block_id_t sf_aip_block_id(const sf_aip_t *a) {
    sf_aip_block_id_t zero = {0, 0, 0, 0};
    return a ? a->block_id : zero;
}
void sf_aip_set_block_id(sf_aip_t *a, sf_aip_block_id_t block_id) {
    if (a) a->block_id = block_id;
}

size_t sf_aip_point_count(const sf_aip_t *a) { return a ? a->point_count : 0u; }

sf_result_t sf_aip_get_point(const sf_aip_t *a, size_t index, sf_aip_point_t *out) {
    SF_CHECK_ARG(a != NULL && out != NULL);
    if (index >= a->point_count) return SF_ERR_OUT_OF_RANGE;
    *out = a->points[index];
    return SF_OK;
}

sf_result_t sf_aip_add_point(sf_aip_t *a, sf_aip_point_t point) {
    SF_CHECK_ARG(a != NULL);
    return aip_push_point(a, point);
}

sf_result_t sf_aip_read_from_memory(sf_aip_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_aip_t *a = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_binary_reader_assert_ascii(r, "FPIA"); if (e != SF_OK) goto done;

    uint32_t version = 0;
    e = sf_binary_reader_read_u32(r, &version); if (e != SF_OK) goto done;

    sf_aip_block_id_t block_id;
    e = sf_binary_reader_read_u8(r, &block_id.index);  if (e != SF_OK) goto done;
    e = sf_binary_reader_read_u8(r, &block_id.region); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_u8(r, &block_id.block);  if (e != SF_OK) goto done;
    e = sf_binary_reader_read_u8(r, &block_id.area);   if (e != SF_OK) goto done;

    uint32_t point_count = 0;
    e = sf_binary_reader_read_u32(r, &point_count); if (e != SF_OK) goto done;

    e = sf_aip_create(&a, alloc); if (e != SF_OK) goto done;
    a->version = version;
    a->block_id = block_id;

    for (uint32_t i = 0; i < point_count; i++) {
        sf_aip_point_t pt;
        sf_vec3_t pos;
        e = sf_binary_reader_read_vec3(r, &pos); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &pt.rotation); if (e != SF_OK) goto done;
        pt.x = pos.x; pt.y = pos.y; pt.z = pos.z;
        e = aip_push_point(a, pt); if (e != SF_OK) goto done;
    }

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_aip_destroy(a); return e; }
    *out = a;
    return SF_OK;
}

sf_result_t sf_aip_write_to_memory(const sf_aip_t *a, void **out_bytes,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(a != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, false, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    e = sf_binary_writer_write_ascii(w, "FPIA", false); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u32(w, a->version); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, a->block_id.index);  if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, a->block_id.region); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, a->block_id.block);  if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, a->block_id.area);   if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u32(w, (uint32_t)a->point_count); if (e != SF_OK) goto done;

    for (size_t i = 0; i < a->point_count; i++) {
        sf_vec3_t pos = { a->points[i].x, a->points[i].y, a->points[i].z };
        e = sf_binary_writer_write_vec3(w, pos); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, a->points[i].rotation); if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
