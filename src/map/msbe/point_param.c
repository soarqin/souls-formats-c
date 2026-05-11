/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Elden Ring MSBE PointParam.
 * Mirrors SoulsFormats/Formats/MSB/MSBE/PointParam.cs
 */

#include "msbe_internal.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbe_region_type_is_known(uint32_t type) {
    static const uint32_t known[] = { 1, 2, 4, 5, 6, 8, 9, 17, 18, 21, 22, 26, 27, 28,
        29, 30, 31, 32, 33, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 46, 48, 49, 50,
        51, 52, 53, 54, 55, UINT32_MAX };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (known[i] == type) return true;
    }
    return false;
}

static bool msbe_region_has_type_data(uint32_t type) {
    return type != 18 && type != 31 && type != 38 && type != 39 && type != 44 &&
           type != UINT32_MAX;
}

void msbe_point_param_free(sf_msbe_region_t *regions, int32_t count, const sf_allocator_t *a) {
    if (!regions || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, regions[i].data.name);
        memset(&regions[i], 0, sizeof(regions[i]));
    }
}

static sf_result_t msbe_region_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                        msbe_region_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0, base1 = 0, base2 = 0, shape = 0, base3 = 0, type_data = 0, unk4 = 0;
    int32_t id = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_u32(r, &out->shape_type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 24); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->region_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base2); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &shape); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base3); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &unk4); if (rc != SF_OK) return rc;

    if (!msbe_region_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || base1 == 0 || base2 == 0 || base3 == 0 || unk4 == 0) return SF_ERR_BAD_MAGIC;
    if (shape != 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (msbe_region_has_type_data(out->type) != (type_data != 0)) return SF_ERR_BAD_MAGIC;
    return sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
}

sf_result_t msbe_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->region_count = count;
    out->regions = NULL;
    if (count == 0) return SF_OK;
    out->regions = (sf_msbe_region_t *)sf_xalloc(a, (size_t)count * sizeof(*out->regions));
    if (!out->regions) return SF_ERR_OOM;
    memset(out->regions, 0, (size_t)count * sizeof(*out->regions));
    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbe_region_read_one(r, entry_offsets[i], &out->regions[i].data, a);
            if (rc != SF_OK) break;
        }
    }
    sf_xfree(a, entry_offsets);
    return rc;
}

static sf_result_t msbe_region_write_one(sf_binary_writer_t *w, const msbe_region_t *region,
                                         int32_t id, int32_t index) {
    char name_key[32], base1_key[32], base2_key[32], shape_key[32], base3_key[32], type_key[32], unk_key[32];
    snprintf(name_key, sizeof name_key, "MsbeRegionName%d", index);
    snprintf(base1_key, sizeof base1_key, "MsbeRegionBase1%d", index);
    snprintf(base2_key, sizeof base2_key, "MsbeRegionBase2%d", index);
    snprintf(shape_key, sizeof shape_key, "MsbeRegionShape%d", index);
    snprintf(base3_key, sizeof base3_key, "MsbeRegionBase3%d", index);
    snprintf(type_key, sizeof type_key, "MsbeRegionType%d", index);
    snprintf(unk_key, sizeof unk_key, "MsbeRegionUnk%d", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    rc = sf_binary_writer_reserve_i64(w, name_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, region->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, region->shape_type); if (rc != SF_OK) return rc;
    for (int i = 0; i < 6; i++) { rc = sf_binary_writer_write_f32(w, 0.0f); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_write_i32(w, region->region_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, base1_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, base2_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, UINT32_MAX); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, shape_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, base3_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, type_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, unk_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, name_key, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, region->name ? region->name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, base1_key, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, base2_key, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, shape_key, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, base3_key, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    if (region->type > 26 && region->type != UINT32_MAX) { rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc; }
    if (msbe_region_has_type_data(region->type)) {
        rc = sf_binary_writer_fill_i64(w, type_key, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
        for (int i = 0; i < 8; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    } else {
        rc = sf_binary_writer_fill_i64(w, type_key, 0); if (rc != SF_OK) return rc;
    }
    if (region->type <= 26 || region->type == UINT32_MAX) { rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_fill_i64(w, unk_key, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
    for (int i = 0; i < 8; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    return sf_binary_writer_pad(w, 8);
}

sf_result_t msbe_point_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe) {
    if (!w || !msbe) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 73); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbe->region_count + 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbeNameOff2"); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbe->region_count; i++) {
        char entry_key[32]; snprintf(entry_key, sizeof entry_key, "MsbeRegionEntry%d", i);
        rc = sf_binary_writer_reserve_i64(w, entry_key); if (rc != SF_OK) return rc;
    }
    rc = sf_binary_writer_reserve_i64(w, "MsbeNextList2"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "MsbeNameOff2", sf_binary_writer_position(w)); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, "POINT_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbe->region_count; i++) {
        char entry_key[32]; snprintf(entry_key, sizeof entry_key, "MsbeRegionEntry%d", i);
        rc = sf_binary_writer_fill_i64(w, entry_key, sf_binary_writer_position(w)); if (rc != SF_OK) return rc;
        rc = msbe_region_write_one(w, &msbe->regions[i].data, i, i); if (rc != SF_OK) return rc;
    }
    return SF_OK;
}
