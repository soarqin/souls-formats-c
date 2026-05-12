/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbfa_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

void msbfa_parts_param_free(sf_msbfa_part_t *parts, int32_t count, const sf_allocator_t *a) {
    if (!parts || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, parts[i].data.name);
        sf_xfree(a, parts[i].data.resource_path);
        memset(&parts[i], 0, sizeof(parts[i]));
    }
}

static sf_result_t msbfa_part_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                        msbfa_part_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0, id = 0, model_offset = 0, resource_offset = 0, config_offset = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &model_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &resource_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->position); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->scale); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_group_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 4); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &config_offset); if (rc != SF_OK) return rc;
    (void)id;
    if (type != MSBFA_PART_MAP_PIECE || name_offset == 0 || model_offset == 0 ||
        resource_offset == 0 || config_offset == 0) return SF_ERR_BAD_MAGIC;
    out->type = MSBFA_PART_MAP_PIECE;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    char *model_name = NULL;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + model_offset, &model_name, NULL);
    if (rc != SF_OK) return rc;
    sf_xfree(a, model_name);
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + resource_offset, &out->resource_path, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + config_offset); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_i32_one(r, 0);
}

sf_result_t msbfa_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbfa_t *out,
                                    const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->part_count = count;
    if (count == 0) return SF_OK;
    out->parts = (sf_msbfa_part_t *)sf_xalloc(a, (size_t)count * sizeof(*out->parts));
    if (!out->parts) return SF_ERR_OOM;
    memset(out->parts, 0, (size_t)count * sizeof(*out->parts));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->parts); out->parts = NULL; out->part_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msbfa_part_read_one(r, offsets[i], &out->parts[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msbfa_parts_param_free(out->parts, count, a); sf_xfree(a, out->parts); out->parts = NULL; out->part_count = 0; }
    return rc;
}

static sf_result_t msbfa_part_fill_rel_i32(sf_binary_writer_t *w, const char *key,
                                            int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msbfa_part_write_entry(sf_binary_writer_t *w, const void *entry,
                                           size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbfa_part_t *part = (const sf_msbfa_part_t *)entry;
    if (part->data.type != MSBFA_PART_MAP_PIECE) return SF_ERR_UNSUPPORTED_VERSION;
    char name_key[40], model_key[40], resource_key[40], config_key[40];
    snprintf(name_key, sizeof name_key, "MsbfaPartName%zu", index);
    snprintf(model_key, sizeof model_key, "MsbfaPartModel%zu", index);
    snprintf(resource_key, sizeof resource_key, "MsbfaPartRes%zu", index);
    snprintf(config_key, sizeof config_key, "MsbfaPartConfig%zu", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_u32(w, (uint32_t)part->data.type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, (int32_t)index); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, model_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, resource_key), return rc);
    rc = sf_binary_writer_write_vec3(w, part->data.position); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->data.rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->data.scale); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->data.entity_group_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->data.entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, false); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, false); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, config_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, msbfa_part_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, part->data.name ? part->data.name : "", true); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbfa_part_fill_rel_i32(w, model_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, "m00", true); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbfa_part_fill_rel_i32(w, resource_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, part->data.resource_path ? part->data.resource_path : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbfa_part_fill_rel_i32(w, config_key, start), return rc);
    return sf_binary_writer_write_i32(w, 0);
}

sf_result_t msbfa_parts_param_write(sf_binary_writer_t *w, const sf_msbfa_t *msb) {
    if (!w || !msb || msb->part_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "PARTS_PARAM_ST", "MsbfaNextList5", msb->parts,
                                       (size_t)msb->part_count, sizeof(*msb->parts),
                                       msbfa_part_write_entry, NULL);
}
