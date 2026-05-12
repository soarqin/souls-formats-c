/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msb2_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

void msb2_point_param_free(sf_msb2_region_t *regions, int32_t count, const sf_allocator_t *a) {
    if (!regions || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, regions[i].data.name);
        memset(&regions[i], 0, sizeof(regions[i]));
    }
}

static sf_result_t msb2_region_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                        msb2_region_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0, id = 0, base1 = 0, base2 = 0, shape_offset = 0;
    int32_t type_offset = 0, unk2c = 0;
    uint32_t type = 0, shape = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &shape); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->position); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &unk2c); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &base1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &base2); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &shape_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_offset); if (rc != SF_OK) return rc;
    (void)id; (void)unk2c;
    if (type != MSB2_REGION_START_POINT || shape != MSB2_REGION_SHAPE_POINT ||
        name_offset == 0 || base1 == 0 || base2 == 0 || shape_offset != 0 ||
        type_offset == 0) {
        return SF_ERR_BAD_MAGIC;
    }
    out->type = MSB2_REGION_START_POINT;
    out->shape_type = MSB2_REGION_SHAPE_POINT;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + base1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + base2); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_id); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + type_offset); if (rc != SF_OK) return rc;
    return sf_binary_reader_read_i32(r, &out->type_value0);
}

sf_result_t msb2_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->region_count = count;
    if (count == 0) return SF_OK;
    out->regions = (sf_msb2_region_t *)sf_xalloc(a, (size_t)count * sizeof(*out->regions));
    if (!out->regions) return SF_ERR_OOM;
    memset(out->regions, 0, (size_t)count * sizeof(*out->regions));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->regions); out->regions = NULL; out->region_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msb2_region_read_one(r, offsets[i], &out->regions[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msb2_point_param_free(out->regions, count, a); sf_xfree(a, out->regions); out->regions = NULL; out->region_count = 0; }
    return rc;
}

static sf_result_t msb2_region_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msb2_region_write_entry(sf_binary_writer_t *w, const void *entry,
                                           size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msb2_region_t *region = (const sf_msb2_region_t *)entry;
    if (region->data.type != MSB2_REGION_START_POINT ||
        region->data.shape_type != MSB2_REGION_SHAPE_POINT) return SF_ERR_UNSUPPORTED_VERSION;
    char name_key[32], base1_key[32], base2_key[32], shape_key[32], type_key[32];
    snprintf(name_key, sizeof name_key, "Msb2RegionName%zu", index);
    snprintf(base1_key, sizeof base1_key, "Msb2RegionBase1%zu", index);
    snprintf(base2_key, sizeof base2_key, "Msb2RegionBase2%zu", index);
    snprintf(shape_key, sizeof shape_key, "Msb2RegionShape%zu", index);
    snprintf(type_key, sizeof type_key, "Msb2RegionType%zu", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_u32(w, (uint32_t)region->data.type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, (int32_t)index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)region->data.shape_type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, region->data.position); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, region->data.rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, base1_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, base2_key), return rc);
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, shape_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, type_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, msb2_region_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, region->data.name ? region->data.name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_region_fill_rel_i32(w, base1_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_region_fill_rel_i32(w, base2_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, region->data.entity_id); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i32(w, shape_key, 0), return rc);
    SF_RESERVE_FILL_PAIR(rc, msb2_region_fill_rel_i32(w, type_key, start), return rc);
    return sf_binary_writer_write_i32(w, region->data.type_value0);
}

sf_result_t msb2_point_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2) {
    if (!w || !msb2 || msb2->region_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "POINT_PARAM_ST", "Msb2NextList2", msb2->regions,
                                       (size_t)msb2->region_count, sizeof(*msb2->regions),
                                       msb2_region_write_entry, NULL);
}
