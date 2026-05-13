/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FMB (.expb) reader / writer.
 * Upstream reference: SoulsFormats/Formats/FMB.cs
 */

#include "souls_formats/sf_fmb.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

struct sf_fmb {
    const sf_allocator_t *alloc;
    int32_t               unk20;
    sf_fmb_entry_t       *entries;
    size_t                entry_count;
    size_t                entry_capacity;
};

static sf_fmb_entry_kind_t fmb_classify_type(int32_t type) {
    switch (type) {
    case 2: case 5: case 6: case 12: case 14:
    case 21: case 31: case 32: case 33: case 34: case 43:
        return SF_FMB_ENTRY_KIND_PLAIN;
    case 7: case 11:
        return SF_FMB_ENTRY_KIND_STRING;
    case 1: case 3: case 4: case 8: case 51: case 61:
        return SF_FMB_ENTRY_KIND_DOUBLE;
    case 52:
        return SF_FMB_ENTRY_KIND_DOUBLE2;
    default:
        return (sf_fmb_entry_kind_t)-1;
    }
}

sf_result_t sf_fmb_create(sf_fmb_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_fmb_t *f = (sf_fmb_t *)sf_xalloc(alloc, sizeof(*f));
    if (!f) return SF_ERR_OOM;
    memset(f, 0, sizeof(*f));
    f->alloc = alloc;
    *out = f;
    return SF_OK;
}

void sf_fmb_destroy(sf_fmb_t *f) {
    if (!f) return;
    if (f->entries) {
        for (size_t i = 0; i < f->entry_count; i++) {
            sf_xfree(f->alloc, f->entries[i].string_value);
        }
        sf_xfree(f->alloc, f->entries);
    }
    sf_xfree(f->alloc, f);
}

bool sf_fmb_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    return memcmp(bytes, "FMB ", 4) == 0;
}

int32_t sf_fmb_get_unk20(const sf_fmb_t *f) {
    return f ? f->unk20 : 0;
}

void sf_fmb_set_unk20(sf_fmb_t *f, int32_t v) {
    if (f) f->unk20 = v;
}

size_t sf_fmb_entry_count(const sf_fmb_t *f) {
    return f ? f->entry_count : 0u;
}

sf_result_t sf_fmb_get_entry(const sf_fmb_t *f, size_t index,
                             const sf_fmb_entry_t **out) {
    SF_CHECK_ARG(f != NULL && out != NULL);
    if (index >= f->entry_count) return SF_ERR_OUT_OF_RANGE;
    *out = &f->entries[index];
    return SF_OK;
}

static sf_result_t fmb_reserve_one(sf_fmb_t *f, sf_fmb_entry_t **out_slot) {
    if (f->entry_count == f->entry_capacity) {
        size_t new_cap = f->entry_capacity ? f->entry_capacity * 2 : 8;
        size_t old_bytes = f->entry_capacity * sizeof(sf_fmb_entry_t);
        size_t new_bytes = new_cap * sizeof(sf_fmb_entry_t);
        sf_fmb_entry_t *p = (sf_fmb_entry_t *)sf_xrealloc(
            f->alloc, f->entries, old_bytes, new_bytes);
        if (!p) return SF_ERR_OOM;
        memset((uint8_t *)p + old_bytes, 0, new_bytes - old_bytes);
        f->entries = p;
        f->entry_capacity = new_cap;
    }
    *out_slot = &f->entries[f->entry_count++];
    return SF_OK;
}

sf_result_t sf_fmb_add_plain_entry(sf_fmb_t *f, int32_t type) {
    SF_CHECK_ARG(f != NULL);
    if (fmb_classify_type(type) != SF_FMB_ENTRY_KIND_PLAIN)
        return SF_ERR_INVALID_ARG;
    sf_fmb_entry_t *slot = NULL;
    TRY(fmb_reserve_one(f, &slot));
    slot->type          = type;
    slot->kind          = SF_FMB_ENTRY_KIND_PLAIN;
    slot->string_value  = NULL;
    slot->double_value  = 0.0;
    slot->double_value2 = 0.0;
    return SF_OK;
}

sf_result_t sf_fmb_add_string_entry(sf_fmb_t *f, int32_t type, const char *value) {
    SF_CHECK_ARG(f != NULL && value != NULL);
    if (fmb_classify_type(type) != SF_FMB_ENTRY_KIND_STRING)
        return SF_ERR_INVALID_ARG;
    char *dup = sf_strdup(f->alloc, value);
    if (!dup) return SF_ERR_OOM;
    sf_fmb_entry_t *slot = NULL;
    sf_result_t e = fmb_reserve_one(f, &slot);
    if (e != SF_OK) { sf_xfree(f->alloc, dup); return e; }
    slot->type          = type;
    slot->kind          = SF_FMB_ENTRY_KIND_STRING;
    slot->string_value  = dup;
    slot->double_value  = 0.0;
    slot->double_value2 = 0.0;
    return SF_OK;
}

sf_result_t sf_fmb_add_double_entry(sf_fmb_t *f, int32_t type, double value) {
    SF_CHECK_ARG(f != NULL);
    if (fmb_classify_type(type) != SF_FMB_ENTRY_KIND_DOUBLE)
        return SF_ERR_INVALID_ARG;
    sf_fmb_entry_t *slot = NULL;
    TRY(fmb_reserve_one(f, &slot));
    slot->type          = type;
    slot->kind          = SF_FMB_ENTRY_KIND_DOUBLE;
    slot->string_value  = NULL;
    slot->double_value  = value;
    slot->double_value2 = 0.0;
    return SF_OK;
}

sf_result_t sf_fmb_add_double2_entry(sf_fmb_t *f, int32_t type,
                                     double value1, double value2) {
    SF_CHECK_ARG(f != NULL);
    if (fmb_classify_type(type) != SF_FMB_ENTRY_KIND_DOUBLE2)
        return SF_ERR_INVALID_ARG;
    sf_fmb_entry_t *slot = NULL;
    TRY(fmb_reserve_one(f, &slot));
    slot->type          = type;
    slot->kind          = SF_FMB_ENTRY_KIND_DOUBLE2;
    slot->string_value  = NULL;
    slot->double_value  = value1;
    slot->double_value2 = value2;
    return SF_OK;
}

static sf_result_t fmb_read_entry(sf_binary_reader_t *r, sf_fmb_entry_t *out,
                                  const sf_allocator_t *alloc) {
    int32_t type = 0;
    TRY(sf_binary_reader_read_i32(r, &type));
    TRY(sf_binary_reader_assert_i32_one(r, 0));

    sf_fmb_entry_kind_t kind = fmb_classify_type(type);
    out->type          = type;
    out->kind          = kind;
    out->string_value  = NULL;
    out->double_value  = 0.0;
    out->double_value2 = 0.0;

    switch (kind) {
    case SF_FMB_ENTRY_KIND_PLAIN: {
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        return SF_OK;
    }
    case SF_FMB_ENTRY_KIND_STRING: {
        int64_t value_off = 0;
        TRY(sf_binary_reader_read_i64(r, &value_off));
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        char *str = NULL;
        TRY(sf_binary_reader_get_ascii(r, 0x40 + value_off, &str, NULL));
        (void)alloc;
        out->string_value = str;
        return SF_OK;
    }
    case SF_FMB_ENTRY_KIND_DOUBLE: {
        double v = 0.0;
        TRY(sf_binary_reader_read_f64(r, &v));
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        out->double_value = v;
        return SF_OK;
    }
    case SF_FMB_ENTRY_KIND_DOUBLE2: {
        double v1 = 0.0, v2 = 0.0;
        TRY(sf_binary_reader_read_f64(r, &v1));
        TRY(sf_binary_reader_read_f64(r, &v2));
        TRY(sf_binary_reader_assert_i64_one(r, 0));
        out->double_value  = v1;
        out->double_value2 = v2;
        return SF_OK;
    }
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

sf_result_t sf_fmb_read_from_memory(sf_fmb_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t      *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_fmb_t          *fmb = NULL;
    int64_t           *entry_offsets = NULL;
    sf_result_t        err = SF_OK;

    err = sf_istream_open_memory(&s, bytes, size, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_reader_create(&r, s, false, alloc);
    if (err != SF_OK) { sf_istream_close(s); return err; }

    err = sf_binary_reader_assert_ascii(r, "FMB "); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 1);    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 1);    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0);    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i64_one(r, 0x20); if (err != SF_OK) goto done;

    err = sf_binary_reader_assert_i32_one(r, 0);    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0);    if (err != SF_OK) goto done;
    int32_t unk20 = 0;
    err = sf_binary_reader_read_i32(r, &unk20);     if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0);    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i64_one(r, 0x30); if (err != SF_OK) goto done;

    err = sf_binary_reader_assert_i32_one(r, 0);    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0);    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i64_one(r, 0x40); if (err != SF_OK) goto done;

    int32_t entry_count = 0;
    err = sf_binary_reader_read_i32(r, &entry_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0);      if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i64_one(r, 0x10);   if (err != SF_OK) goto done;

    if (entry_count < 0) { err = SF_ERR_BAD_MAGIC; goto done; }

    err = sf_fmb_create(&fmb, alloc);
    if (err != SF_OK) goto done;
    fmb->unk20 = unk20;

    if (entry_count > 0) {
        entry_offsets = (int64_t *)sf_xalloc(alloc,
            (size_t)entry_count * sizeof(int64_t));
        if (!entry_offsets) { err = SF_ERR_OOM; goto done; }
        err = sf_binary_reader_read_i64s(r, (size_t)entry_count, entry_offsets);
        if (err != SF_OK) goto done;

        fmb->entries = (sf_fmb_entry_t *)sf_xalloc(alloc,
            (size_t)entry_count * sizeof(sf_fmb_entry_t));
        if (!fmb->entries) { err = SF_ERR_OOM; goto done; }
        memset(fmb->entries, 0, (size_t)entry_count * sizeof(sf_fmb_entry_t));
        fmb->entry_capacity = (size_t)entry_count;
        fmb->entry_count    = (size_t)entry_count;
    }

    for (int32_t i = 0; i < entry_count; i++) {
        int64_t pos = 0x40 + entry_offsets[i];
        err = sf_istream_seek(sf_binary_reader_stream(r), pos);
        if (err != SF_OK) goto done;
        err = fmb_read_entry(r, &fmb->entries[i], alloc);
        if (err != SF_OK) goto done;
    }

done:
    sf_xfree(alloc, entry_offsets);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (err != SF_OK) { sf_fmb_destroy(fmb); return err; }
    *out = fmb;
    return SF_OK;
}

static sf_result_t fmb_write_entry(sf_binary_writer_t *w, const sf_fmb_entry_t *e,
                                   size_t index) {
    TRY(sf_binary_writer_write_i32(w, e->type));
    TRY(sf_binary_writer_write_i32(w, 0));

    char label[64];

    switch (e->kind) {
    case SF_FMB_ENTRY_KIND_PLAIN:
        TRY(sf_binary_writer_write_i64(w, 0));
        TRY(sf_binary_writer_write_i64(w, 0));
        TRY(sf_binary_writer_write_i64(w, 0));
        return SF_OK;
    case SF_FMB_ENTRY_KIND_STRING:
        snprintf(label, sizeof(label), "ValueOffset[%zu]", index);
        TRY(sf_binary_writer_reserve_i64(w, label));
        TRY(sf_binary_writer_write_i64(w, 0));
        TRY(sf_binary_writer_write_i64(w, 0));
        return SF_OK;
    case SF_FMB_ENTRY_KIND_DOUBLE:
        TRY(sf_binary_writer_write_f64(w, e->double_value));
        TRY(sf_binary_writer_write_i64(w, 0));
        TRY(sf_binary_writer_write_i64(w, 0));
        return SF_OK;
    case SF_FMB_ENTRY_KIND_DOUBLE2:
        TRY(sf_binary_writer_write_f64(w, e->double_value));
        TRY(sf_binary_writer_write_f64(w, e->double_value2));
        TRY(sf_binary_writer_write_i64(w, 0));
        return SF_OK;
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t fmb_write_offset_data(sf_binary_writer_t *w,
                                         const sf_fmb_entry_t *e, size_t index) {
    if (e->kind != SF_FMB_ENTRY_KIND_STRING) return SF_OK;
    char label[64];
    snprintf(label, sizeof(label), "ValueOffset[%zu]", index);
    int64_t pos = sf_binary_writer_position(w);
    TRY(sf_binary_writer_fill_i64(w, label, pos - 0x40));
    const char *value = e->string_value ? e->string_value : "";
    TRY(sf_binary_writer_write_ascii(w, value, true));
    return SF_OK;
}

sf_result_t sf_fmb_write_to_memory(const sf_fmb_t *fmb, uint8_t **out_data,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmb != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t       *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t         err = SF_OK;

    err = sf_ostream_open_memory(&s, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_writer_create(&w, s, false, alloc);
    if (err != SF_OK) { sf_ostream_close(s); return err; }

    err = sf_binary_writer_write_ascii(w, "FMB ", false); if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, 1);    if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, 1);    if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, 0);    if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i64(w, 0x20); if (err != SF_OK) goto done;

    err = sf_binary_writer_write_i32(w, 0);              if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, 0);              if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, fmb->unk20);     if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, 0);              if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i64(w, 0x30);           if (err != SF_OK) goto done;

    err = sf_binary_writer_write_i32(w, 0);    if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, 0);    if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i64(w, 0x40); if (err != SF_OK) goto done;

    err = sf_binary_writer_write_i32(w, (int32_t)fmb->entry_count); if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i32(w, 0);                          if (err != SF_OK) goto done;
    err = sf_binary_writer_write_i64(w, 0x10);                       if (err != SF_OK) goto done;

    for (size_t i = 0; i < fmb->entry_count; i++) {
        char label[64];
        snprintf(label, sizeof(label), "EntryOffset[%zu]", i);
        err = sf_binary_writer_reserve_i64(w, label);
        if (err != SF_OK) goto done;
    }

    err = sf_binary_writer_pad(w, 0x10);
    if (err != SF_OK) goto done;

    for (size_t i = 0; i < fmb->entry_count; i++) {
        char label[64];
        snprintf(label, sizeof(label), "EntryOffset[%zu]", i);
        int64_t pos = sf_binary_writer_position(w);
        err = sf_binary_writer_fill_i64(w, label, pos - 0x40);
        if (err != SF_OK) goto done;
        err = fmb_write_entry(w, &fmb->entries[i], i);
        if (err != SF_OK) goto done;
    }

    for (size_t i = 0; i < fmb->entry_count; i++) {
        err = fmb_write_offset_data(w, &fmb->entries[i], i);
        if (err != SF_OK) goto done;
    }

    err = sf_binary_writer_pad(w, 0x10);
    if (err != SF_OK) goto done;

    err = sf_binary_writer_finish_bytes(w, out_data, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return err;
}
