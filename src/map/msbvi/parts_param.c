/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core VI MSBVI PartsParam.
 * Mirrors SoulsFormats/Formats/MSB/MSBVI/PartsParam.cs
 */

#include "msbvi_internal.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbvi_part_type_is_known(uint32_t type) {
    return type == 0 || type == 1 || type == 2 || type == 3 || type == 4 || type == 5 ||
           type == 6 || type == 7 || type == 8 || type == 9 || type == 10 || type == 11 ||
           type == 12 || type == 13;
}

void msbvi_parts_param_free(sf_msbvi_part_t *parts, int32_t count, const sf_allocator_t *a) {
    if (!parts || count <= 0) return;
    for (int32_t i = 0; i < count; i++) { sf_xfree(a, parts[i].data.name); sf_xfree(a, parts[i].data.layout_path); memset(&parts[i], 0, sizeof(parts[i])); }
}

static sf_result_t msbvi_part_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                       msbvi_part_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out)); out->alloc = a;
    sf_result_t rc = sf_istream_seek(sf_binary_reader_stream(r), entry_offset); if (rc != SF_OK) return rc;
    int64_t name_offset = 0, source_offset = 0, common_offset = 0, type_offset = 0;
    int32_t type_index = 0, zero = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->model_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &zero);
    if (rc != SF_OK) return rc;
    if (zero != 0) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_read_i64(r, &source_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 36); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_skip(r, 12); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &zero);
    if (rc != SF_OK) return rc;
    if (zero != -1) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_read_i32(r, &zero);
    if (rc != SF_OK) return rc;
    if (zero != 1) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_skip(r, 16); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &common_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_offset); if (rc != SF_OK) return rc;
    if (!msbvi_part_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || source_offset == 0 || common_offset == 0) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_get_utf16(r, entry_offset + source_offset, &out->layout_path, NULL); if (rc != SF_OK) return rc;
    (void)type_index; (void)type_offset;
    return SF_OK;
}

sf_result_t msbvi_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->part_count = count;
    out->parts = NULL;
    if (count == 0) return SF_OK;
    out->parts = (sf_msbvi_part_t *)sf_xalloc(a, (size_t)count * sizeof(*out->parts));
    if (!out->parts) return SF_ERR_OOM;
    memset(out->parts, 0, (size_t)count * sizeof(*out->parts));
    int64_t *offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, offsets);
    if (rc == SF_OK) for (int32_t i = 0; i < count; i++) { rc = msbvi_part_read_one(r, offsets[i], &out->parts[i].data, a); if (rc != SF_OK) break; }
    sf_xfree(a, offsets);
    return rc;
}

static sf_result_t msbvi_part_write_one(sf_binary_writer_t *w, const msbvi_part_t *part,
                                        int32_t id) {
    int64_t start = sf_binary_writer_position(w); char name_res[32], source_res[32], common_res[32], type_res[32];
    snprintf(name_res, sizeof name_res, "MsbviPartName%lld", (long long)start); snprintf(source_res, sizeof source_res, "MsbviPartSource%lld", (long long)start); snprintf(common_res, sizeof common_res, "MsbviPartCommon%lld", (long long)start); snprintf(type_res, sizeof type_res, "MsbviPartType%lld", (long long)start);
    sf_result_t rc = sf_binary_writer_reserve_i64(w, name_res); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, part->type); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, part->model_index); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, source_res); if (rc != SF_OK) return rc; for (int i = 0; i < 9; i++) { rc = sf_binary_writer_write_i32(w, i < 6 ? 0 : (i == 6 ? -1 : (i == 7 ? 1 : 0))); if (rc != SF_OK) return rc; }
    for (int i = 0; i < 2; i++) { rc = sf_binary_writer_reserve_i64(w, i == 0 ? "MsbviPartZeroA" : "MsbviPartZeroB"); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_reserve_i64(w, common_res); if (rc != SF_OK) return rc; rc = sf_binary_writer_reserve_i64(w, type_res); if (rc != SF_OK) return rc;
    for (int i = 0; i < 10; i++) { rc = sf_binary_writer_write_i64(w, 0); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_fill_i64(w, name_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_utf16(w, part->name ? part->name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, source_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_utf16(w, part->layout_path ? part->layout_path : "", true); if (rc != SF_OK) return rc; rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, common_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc; for (int i = 0; i < 40; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_fill_i64(w, type_res, 0); if (rc != SF_OK) return rc; rc = sf_binary_writer_fill_i64(w, "MsbviPartZeroA", 0); if (rc != SF_OK) return rc; return sf_binary_writer_fill_i64(w, "MsbviPartZeroB", 0);
}

sf_result_t msbvi_parts_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi) {
    if (!w || !msbvi) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 52); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbvi->part_count + 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbviNameOff5"); if (rc != SF_OK) return rc; for (int32_t i = 0; i < msbvi->part_count; i++) { char n[32]; snprintf(n, sizeof n, "MsbviPartOff%d", i); rc = sf_binary_writer_reserve_i64(w, n); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_reserve_i64(w, "MsbviNextList5"); if (rc != SF_OK) return rc; rc = sf_binary_writer_fill_i64(w, "MsbviNameOff5", sf_binary_writer_position(w)); if (rc != SF_OK) return rc; rc = sf_binary_writer_write_utf16(w, "PARTS_PARAM_ST", true); if (rc != SF_OK) return rc; rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbvi->part_count; i++) { char n[32]; snprintf(n, sizeof n, "MsbviPartOff%d", i); rc = sf_binary_writer_fill_i64(w, n, sf_binary_writer_position(w)); if (rc != SF_OK) return rc; rc = msbvi_part_write_one(w, &msbvi->parts[i].data, i); if (rc != SF_OK) return rc; }
    return SF_OK;
}
