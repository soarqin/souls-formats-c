/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_clm2.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

struct sf_clm2_mesh {
    sf_clm2_entry_t *entries;
    size_t entry_count;
    size_t entry_cap;
};

struct sf_clm2 {
    const sf_allocator_t *alloc;
    struct sf_clm2_mesh *meshes;
    size_t mesh_count;
    size_t mesh_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static sf_result_t clm2_grow_meshes(sf_clm2_t *c) {
    if (c->mesh_count < c->mesh_cap) return SF_OK;
    size_t new_cap = c->mesh_cap == 0 ? 4u : c->mesh_cap * 2u;
    struct sf_clm2_mesh *nm = (struct sf_clm2_mesh *)sf_xalloc(
        c->alloc, new_cap * sizeof(*nm));
    if (!nm) return SF_ERR_OOM;
    memset(nm, 0, new_cap * sizeof(*nm));
    if (c->meshes) {
        memcpy(nm, c->meshes, c->mesh_count * sizeof(*nm));
        sf_xfree(c->alloc, c->meshes);
    }
    c->meshes = nm;
    c->mesh_cap = new_cap;
    return SF_OK;
}

static sf_result_t clm2_push_entry(sf_clm2_t *c, size_t mesh_idx, sf_clm2_entry_t entry) {
    struct sf_clm2_mesh *m = &c->meshes[mesh_idx];
    if (m->entry_count >= m->entry_cap) {
        size_t new_cap = m->entry_cap == 0 ? 8u : m->entry_cap * 2u;
        sf_clm2_entry_t *ne = (sf_clm2_entry_t *)sf_xalloc(c->alloc, new_cap * sizeof(*ne));
        if (!ne) return SF_ERR_OOM;
        if (m->entries) {
            memcpy(ne, m->entries, m->entry_count * sizeof(*ne));
            sf_xfree(c->alloc, m->entries);
        }
        m->entries = ne;
        m->entry_cap = new_cap;
    }
    m->entries[m->entry_count++] = entry;
    return SF_OK;
}

sf_result_t sf_clm2_create(sf_clm2_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_clm2_t *c = (sf_clm2_t *)sf_xalloc(alloc, sizeof(*c));
    if (!c) return SF_ERR_OOM;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    *out = c;
    return SF_OK;
}

void sf_clm2_destroy(sf_clm2_t *c) {
    if (!c) return;
    for (size_t i = 0; i < c->mesh_count; i++) {
        sf_xfree(c->alloc, c->meshes[i].entries);
    }
    sf_xfree(c->alloc, c->meshes);
    sf_xfree(c->alloc, c);
}

bool sf_clm2_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    return memcmp(bytes, "CLM2", 4) == 0;
}

size_t sf_clm2_mesh_count(const sf_clm2_t *c) { return c ? c->mesh_count : 0u; }

sf_result_t sf_clm2_add_mesh(sf_clm2_t *c, size_t *out_mesh_index) {
    SF_CHECK_ARG(c != NULL);
    TRY(clm2_grow_meshes(c));
    if (out_mesh_index) *out_mesh_index = c->mesh_count;
    c->meshes[c->mesh_count].entries = NULL;
    c->meshes[c->mesh_count].entry_count = 0;
    c->meshes[c->mesh_count].entry_cap = 0;
    c->mesh_count++;
    return SF_OK;
}

sf_result_t sf_clm2_get_mesh_entry_count(const sf_clm2_t *c, size_t mesh_index,
                                         size_t *out_count) {
    SF_CHECK_ARG(c != NULL && out_count != NULL);
    if (mesh_index >= c->mesh_count) return SF_ERR_OUT_OF_RANGE;
    *out_count = c->meshes[mesh_index].entry_count;
    return SF_OK;
}

sf_result_t sf_clm2_get_mesh_entry(const sf_clm2_t *c, size_t mesh_index,
                                   size_t entry_index, sf_clm2_entry_t *out_entry) {
    SF_CHECK_ARG(c != NULL && out_entry != NULL);
    if (mesh_index >= c->mesh_count) return SF_ERR_OUT_OF_RANGE;
    if (entry_index >= c->meshes[mesh_index].entry_count) return SF_ERR_OUT_OF_RANGE;
    *out_entry = c->meshes[mesh_index].entries[entry_index];
    return SF_OK;
}

sf_result_t sf_clm2_add_mesh_entry(sf_clm2_t *c, size_t mesh_index,
                                   int16_t unk00, int16_t unk02) {
    SF_CHECK_ARG(c != NULL);
    if (mesh_index >= c->mesh_count) return SF_ERR_OUT_OF_RANGE;
    sf_clm2_entry_t entry = { unk00, unk02 };
    return clm2_push_entry(c, mesh_index, entry);
}

sf_result_t sf_clm2_read_from_memory(sf_clm2_t **out, const void *bytes, size_t size,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_clm2_t *c = NULL;
    int32_t *entry_counts = NULL;
    uint32_t *entry_offsets = NULL;
    sf_result_t e = SF_OK;

    alloc = sf_alloc_or_default(alloc);

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_binary_reader_assert_ascii(r, "CLM2"); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i16_one(r, 1); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i16_one(r, 1); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;

    int32_t mesh_count = 0;
    e = sf_binary_reader_read_i32(r, &mesh_count); if (e != SF_OK) goto done;

    e = sf_binary_reader_assert_i32_one(r, 0x28); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0x28); if (e != SF_OK) goto done;

    if (mesh_count < 0) { e = SF_ERR_INVALID_ARG; goto done; }

    e = sf_clm2_create(&c, alloc); if (e != SF_OK) goto done;

    size_t mn = (size_t)mesh_count;
    if (mn > 0) {
        entry_counts = (int32_t *)sf_xalloc(alloc, mn * sizeof(int32_t));
        entry_offsets = (uint32_t *)sf_xalloc(alloc, mn * sizeof(uint32_t));
        if (!entry_counts || !entry_offsets) { e = SF_ERR_OOM; goto done; }
    }

    for (size_t i = 0; i < mn; i++) {
        e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &entry_counts[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_u32(r, &entry_offsets[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < mn; i++) {
        size_t mesh_idx = 0;
        e = sf_clm2_add_mesh(c, &mesh_idx); if (e != SF_OK) goto done;
        int32_t ec = entry_counts[i];
        if (ec < 0) { e = SF_ERR_INVALID_ARG; goto done; }
        if (ec == 0) continue;

        e = sf_binary_reader_step_in(r, (int64_t)entry_offsets[i]);
        if (e != SF_OK) goto done;
        for (int32_t j = 0; j < ec; j++) {
            int16_t u00 = 0, u02 = 0;
            e = sf_binary_reader_read_i16(r, &u00);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
            e = sf_binary_reader_read_i16(r, &u02);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
            e = sf_clm2_add_mesh_entry(c, mesh_idx, u00, u02);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
        }
        e = sf_binary_reader_step_out(r); if (e != SF_OK) goto done;
    }

done:
    sf_xfree(alloc, entry_counts);
    sf_xfree(alloc, entry_offsets);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_clm2_destroy(c); return e; }
    *out = c;
    return SF_OK;
}

sf_result_t sf_clm2_write_to_memory(const sf_clm2_t *c, void **out_bytes,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(c != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, false, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    e = sf_binary_writer_write_ascii(w, "CLM2", false); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, 1); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, 1); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)c->mesh_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x28); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x28); if (e != SF_OK) goto done;

    for (size_t i = 0; i < c->mesh_count; i++) {
        char key[48];
        snprintf(key, sizeof(key), "EntriesOffset%zu", i);
        e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, (int32_t)c->meshes[i].entry_count); if (e != SF_OK) goto done;
        e = sf_binary_writer_reserve_u32(w, key); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < c->mesh_count; i++) {
        char key[48];
        snprintf(key, sizeof(key), "EntriesOffset%zu", i);
        if (c->meshes[i].entry_count == 0) {
            e = sf_binary_writer_fill_u32(w, key, 0u); if (e != SF_OK) goto done;
        } else {
            e = sf_binary_writer_fill_u32(w, key,
                                           (uint32_t)sf_binary_writer_position(w));
            if (e != SF_OK) goto done;
            for (size_t j = 0; j < c->meshes[i].entry_count; j++) {
                e = sf_binary_writer_write_i16(w, c->meshes[i].entries[j].unk00);
                if (e != SF_OK) goto done;
                e = sf_binary_writer_write_i16(w, c->meshes[i].entries[j].unk02);
                if (e != SF_OK) goto done;
            }
            e = sf_binary_writer_pad(w, 8); if (e != SF_OK) goto done;
        }
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
