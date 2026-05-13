/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_mlb.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

struct sf_mlb_ac5 {
    const sf_allocator_t *alloc;
    sf_mlb_ac5_resource_type_t type;
    size_t resource_count;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

sf_result_t sf_mlb_ac5_create(sf_mlb_ac5_t **out, sf_mlb_ac5_resource_type_t type,
                              const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_mlb_ac5_t *m = (sf_mlb_ac5_t *)sf_xalloc(alloc, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    memset(m, 0, sizeof(*m));
    m->alloc = alloc;
    m->type = type;
    *out = m;
    return SF_OK;
}

void sf_mlb_ac5_destroy(sf_mlb_ac5_t *m) {
    if (!m) return;
    sf_xfree(m->alloc, m);
}

sf_mlb_ac5_resource_type_t sf_mlb_ac5_resource_type(const sf_mlb_ac5_t *m) {
    return m ? m->type : SF_MLB_AC5_RESOURCE_MODEL;
}
size_t sf_mlb_ac5_resource_count(const sf_mlb_ac5_t *m) { return m ? m->resource_count : 0u; }

sf_result_t sf_mlb_ac5_read_from_memory(sf_mlb_ac5_t **out, const void *bytes,
                                        size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_mlb_ac5_t *m = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, true, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    int32_t entries_offset = 0, entries_count = 0, type_val = 0, zero = 0;
    e = sf_binary_reader_read_i32(r, &entries_offset); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &entries_count); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &type_val); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &zero); if (e != SF_OK) goto done;

    e = sf_mlb_ac5_create(&m, (sf_mlb_ac5_resource_type_t)type_val, alloc);
    if (e != SF_OK) goto done;
    m->resource_count = (size_t)entries_count;

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_mlb_ac5_destroy(m); return e; }
    *out = m;
    return SF_OK;
}

sf_result_t sf_mlb_ac5_write_to_memory(const sf_mlb_ac5_t *m, void **out_bytes,
                                       size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(m != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, true, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    int32_t entries_offset = 16;
    e = sf_binary_writer_write_i32(w, entries_offset); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)m->resource_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)m->type); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;

    for (size_t i = 0; i < m->resource_count; i++) {
        e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
