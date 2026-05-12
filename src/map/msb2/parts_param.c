/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msb2_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

void msb2_parts_param_free(sf_msb2_part_t *parts, int32_t count, const sf_allocator_t *a) {
    if (!parts || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, parts[i].data.name);
        sf_xfree(a, parts[i].data.sib_path);
        memset(&parts[i], 0, sizeof(parts[i]));
    }
}

static sf_result_t msb2_part_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                      msb2_part_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0, id = 0, zero = 0, sib_offset = 0, entity_offset = 0;
    int32_t type_offset = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->model_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &zero); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &sib_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->position); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->scale); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32s(r, 4, out->draw_groups); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32s(r, 4, out->disp_groups); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &entity_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_offset); if (rc != SF_OK) return rc;
    (void)id;
    if (type != MSB2_PART_MAP_PIECE || zero != 0 || name_offset == 0 || sib_offset == 0 ||
        entity_offset == 0 || type_offset == 0) {
        return SF_ERR_BAD_MAGIC;
    }
    out->type = MSB2_PART_MAP_PIECE;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + sib_offset, &out->sib_path, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + entity_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_id); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + type_offset); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_i32_one(r, 0);
}

sf_result_t msb2_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->part_count = count;
    if (count == 0) return SF_OK;
    out->parts = (sf_msb2_part_t *)sf_xalloc(a, (size_t)count * sizeof(*out->parts));
    if (!out->parts) return SF_ERR_OOM;
    memset(out->parts, 0, (size_t)count * sizeof(*out->parts));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->parts); out->parts = NULL; out->part_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msb2_part_read_one(r, offsets[i], &out->parts[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msb2_parts_param_free(out->parts, count, a); sf_xfree(a, out->parts); out->parts = NULL; out->part_count = 0; }
    return rc;
}

static sf_result_t msb2_part_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msb2_write_default_u32s(sf_binary_writer_t *w, const uint32_t values[4]) {
    uint32_t defaults[4] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
    const uint32_t *data = values ? values : defaults;
    return sf_binary_writer_write_u32s(w, 4, data);
}

static sf_result_t msb2_part_write_entry(sf_binary_writer_t *w, const void *entry,
                                         size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msb2_part_t *part = (const sf_msb2_part_t *)entry;
    if (part->data.type != MSB2_PART_MAP_PIECE) return SF_ERR_UNSUPPORTED_VERSION;
    char name_key[32], sib_key[32], entity_key[32], type_key[32];
    snprintf(name_key, sizeof name_key, "Msb2PartName%zu", index);
    snprintf(sib_key, sizeof sib_key, "Msb2PartSib%zu", index);
    snprintf(entity_key, sizeof entity_key, "Msb2PartEntity%zu", index);
    snprintf(type_key, sizeof type_key, "Msb2PartType%zu", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_u32(w, (uint32_t)part->data.type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, (int32_t)index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->data.model_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, sib_key), return rc);
    rc = sf_binary_writer_write_vec3(w, part->data.position); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->data.rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->data.scale); if (rc != SF_OK) return rc;
    rc = msb2_write_default_u32s(w, part->data.draw_groups); if (rc != SF_OK) return rc;
    rc = msb2_write_default_u32s(w, part->data.disp_groups); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, entity_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, type_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, msb2_part_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, part->data.name ? part->data.name : "", true); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_part_fill_rel_i32(w, sib_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, part->data.sib_path ? part->data.sib_path : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_part_fill_rel_i32(w, entity_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, part->data.entity_id); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_part_fill_rel_i32(w, type_key, start), return rc);
    return sf_binary_writer_write_i32(w, 0);
}

sf_result_t msb2_parts_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2) {
    if (!w || !msb2 || msb2->part_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "PARTS_PARAM_ST", "Msb2NextList3", msb2->parts,
                                       (size_t)msb2->part_count, sizeof(*msb2->parts),
                                       msb2_part_write_entry, NULL);
}
