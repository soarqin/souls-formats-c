/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core VI MSBVI PointParam.
 * Mirrors SoulsFormats/Formats/MSB/MSBVI/PointParam.cs
 */

#include "msbvi_internal.h"
#include "internal/sf_internal.h"
#include "map/msb_internal.h" /* IWYU pragma: keep */

#include <stdio.h>
#include <string.h>

static bool msbvi_region_type_is_known(uint32_t type) {
    static const uint32_t known[] = { 1, 2, 4, 5, 6, 17, 18, 28, 29, 30, 32, 33, 35, 36,
        37, 39, 45, 46, 47, 49, 50, 51, 52, 53, 54, 55, 56, UINT32_MAX };
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) if (known[i] == type) return true;
    return false;
}

void msbvi_point_param_free(sf_msbvi_region_t *regions, int32_t count, const sf_allocator_t *a) {
    if (!regions || count <= 0) return;
    for (int32_t i = 0; i < count; i++) { sf_xfree(a, regions[i].data.name); memset(&regions[i], 0, sizeof(regions[i])); }
}

static sf_result_t msbvi_region_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                         msbvi_region_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out)); out->alloc = a;
    sf_result_t rc = sf_istream_seek(sf_binary_reader_stream(r), entry_offset); if (rc != SF_OK) return rc;
    int64_t name_offset = 0, parent_list = 0, child_list = 0, form = 0, common = 0, type_data = 0, struct98 = 0;
    int32_t type_index = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_index); if (rc != SF_OK) return rc;
    (void)type_index;
    rc = sf_binary_reader_read_u32(r, &out->shape_type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 28); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &parent_list); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &child_list); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &form); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &common); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &struct98); if (rc != SF_OK) return rc;
    if (!msbvi_region_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || parent_list == 0 || child_list == 0 || common == 0 || struct98 == 0) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(sf_binary_reader_stream(r), entry_offset + common); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 4); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->entity_id); if (rc != SF_OK) return rc;
    (void)form; (void)type_data;
    return SF_OK;
}

sf_result_t msbvi_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->region_count = count;
    out->regions = NULL;
    if (count == 0) return SF_OK;
    out->regions = (sf_msbvi_region_t *)sf_xalloc(a, (size_t)count * sizeof(*out->regions));
    if (!out->regions) return SF_ERR_OOM;
    memset(out->regions, 0, (size_t)count * sizeof(*out->regions));
    int64_t *offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, offsets);
    if (rc == SF_OK) for (int32_t i = 0; i < count; i++) { rc = msbvi_region_read_one(r, offsets[i], &out->regions[i].data, a); if (rc != SF_OK) break; }
    sf_xfree(a, offsets);
    return rc;
}

static sf_result_t msbvi_region_write_one(sf_binary_writer_t *w, const msbvi_region_t *region,
                                          int32_t id) {
    (void)id; int64_t start = sf_binary_writer_position(w);
    char name_res[32], parents_res[32], children_res[32], form_res[32], common_res[32], type_res[32], tail_res[32];
    snprintf(name_res, sizeof name_res, "MsbviRegionName%lld", (long long)start); snprintf(parents_res, sizeof parents_res, "MsbviRegionParents%lld", (long long)start); snprintf(children_res, sizeof children_res, "MsbviRegionChildren%lld", (long long)start); snprintf(form_res, sizeof form_res, "MsbviRegionForm%lld", (long long)start); snprintf(common_res, sizeof common_res, "MsbviRegionCommon%lld", (long long)start); snprintf(type_res, sizeof type_res, "MsbviRegionType%lld", (long long)start); snprintf(tail_res, sizeof tail_res, "MsbviRegionTail%lld", (long long)start);
    sf_result_t rc = sf_binary_writer_reserve_i64(w, name_res); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, region->type); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_u32(w, region->shape_type); if (rc != SF_OK) return rc;
    for (int i = 0; i < 7; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_reserve_i64(w, parents_res); if (rc != SF_OK) return rc; rc = sf_binary_writer_reserve_i64(w, children_res); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, form_res); if (rc != SF_OK) return rc; rc = sf_binary_writer_reserve_i64(w, common_res); if (rc != SF_OK) return rc; rc = sf_binary_writer_reserve_i64(w, type_res); if (rc != SF_OK) return rc; rc = sf_binary_writer_reserve_i64(w, tail_res); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, name_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_utf16(w, region->name ? region->name : "", true); if (rc != SF_OK) return rc; rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, parents_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, children_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, form_res, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_fill_i64(w, common_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_u32(w, region->entity_id); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i16(w, -1); if (rc != SF_OK) return rc; for (int i = 0; i < 7; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_fill_i64(w, type_res, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_fill_i64(w, tail_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; return sf_binary_writer_write_i32(w, -1);
}

static sf_result_t msbvi_region_write_entry(sf_binary_writer_t *w,
                                            const void         *entry,
                                            size_t              index,
                                            void               *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbvi_region_t *region = (const sf_msbvi_region_t *)entry;
    return msbvi_region_write_one(w, &region->data, (int32_t)index);
}

sf_result_t msbvi_point_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi) {
    if (!w || !msbvi) return SF_ERR_INVALID_ARG;
    return msb_entry_list_write(w, 52, "POINT_PARAM_ST", "MsbviNextList2", msbvi->regions,
                                (size_t)msbvi->region_count, sizeof(*msbvi->regions),
                                msbvi_region_write_entry, NULL);
}
