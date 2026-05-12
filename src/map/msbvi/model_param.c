/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core VI MSBVI ModelParam.
 * Mirrors SoulsFormats/Formats/MSB/MSBVI/ModelParam.cs
 */

#include "msbvi_internal.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbvi_model_type_is_known(uint32_t type) {
    return type == 0 || type == 2 || type == 4 || type == 5 || type == 10;
}

void msbvi_model_param_free(sf_msbvi_model_t *models, int32_t count, const sf_allocator_t *a) {
    if (!models || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, models[i].data.name);
        sf_xfree(a, models[i].data.sib_path);
        memset(&models[i], 0, sizeof(models[i]));
    }
}

static sf_result_t msbvi_model_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                        msbvi_model_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0, source_offset = 0;
    int32_t type_index = 0, zero = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset);         if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->type);           if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_index);          if (rc != SF_OK) return rc;
    (void)type_index;
    rc = sf_binary_reader_read_i64(r, &source_offset);       if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->instance_count); if (rc != SF_OK) return rc;
    for (int i = 0; i < 3; i++) {
        rc = sf_binary_reader_read_i32(r, &zero); if (rc != SF_OK) return rc;
        if (zero != 0) return SF_ERR_BAD_MAGIC;
    }
    if (!msbvi_model_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || source_offset == 0) return SF_ERR_BAD_MAGIC;

    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    return sf_binary_reader_get_utf16(r, entry_offset + source_offset, &out->sib_path, NULL);
}

sf_result_t msbvi_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->model_count = count;
    out->models = NULL;
    if (count == 0) return SF_OK;
    out->models = (sf_msbvi_model_t *)sf_xalloc(a, (size_t)count * sizeof(*out->models));
    if (!out->models) return SF_ERR_OOM;
    memset(out->models, 0, (size_t)count * sizeof(*out->models));
    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbvi_model_read_one(r, entry_offsets[i], &out->models[i].data, a);
            if (rc != SF_OK) break;
        }
    }
    sf_xfree(a, entry_offsets);
    return rc;
}

static sf_result_t msbvi_model_write_one(sf_binary_writer_t *w, const msbvi_model_t *model,
                                         int32_t id) {
    int64_t start = sf_binary_writer_position(w);
    char name_res[32], source_res[32];
    snprintf(name_res, sizeof name_res, "MsbviModelName%lld", (long long)start);
    snprintf(source_res, sizeof source_res, "MsbviModelSource%lld", (long long)start);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_res), return rc);
    rc = sf_binary_writer_write_u32(w, model->type);           if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id);                    if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, source_res), return rc);
    rc = sf_binary_writer_write_i32(w, model->instance_count); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0);                     if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0);                     if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0);                     if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_res, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, model->name ? model->name : "", true);
    if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, source_res, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, model->sib_path ? model->sib_path : "", true);
    if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

sf_result_t msbvi_model_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi) {
    if (!w || !msbvi) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 52); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbvi->model_count + 1); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbviNameOff0"), return rc);
    for (int32_t i = 0; i < msbvi->model_count; i++) {
        char off_name[32];
        snprintf(off_name, sizeof off_name, "MsbviModelOff%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, off_name), return rc);
    }
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbviNextList0"), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, "MsbviNameOff0", sf_binary_writer_position(w)), return rc);
    rc = sf_binary_writer_write_utf16(w, "MODEL_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbvi->model_count; i++) {
        char off_name[32];
        snprintf(off_name, sizeof off_name, "MsbviModelOff%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, off_name, sf_binary_writer_position(w)), return rc);
        rc = msbvi_model_write_one(w, &msbvi->models[i].data, i);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}
