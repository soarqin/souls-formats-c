/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MSBB legacy MSB PointParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBB/PointParam.cs
 */

#include "msbb_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbb_region_shape_type_is_known(uint32_t type) {
    return type <= MSBB_REGION_SHAPE_BOX;
}

static bool msbb_region_shape_has_data(msbb_region_shape_type_t type) {
    return type != MSBB_REGION_SHAPE_POINT;
}

void msbb_point_param_free(sf_msbb_region_t *regions, int32_t count, const sf_allocator_t *a) {
    if (!regions || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, regions[i].data.name);
        memset(&regions[i], 0, sizeof(regions[i]));
    }
}

static sf_result_t msbb_region_read_shape_data(sf_binary_reader_t *r, msbb_region_t *region) {
    sf_result_t rc;
    switch (region->shape_type) {
    case MSBB_REGION_SHAPE_CIRCLE:
        return sf_binary_reader_read_f32(r, &region->shape.circle.radius);
    case MSBB_REGION_SHAPE_SPHERE:
        return sf_binary_reader_read_f32(r, &region->shape.sphere.radius);
    case MSBB_REGION_SHAPE_CYLINDER:
        rc = sf_binary_reader_read_f32(r, &region->shape.cylinder.radius); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_f32(r, &region->shape.cylinder.height);
    case MSBB_REGION_SHAPE_RECTANGLE:
        rc = sf_binary_reader_read_f32(r, &region->shape.rectangle.width); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_f32(r, &region->shape.rectangle.depth);
    case MSBB_REGION_SHAPE_BOX:
        rc = sf_binary_reader_read_f32(r, &region->shape.box.width); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->shape.box.depth); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_f32(r, &region->shape.box.height);
    case MSBB_REGION_SHAPE_POINT:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbb_region_write_shape_data(sf_binary_writer_t *w, const msbb_region_t *region) {
    sf_result_t rc;
    switch (region->shape_type) {
    case MSBB_REGION_SHAPE_CIRCLE:
        return sf_binary_writer_write_f32(w, region->shape.circle.radius);
    case MSBB_REGION_SHAPE_SPHERE:
        return sf_binary_writer_write_f32(w, region->shape.sphere.radius);
    case MSBB_REGION_SHAPE_CYLINDER:
        rc = sf_binary_writer_write_f32(w, region->shape.cylinder.radius); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, region->shape.cylinder.height);
    case MSBB_REGION_SHAPE_RECTANGLE:
        rc = sf_binary_writer_write_f32(w, region->shape.rectangle.width); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, region->shape.rectangle.depth);
    case MSBB_REGION_SHAPE_BOX:
        rc = sf_binary_writer_write_f32(w, region->shape.box.width); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->shape.box.depth); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, region->shape.box.height);
    case MSBB_REGION_SHAPE_POINT:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbb_region_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                        msbb_region_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    out->type = MSBB_REGION_LOGIC;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int32_t name_offset = 0, id = 0, unk_offset_a = 0, unk_offset_b = 0;
    int32_t shape_data_offset = 0, entity_data_offset = 0;
    uint32_t shape_type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_u32(r, &shape_type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->position); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &unk_offset_a); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &unk_offset_b); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &shape_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &entity_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;

    if (!msbb_region_shape_type_is_known(shape_type)) return SF_ERR_UNSUPPORTED_VERSION;
    out->shape_type = (msbb_region_shape_type_t)shape_type;
    if (name_offset == 0 || unk_offset_a == 0 || unk_offset_b == 0 || entity_data_offset == 0) {
        return SF_ERR_BAD_MAGIC;
    }
    if (msbb_region_shape_has_data(out->shape_type) != (shape_data_offset != 0)) {
        return SF_ERR_BAD_MAGIC;
    }

    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + unk_offset_a); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + unk_offset_b); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;

    if (shape_data_offset != 0) {
        rc = sf_istream_seek(stream, (int64_t)entry_offset + shape_data_offset); if (rc != SF_OK) return rc;
        rc = msbb_region_read_shape_data(r, out); if (rc != SF_OK) return rc;
    }

    rc = sf_istream_seek(stream, (int64_t)entry_offset + entity_data_offset); if (rc != SF_OK) return rc;
    return sf_binary_reader_read_i32(r, &out->entity_id);
}

sf_result_t msbb_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbb_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->region_count = count;
    out->regions = NULL;
    if (count == 0) return SF_OK;

    size_t bytes = (size_t)count * sizeof(*out->regions);
    out->regions = (sf_msbb_region_t *)sf_xalloc(a, bytes);
    if (!out->regions) return SF_ERR_OOM;
    memset(out->regions, 0, bytes);

    int32_t *entry_offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) {
        sf_xfree(a, out->regions);
        out->regions = NULL;
        out->region_count = 0;
        return SF_ERR_OOM;
    }

    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbb_region_read_one(r, entry_offsets[i], &out->regions[i].data, a);
            if (rc != SF_OK) break;
        }
    }

    sf_xfree(a, entry_offsets);
    if (rc != SF_OK) {
        msbb_point_param_free(out->regions, count, a);
        sf_xfree(a, out->regions);
        out->regions = NULL;
        out->region_count = 0;
    }
    return rc;
}

static sf_result_t msbb_region_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msbb_region_write_one(sf_binary_writer_t *w, const msbb_region_t *region,
                                         int32_t id, int32_t index) {
    if (!msbb_region_shape_type_is_known((uint32_t)region->shape_type)) return SF_ERR_UNSUPPORTED_VERSION;

    char name_key[32], unk_a_key[32], unk_b_key[32], shape_key[32], entity_key[32];
    snprintf(name_key, sizeof name_key, "MsbbRegionName%d", index);
    snprintf(unk_a_key, sizeof unk_a_key, "MsbbRegionUnkA%d", index);
    snprintf(unk_b_key, sizeof unk_b_key, "MsbbRegionUnkB%d", index);
    snprintf(shape_key, sizeof shape_key, "MsbbRegionShape%d", index);
    snprintf(entity_key, sizeof entity_key, "MsbbRegionEntity%d", index);

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)region->shape_type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, region->position); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, region->rotation); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, unk_a_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, unk_b_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, shape_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, entity_key), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;

    SF_RESERVE_FILL_PAIR(rc, msbb_region_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, region->name ? region->name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;

    SF_RESERVE_FILL_PAIR(rc, msbb_region_fill_rel_i32(w, unk_a_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbb_region_fill_rel_i32(w, unk_b_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;

    if (msbb_region_shape_has_data(region->shape_type)) {
        SF_RESERVE_FILL_PAIR(rc, msbb_region_fill_rel_i32(w, shape_key, start), return rc);
        rc = msbb_region_write_shape_data(w, region); if (rc != SF_OK) return rc;
    } else {
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i32(w, shape_key, 0), return rc);
    }

    SF_RESERVE_FILL_PAIR(rc, msbb_region_fill_rel_i32(w, entity_key, start), return rc);
    return sf_binary_writer_write_i32(w, region->entity_id);
}

static sf_result_t msbb_region_write_entry(sf_binary_writer_t *w, const void *entry,
                                           size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbb_region_t *region = (const sf_msbb_region_t *)entry;
    return msbb_region_write_one(w, &region->data, (int32_t)index, (int32_t)index);
}

sf_result_t msbb_point_param_write(sf_binary_writer_t *w, const sf_msbb_t *msbb) {
    if (!w || !msbb || msbb->region_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "POINT_PARAM_ST", "MsbbNextList2", msbb->regions,
                                       (size_t)msbb->region_count, sizeof(*msbb->regions),
                                       msbb_region_write_entry, NULL);
}
