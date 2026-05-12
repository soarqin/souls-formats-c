/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MSBD legacy MSB PartsParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBD/PartsParam.cs
 */

#include "msbd_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbd_part_type_is_known(uint32_t type) {
    switch (type) {
    case MSBD_PART_MAP_PIECE:
    case MSBD_PART_OBJECT:
    case MSBD_PART_ENEMY:
    case MSBD_PART_PLAYER:
    case MSBD_PART_COLLISION:
    case MSBD_PART_NAVMESH:
    case MSBD_PART_DUMMY_OBJECT:
    case MSBD_PART_DUMMY_ENEMY:
    case MSBD_PART_CONNECT_COLLISION:
        return true;
    default:
        return false;
    }
}

void msbd_parts_param_free(sf_msbd_part_t *parts, int32_t count, const sf_allocator_t *a) {
    if (!parts || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, parts[i].data.name);
        sf_xfree(a, parts[i].data.sib_path);
        memset(&parts[i], 0, sizeof(parts[i]));
    }
}

static sf_result_t msbd_part_read_entity_data(sf_binary_reader_t *r, msbd_part_t *part) {
    sf_result_t rc;
    rc = sf_binary_reader_read_i32(r, &part->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->light_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->fog_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->scatter_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->lens_flare_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->shadow_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->dof_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->tone_map_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->tone_correct_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->lantern_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->lod_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_u8_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->is_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->is_shadow_dest); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->is_shadow_only); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->draw_by_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->draw_only_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->use_depth_bias_float); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->disable_point_light_effect); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_u8_one(r, 0); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_u8_one(r, 0);
}

static sf_result_t msbd_part_write_entity_data(sf_binary_writer_t *w, const msbd_part_t *part) {
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, part->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->light_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->fog_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->scatter_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->lens_flare_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->shadow_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->dof_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->tone_map_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->tone_correct_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->lantern_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->lod_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->is_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->is_shadow_dest); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->is_shadow_only); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->draw_by_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->draw_only_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->use_depth_bias_float); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->disable_point_light_effect); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
    return sf_binary_writer_write_u8(w, 0);
}

static sf_result_t msbd_part_read_type_data(sf_binary_reader_t *r, msbd_part_t *part) {
    sf_result_t rc;
    switch (part->type) {
    case MSBD_PART_MAP_PIECE:
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBD_PART_PLAYER:
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBD_PART_OBJECT:
    case MSBD_PART_ENEMY:
    case MSBD_PART_COLLISION:
    case MSBD_PART_NAVMESH:
    case MSBD_PART_DUMMY_OBJECT:
    case MSBD_PART_DUMMY_ENEMY:
    case MSBD_PART_CONNECT_COLLISION:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbd_part_write_type_data(sf_binary_writer_t *w, const msbd_part_t *part) {
    sf_result_t rc;
    switch (part->type) {
    case MSBD_PART_MAP_PIECE:
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBD_PART_PLAYER:
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBD_PART_OBJECT:
    case MSBD_PART_ENEMY:
    case MSBD_PART_COLLISION:
    case MSBD_PART_NAVMESH:
    case MSBD_PART_DUMMY_OBJECT:
    case MSBD_PART_DUMMY_ENEMY:
    case MSBD_PART_CONNECT_COLLISION:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbd_part_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                      msbd_part_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    out->scale = (sf_vec3_t){ 1.0f, 1.0f, 1.0f };

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int32_t name_offset = 0, id = 0, sib_offset = 0, entity_data_offset = 0, type_data_offset = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    if (!msbd_part_type_is_known(type)) return SF_ERR_UNSUPPORTED_VERSION;
    out->type = (msbd_part_type_t)type;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_i32(r, &out->model_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &sib_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->position); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->scale); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32s(r, 4, out->draw_groups); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32s(r, 4, out->disp_groups); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &entity_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;

    if (name_offset == 0 || sib_offset == 0 || entity_data_offset == 0 || type_data_offset == 0) {
        return SF_ERR_BAD_MAGIC;
    }

    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + sib_offset, &out->sib_path, NULL);
    if (rc != SF_OK) return rc;

    rc = sf_istream_seek(stream, (int64_t)entry_offset + entity_data_offset); if (rc != SF_OK) return rc;
    rc = msbd_part_read_entity_data(r, out); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + type_data_offset); if (rc != SF_OK) return rc;
    return msbd_part_read_type_data(r, out);
}

sf_result_t msbd_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbd_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->part_count = count;
    out->parts = NULL;
    if (count == 0) return SF_OK;

    size_t bytes = (size_t)count * sizeof(*out->parts);
    out->parts = (sf_msbd_part_t *)sf_xalloc(a, bytes);
    if (!out->parts) return SF_ERR_OOM;
    memset(out->parts, 0, bytes);

    int32_t *entry_offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) {
        sf_xfree(a, out->parts);
        out->parts = NULL;
        out->part_count = 0;
        return SF_ERR_OOM;
    }

    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbd_part_read_one(r, entry_offsets[i], &out->parts[i].data, a);
            if (rc != SF_OK) break;
        }
    }

    sf_xfree(a, entry_offsets);
    if (rc != SF_OK) {
        msbd_parts_param_free(out->parts, count, a);
        sf_xfree(a, out->parts);
        out->parts = NULL;
        out->part_count = 0;
    }
    return rc;
}

static sf_result_t msbd_part_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msbd_part_write_one(sf_binary_writer_t *w, const msbd_part_t *part,
                                       int32_t id, int32_t index) {
    if (!msbd_part_type_is_known((uint32_t)part->type)) return SF_ERR_UNSUPPORTED_VERSION;

    char name_key[32], sib_key[32], entity_key[32], type_key[32];
    snprintf(name_key, sizeof name_key, "MsbdPartName%d", index);
    snprintf(sib_key, sizeof sib_key, "MsbdPartSib%d", index);
    snprintf(entity_key, sizeof entity_key, "MsbdPartEntity%d", index);
    snprintf(type_key, sizeof type_key, "MsbdPartType%d", index);

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_u32(w, (uint32_t)part->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->model_index); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, sib_key), return rc);
    rc = sf_binary_writer_write_vec3(w, part->position); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->scale); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32s(w, 4, part->draw_groups); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32s(w, 4, part->disp_groups); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, entity_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, type_key), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;

    int64_t strings_start = sf_binary_writer_position(w);
    SF_RESERVE_FILL_PAIR(rc, msbd_part_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, part->name ? part->name : "", true); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbd_part_fill_rel_i32(w, sib_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, part->sib_path ? part->sib_path : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    int64_t string_bytes = sf_binary_writer_position(w) - strings_start;
    if (string_bytes < 0x14) {
        rc = sf_binary_writer_write_pattern(w, (size_t)(0x14 - string_bytes), 0x00);
        if (rc != SF_OK) return rc;
    }

    SF_RESERVE_FILL_PAIR(rc, msbd_part_fill_rel_i32(w, entity_key, start), return rc);
    rc = msbd_part_write_entity_data(w, part); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbd_part_fill_rel_i32(w, type_key, start), return rc);
    return msbd_part_write_type_data(w, part);
}

static sf_result_t msbd_part_write_entry(sf_binary_writer_t *w, const void *entry,
                                         size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbd_part_t *part = (const sf_msbd_part_t *)entry;
    return msbd_part_write_one(w, &part->data, (int32_t)index, (int32_t)index);
}

sf_result_t msbd_parts_param_write(sf_binary_writer_t *w, const sf_msbd_t *msbd) {
    if (!w || !msbd || msbd->part_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "PARTS_PARAM_ST", "MsbdNextList3", msbd->parts,
                                       (size_t)msbd->part_count, sizeof(*msbd->parts),
                                       msbd_part_write_entry, NULL);
}
