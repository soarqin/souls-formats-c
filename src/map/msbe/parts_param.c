/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Elden Ring MSBE PartsParam.
 * Mirrors SoulsFormats/Formats/MSB/MSBE/PartsParam.cs
 */

#include "msbe_internal.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbe_part_type_is_known(uint32_t type) {
    return type == 0 || type == 2 || type == 4 || type == 5 || type == 9 || type == 10 ||
           type == 11 || type == 13;
}

void msbe_parts_param_free(sf_msbe_part_t *parts, int32_t count, const sf_allocator_t *a) {
    if (!parts || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, parts[i].data.name);
        memset(&parts[i], 0, sizeof(parts[i]));
    }
}

static sf_result_t msbe_part_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                      msbe_part_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0, sib_offset = 0, entity_offset = 0, type_offset = 0;
    int32_t instance_id = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &instance_id); if (rc != SF_OK) return rc;
    (void)instance_id;
    rc = sf_binary_reader_read_u32(r, &out->type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->other_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->model_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &sib_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 44); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 16); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &entity_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_offset); if (rc != SF_OK) return rc;

    if (!msbe_part_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || sib_offset == 0 || entity_offset == 0 || type_offset == 0) return SF_ERR_BAD_MAGIC;
    return sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
}

sf_result_t msbe_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->part_count = count;
    out->parts = NULL;
    if (count == 0) return SF_OK;
    out->parts = (sf_msbe_part_t *)sf_xalloc(a, (size_t)count * sizeof(*out->parts));
    if (!out->parts) return SF_ERR_OOM;
    memset(out->parts, 0, (size_t)count * sizeof(*out->parts));
    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbe_part_read_one(r, entry_offsets[i], &out->parts[i].data, a);
            if (rc != SF_OK) break;
        }
    }
    sf_xfree(a, entry_offsets);
    return rc;
}

static sf_result_t msbe_part_write_one(sf_binary_writer_t *w, const msbe_part_t *part,
                                       int32_t id, int32_t index) {
    char name_key[32], sib_key[32], entity_key[32], type_key[32];
    snprintf(name_key, sizeof name_key, "MsbePartName%d", index);
    snprintf(sib_key, sizeof sib_key, "MsbePartSib%d", index);
    snprintf(entity_key, sizeof entity_key, "MsbePartEntity%d", index);
    snprintf(type_key, sizeof type_key, "MsbePartType%d", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_key), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, part->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->other_id != -1 ? part->other_id : id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->model_index); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, sib_key), return rc);
    for (int i = 0; i < 9; i++) { rc = sf_binary_writer_write_f32(w, (i >= 6) ? 1.0f : 0.0f); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_write_u32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, UINT32_MAX); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i64(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i64(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, entity_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, type_key), return rc);
    for (int i = 0; i < 10; i++) { rc = sf_binary_writer_write_i64(w, 0); if (rc != SF_OK) return rc; }
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, part->name ? part->name : "", true); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, sib_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, entity_key, sf_binary_writer_position(w) - start), return rc);
    for (int i = 0; i < 22; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, type_key, sf_binary_writer_position(w) - start), return rc);
    for (int i = 0; i < 16; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    return sf_binary_writer_pad(w, 8);
}

sf_result_t msbe_parts_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe) {
    if (!w || !msbe) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 73); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbe->part_count + 1); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbeNameOff5"), return rc);
    for (int32_t i = 0; i < msbe->part_count; i++) {
        char entry_key[32]; snprintf(entry_key, sizeof entry_key, "MsbePartEntry%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, entry_key), return rc);
    }
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbeNextList5"), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, "MsbeNameOff5", sf_binary_writer_position(w)), return rc);
    rc = sf_binary_writer_write_utf16(w, "PARTS_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbe->part_count; i++) {
        char entry_key[32]; snprintf(entry_key, sizeof entry_key, "MsbePartEntry%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, entry_key, sf_binary_writer_position(w)), return rc);
        rc = msbe_part_write_one(w, &msbe->parts[i].data, i, i); if (rc != SF_OK) return rc;
    }
    return SF_OK;
}
