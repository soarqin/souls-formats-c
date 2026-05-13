/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_f2tr.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

struct sf_f2tr_entry {
    char *name;
    int16_t *indices;
    size_t index_count;
};

struct sf_f2tr {
    const sf_allocator_t *alloc;
    bool big_endian;
    struct sf_f2tr_entry *entries;
    size_t entry_count;
    size_t entry_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static sf_result_t f2tr_grow_entries(sf_f2tr_t *f) {
    if (f->entry_count < f->entry_cap) return SF_OK;
    size_t new_cap = f->entry_cap == 0 ? 8u : f->entry_cap * 2u;
    struct sf_f2tr_entry *ne = (struct sf_f2tr_entry *)sf_xalloc(
        f->alloc, new_cap * sizeof(*ne));
    if (!ne) return SF_ERR_OOM;
    memset(ne, 0, new_cap * sizeof(*ne));
    if (f->entries) {
        memcpy(ne, f->entries, f->entry_count * sizeof(*ne));
        sf_xfree(f->alloc, f->entries);
    }
    f->entries = ne;
    f->entry_cap = new_cap;
    return SF_OK;
}

sf_result_t sf_f2tr_create(sf_f2tr_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_f2tr_t *f = (sf_f2tr_t *)sf_xalloc(alloc, sizeof(*f));
    if (!f) return SF_ERR_OOM;
    memset(f, 0, sizeof(*f));
    f->alloc = alloc;
    *out = f;
    return SF_OK;
}

void sf_f2tr_destroy(sf_f2tr_t *f) {
    if (!f) return;
    for (size_t i = 0; i < f->entry_count; i++) {
        sf_xfree(f->alloc, f->entries[i].name);
        sf_xfree(f->alloc, f->entries[i].indices);
    }
    sf_xfree(f->alloc, f->entries);
    sf_xfree(f->alloc, f);
}

bool sf_f2tr_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    return memcmp(bytes, "F2TR", 4) == 0;
}

bool sf_f2tr_big_endian(const sf_f2tr_t *f) { return f ? f->big_endian : false; }
void sf_f2tr_set_big_endian(sf_f2tr_t *f, bool big_endian) {
    if (f) f->big_endian = big_endian;
}

size_t sf_f2tr_entry_count(const sf_f2tr_t *f) { return f ? f->entry_count : 0u; }

const char *sf_f2tr_get_entry_name(const sf_f2tr_t *f, size_t index) {
    if (!f || index >= f->entry_count) return NULL;
    return f->entries[index].name;
}

sf_result_t sf_f2tr_get_entry_indices(const sf_f2tr_t *f, size_t index,
                                      const int16_t **out_indices, size_t *out_count) {
    SF_CHECK_ARG(f != NULL && out_indices != NULL && out_count != NULL);
    if (index >= f->entry_count) return SF_ERR_OUT_OF_RANGE;
    *out_indices = f->entries[index].indices;
    *out_count = f->entries[index].index_count;
    return SF_OK;
}

sf_result_t sf_f2tr_add_entry(sf_f2tr_t *f, const char *name_utf8,
                              const int16_t *indices, size_t count) {
    SF_CHECK_ARG(f != NULL && name_utf8 != NULL && (count == 0 || indices != NULL));

    TRY(f2tr_grow_entries(f));

    size_t name_len = strlen(name_utf8);
    char *name_copy = (char *)sf_xalloc(f->alloc, name_len + 1u);
    if (!name_copy) return SF_ERR_OOM;
    memcpy(name_copy, name_utf8, name_len + 1u);

    int16_t *indices_copy = NULL;
    if (count > 0) {
        indices_copy = (int16_t *)sf_xalloc(f->alloc, count * sizeof(int16_t));
        if (!indices_copy) {
            sf_xfree(f->alloc, name_copy);
            return SF_ERR_OOM;
        }
        memcpy(indices_copy, indices, count * sizeof(int16_t));
    }

    f->entries[f->entry_count].name = name_copy;
    f->entries[f->entry_count].indices = indices_copy;
    f->entries[f->entry_count].index_count = count;
    f->entry_count++;
    return SF_OK;
}

sf_result_t sf_f2tr_read_from_memory(sf_f2tr_t **out, const void *bytes, size_t size,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_f2tr_t *f = NULL;
    int32_t *name_offsets = NULL;
    int32_t *indices_offsets = NULL;
    int16_t *index_counts = NULL;
    sf_result_t e = SF_OK;

    alloc = sf_alloc_or_default(alloc);

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_binary_reader_assert_ascii(r, "F2TR"); if (e != SF_OK) goto done;

    uint8_t endian_byte = 0;
    static const uint8_t endian_options[2] = {0x00u, 0xFFu};
    e = sf_binary_reader_assert_u8(r, 2, endian_options, &endian_byte);
    if (e != SF_OK) goto done;
    bool big_endian = (endian_byte == 0xFFu);
    sf_binary_reader_set_big_endian(r, big_endian);

    e = sf_binary_reader_assert_u8_one(r, 0u); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i16_one(r, 1); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i16_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i16_one(r, 0x10); if (e != SF_OK) goto done;

    int16_t entry_count = 0;
    e = sf_binary_reader_read_i16(r, &entry_count); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i16_one(r, 0xC); if (e != SF_OK) goto done;

    if (entry_count < 0) { e = SF_ERR_INVALID_ARG; goto done; }

    e = sf_f2tr_create(&f, alloc); if (e != SF_OK) goto done;
    f->big_endian = big_endian;

    size_t n = (size_t)entry_count;
    if (n > 0) {
        name_offsets = (int32_t *)sf_xalloc(alloc, n * sizeof(int32_t));
        indices_offsets = (int32_t *)sf_xalloc(alloc, n * sizeof(int32_t));
        index_counts = (int16_t *)sf_xalloc(alloc, n * sizeof(int16_t));
        if (!name_offsets || !indices_offsets || !index_counts) { e = SF_ERR_OOM; goto done; }
    }

    for (size_t i = 0; i < n; i++) {
        e = sf_binary_reader_read_i32(r, &name_offsets[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &indices_offsets[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i16(r, &index_counts[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_assert_i16_one(r, 0); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < n; i++) {
        e = f2tr_grow_entries(f); if (e != SF_OK) goto done;

        char *name = NULL;
        e = sf_binary_reader_get_utf16(r, (int64_t)name_offsets[i], &name, NULL);
        if (e != SF_OK) goto done;

        int16_t *idx = NULL;
        size_t ic = (size_t)(index_counts[i] < 0 ? 0 : index_counts[i]);
        if (ic > 0) {
            idx = (int16_t *)sf_xalloc(alloc, ic * sizeof(int16_t));
            if (!idx) { sf_free(alloc, name); e = SF_ERR_OOM; goto done; }
            e = sf_binary_reader_get_i16s(r, (int64_t)indices_offsets[i], ic, idx);
            if (e != SF_OK) { sf_free(alloc, name); sf_xfree(alloc, idx); goto done; }
        }

        f->entries[f->entry_count].name = name;
        f->entries[f->entry_count].indices = idx;
        f->entries[f->entry_count].index_count = ic;
        f->entry_count++;
    }

done:
    sf_xfree(alloc, name_offsets);
    sf_xfree(alloc, indices_offsets);
    sf_xfree(alloc, index_counts);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_f2tr_destroy(f); return e; }
    *out = f;
    return SF_OK;
}

sf_result_t sf_f2tr_write_to_memory(const sf_f2tr_t *f, void **out_bytes,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(f != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, f->big_endian, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    e = sf_binary_writer_write_ascii(w, "F2TR", false); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, f->big_endian ? 0xFFu : 0x00u); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, 0u); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, 1); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, 0x10); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, (int16_t)f->entry_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, 0xC); if (e != SF_OK) goto done;

    for (size_t i = 0; i < f->entry_count; i++) {
        char name_key[48], idx_key[48];
        snprintf(name_key, sizeof(name_key), "NameOffset%zu", i);
        snprintf(idx_key, sizeof(idx_key), "IndicesOffset%zu", i);
        e = sf_binary_writer_reserve_i32(w, name_key); if (e != SF_OK) goto done;
        e = sf_binary_writer_reserve_i32(w, idx_key); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i16(w, (int16_t)f->entries[i].index_count); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i16(w, 0); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < f->entry_count; i++) {
        char idx_key[48];
        snprintf(idx_key, sizeof(idx_key), "IndicesOffset%zu", i);
        e = sf_binary_writer_fill_i32(w, idx_key,
                                       (int32_t)sf_binary_writer_position(w));
        if (e != SF_OK) goto done;
        if (f->entries[i].index_count > 0) {
            e = sf_binary_writer_write_i16s(w, f->entries[i].index_count,
                                            f->entries[i].indices);
            if (e != SF_OK) goto done;
        }
    }

    for (size_t i = 0; i < f->entry_count; i++) {
        char name_key[48];
        snprintf(name_key, sizeof(name_key), "NameOffset%zu", i);
        e = sf_binary_writer_fill_i32(w, name_key,
                                       (int32_t)sf_binary_writer_position(w));
        if (e != SF_OK) goto done;
        e = sf_binary_writer_write_utf16(w, f->entries[i].name, true);
        if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
