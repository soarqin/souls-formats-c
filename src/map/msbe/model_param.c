/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Elden Ring MSBE ModelParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBE/ModelParam.cs
 */

#include "msbe_internal.h"

#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBE_MODEL_TYPE_MAP_PIECE = 0,
    MSBE_MODEL_TYPE_ENEMY     = 2,
    MSBE_MODEL_TYPE_PLAYER    = 4,
    MSBE_MODEL_TYPE_COLLISION = 5,
    MSBE_MODEL_TYPE_ASSET     = 10,
};

static bool msbe_model_type_is_known(uint32_t type) {
    switch (type) {
    case MSBE_MODEL_TYPE_MAP_PIECE:
    case MSBE_MODEL_TYPE_ENEMY:
    case MSBE_MODEL_TYPE_PLAYER:
    case MSBE_MODEL_TYPE_COLLISION:
    case MSBE_MODEL_TYPE_ASSET:
        return true;
    default:
        return false;
    }
}

static bool msbe_model_has_type_data(uint32_t type) {
    return type == MSBE_MODEL_TYPE_MAP_PIECE;
}

void msbe_model_param_free(sf_msbe_model_t *models, int32_t count, const sf_allocator_t *a) {
    if (!models || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, models[i].data.name);
        sf_xfree(a, models[i].data.sib_path);
        memset(&models[i], 0, sizeof(models[i]));
    }
}

static sf_result_t msbe_model_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                       msbe_model_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0;
    int64_t sib_offset = 0;
    int64_t type_data_offset = 0;
    int32_t id = 0;

    rc = sf_binary_reader_read_i64(r, &name_offset);          if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->type);            if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id);                   if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_i64(r, &sib_offset);           if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->instance_count);  if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk1c);           if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data_offset);     if (rc != SF_OK) return rc;

    if (!msbe_model_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || sib_offset == 0) return SF_ERR_BAD_MAGIC;
    if (msbe_model_has_type_data(out->type) != (type_data_offset != 0)) return SF_ERR_BAD_MAGIC;

    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    return sf_binary_reader_get_utf16(r, entry_offset + sib_offset, &out->sib_path, NULL);
}

sf_result_t msbe_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->model_count = count;
    out->models = NULL;
    if (count == 0) return SF_OK;

    out->models = (sf_msbe_model_t *)sf_xalloc(a, (size_t)count * sizeof(*out->models));
    if (!out->models) return SF_ERR_OOM;
    memset(out->models, 0, (size_t)count * sizeof(*out->models));

    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbe_model_read_one(r, entry_offsets[i], &out->models[i].data, a);
            if (rc != SF_OK) break;
        }
    }
    sf_xfree(a, entry_offsets);
    return rc;
}

static sf_result_t msbe_model_write_one(sf_binary_writer_t *w, const msbe_model_t *model,
                                        int32_t id) {
    int64_t start = sf_binary_writer_position(w);
    char name_res[32];
    char sib_res[32];
    char type_res[32];
    snprintf(name_res, sizeof name_res, "MsbeModelName%lld", (long long)start);
    snprintf(sib_res, sizeof sib_res, "MsbeModelSib%lld", (long long)start);
    snprintf(type_res, sizeof type_res, "MsbeModelType%lld", (long long)start);

    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_res), return rc);
    rc = sf_binary_writer_write_u32(w, model->type);            if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id);                     if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, sib_res), return rc);
    rc = sf_binary_writer_write_i32(w, model->instance_count);  if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, model->unk1c);           if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, type_res), return rc);

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_res, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, model->name ? model->name : "", true);
    if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, sib_res, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, model->sib_path ? model->sib_path : "", true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    if (msbe_model_has_type_data(model->type)) {
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, type_res, sf_binary_writer_position(w) - start), return rc);
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, 0.0f); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, 0.0f); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, 0.0f); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, 0.0f); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, 0.0f); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, 0.0f);
    }
    return sf_binary_writer_fill_i64(w, type_res, 0);
}

sf_result_t msbe_model_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe) {
    if (!w || !msbe) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 73); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbe->model_count + 1); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbeNameOff0"), return rc);
    for (int32_t i = 0; i < msbe->model_count; i++) {
        char off_name[32];
        snprintf(off_name, sizeof off_name, "MsbeModelOff%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, off_name), return rc);
    }
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbeNextList0"), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, "MsbeNameOff0", sf_binary_writer_position(w)), return rc);
    rc = sf_binary_writer_write_utf16(w, "MODEL_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    for (int32_t i = 0; i < msbe->model_count; i++) {
        char off_name[32];
        snprintf(off_name, sizeof off_name, "MsbeModelOff%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, off_name, sf_binary_writer_position(w)), return rc);
        rc = msbe_model_write_one(w, &msbe->models[i].data, i);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}
