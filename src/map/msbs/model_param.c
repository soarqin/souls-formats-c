/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS ModelParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBS/ModelParam.cs
 */

#include "msbs_internal.h"

#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBS_MODEL_TYPE_MAP_PIECE = 0,
    MSBS_MODEL_TYPE_OBJECT    = 1,
    MSBS_MODEL_TYPE_ENEMY     = 2,
    MSBS_MODEL_TYPE_PLAYER    = 4,
    MSBS_MODEL_TYPE_COLLISION = 5,
};

static sf_result_t msbs_model_kind_from_type(uint32_t type, sf_msb_model_kind_t *out) {
    if (!out) return SF_ERR_INVALID_ARG;
    switch (type) {
    case MSBS_MODEL_TYPE_MAP_PIECE: *out = SF_MSB_MODEL_MAP_PIECE; return SF_OK;
    case MSBS_MODEL_TYPE_OBJECT:    *out = SF_MSB_MODEL_OBJECT;    return SF_OK;
    case MSBS_MODEL_TYPE_ENEMY:     *out = SF_MSB_MODEL_CHARACTER; return SF_OK;
    case MSBS_MODEL_TYPE_PLAYER:    *out = SF_MSB_MODEL_PLAYER;    return SF_OK;
    case MSBS_MODEL_TYPE_COLLISION: *out = SF_MSB_MODEL_COLLISION; return SF_OK;
    default: return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_model_type_from_kind(sf_msb_model_kind_t kind, uint32_t *out) {
    if (!out) return SF_ERR_INVALID_ARG;
    switch (kind) {
    case SF_MSB_MODEL_MAP_PIECE: *out = MSBS_MODEL_TYPE_MAP_PIECE; return SF_OK;
    case SF_MSB_MODEL_OBJECT:    *out = MSBS_MODEL_TYPE_OBJECT;    return SF_OK;
    case SF_MSB_MODEL_CHARACTER: *out = MSBS_MODEL_TYPE_ENEMY;     return SF_OK;
    case SF_MSB_MODEL_PLAYER:    *out = MSBS_MODEL_TYPE_PLAYER;    return SF_OK;
    case SF_MSB_MODEL_COLLISION: *out = MSBS_MODEL_TYPE_COLLISION; return SF_OK;
    default: return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static bool msbs_model_has_type_data(sf_msb_model_kind_t kind) {
    return kind == SF_MSB_MODEL_MAP_PIECE;
}

void msbs_model_param_free(sf_msbs_model_t *models, int32_t count, const sf_allocator_t *a) {
    if (!models || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, models[i].data.name);
        sf_xfree(a, models[i].data.sib_path);
        memset(&models[i], 0, sizeof(models[i]));
    }
}

static sf_result_t msbs_model_read_type_data(sf_binary_reader_t *r, msbs_model_t *model) {
    if (model->kind != SF_MSB_MODEL_MAP_PIECE) return SF_ERR_UNSUPPORTED_VERSION;

    sf_result_t rc;
    rc = sf_binary_reader_read_bool(r, &model->u.map_piece.unk_t00); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &model->u.map_piece.unk_t01); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &model->u.map_piece.unk_t02); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_u8_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_f32(r, &model->u.map_piece.unk_t04); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_f32(r, &model->u.map_piece.unk_t08); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_f32(r, &model->u.map_piece.unk_t0c); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_f32(r, &model->u.map_piece.unk_t10); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_f32(r, &model->u.map_piece.unk_t14); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_f32(r, &model->u.map_piece.unk_t18); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_i32_one(r, 0);
}

static sf_result_t msbs_model_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                       msbs_model_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0;
    uint32_t type = 0;
    int32_t id = 0;
    int64_t sib_offset = 0;
    int64_t type_data_offset = 0;

    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_i64(r, &sib_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->instance_count); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk1c); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data_offset); if (rc != SF_OK) return rc;

    rc = msbs_model_kind_from_type(type, &out->kind);
    if (rc != SF_OK) return rc;
    if (name_offset == 0 || sib_offset == 0) return SF_ERR_BAD_MAGIC;
    if (msbs_model_has_type_data(out->kind) != (type_data_offset != 0)) return SF_ERR_BAD_MAGIC;

    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_get_utf16(r, entry_offset + sib_offset, &out->sib_path, NULL);
    if (rc != SF_OK) return rc;

    if (type_data_offset != 0) {
        rc = sf_istream_seek(stream, entry_offset + type_data_offset);
        if (rc != SF_OK) return rc;
        rc = msbs_model_read_type_data(r, out);
        if (rc != SF_OK) return rc;
    }

    return SF_OK;
}

sf_result_t msbs_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;

    out->model_count = count;
    out->models = NULL;
    if (count == 0) return SF_OK;

    size_t bytes = (size_t)count * sizeof(*out->models);
    out->models = (sf_msbs_model_t *)sf_xalloc(a, bytes);
    if (!out->models) return SF_ERR_OOM;
    memset(out->models, 0, bytes);

    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;

    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbs_model_read_one(r, entry_offsets[i], &out->models[i].data, a);
            if (rc != SF_OK) break;
        }
    }

    sf_xfree(a, entry_offsets);
    if (rc != SF_OK) {
        msbs_model_param_free(out->models, count, a);
        sf_xfree(a, out->models);
        out->models = NULL;
        out->model_count = 0;
    }
    return rc;
}

static sf_result_t msbs_model_write_type_data(sf_binary_writer_t *w, const msbs_model_t *model) {
    const struct {
        bool unk_t00, unk_t01, unk_t02;
        float unk_t04, unk_t08, unk_t0c, unk_t10, unk_t14, unk_t18;
    } *mp = (const void *)&model->u.map_piece;
    sf_result_t rc;
    rc = sf_binary_writer_write_bool(w, mp->unk_t00); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, mp->unk_t01); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, mp->unk_t02); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_f32(w, mp->unk_t04); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_f32(w, mp->unk_t08); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_f32(w, mp->unk_t0c); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_f32(w, mp->unk_t10); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_f32(w, mp->unk_t14); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_f32(w, mp->unk_t18); if (rc != SF_OK) return rc;
    return sf_binary_writer_write_i32(w, 0);
}

static sf_result_t msbs_model_write_one(sf_binary_writer_t *w, const msbs_model_t *model,
                                        int32_t id, int32_t index) {
    char name_offset_key[32];
    char sib_offset_key[32];
    char type_data_offset_key[32];
    snprintf(name_offset_key, sizeof name_offset_key, "MsbsModelName%d", index);
    snprintf(sib_offset_key, sizeof sib_offset_key, "MsbsModelSib%d", index);
    snprintf(type_data_offset_key, sizeof type_data_offset_key, "MsbsModelType%d", index);

    uint32_t type = 0;
    sf_result_t rc = msbs_model_type_from_kind(model->kind, &type);
    if (rc != SF_OK) return rc;

    int64_t start = sf_binary_writer_position(w);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_offset_key), return rc);
    rc = sf_binary_writer_write_u32(w, type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, sib_offset_key), return rc);
    rc = sf_binary_writer_write_i32(w, model->instance_count); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, model->unk1c); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, type_data_offset_key), return rc);

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_offset_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, model->name ? model->name : "", true);
    if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, sib_offset_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, model->sib_path ? model->sib_path : "", true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    if (msbs_model_has_type_data(model->kind)) {
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, type_data_offset_key, sf_binary_writer_position(w) - start), return rc);
        return msbs_model_write_type_data(w, model);
    }
    return sf_binary_writer_fill_i64(w, type_data_offset_key, 0);
}

sf_result_t msbs_model_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    if (!w || !msbs || msbs->model_count < 0) return SF_ERR_INVALID_ARG;

    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 35); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbs->model_count + 1); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbsNameOff0"), return rc);

    for (int32_t i = 0; i < msbs->model_count; i++) {
        char entry_key[32];
        snprintf(entry_key, sizeof entry_key, "MsbsModelEntry%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, entry_key), return rc);
    }
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, "MsbsNextList0"), return rc);

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, "MsbsNameOff0", sf_binary_writer_position(w)), return rc);
    rc = sf_binary_writer_write_utf16(w, "MODEL_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    for (int32_t i = 0; i < msbs->model_count; i++) {
        char entry_key[32];
        snprintf(entry_key, sizeof entry_key, "MsbsModelEntry%d", i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, entry_key, sf_binary_writer_position(w)), return rc);
        rc = msbs_model_write_one(w, &msbs->models[i].data, i, i);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}

sf_msb_model_kind_t sf_msb_model_get_kind(const sf_msb_model_t *m) {
    const sf_msbs_model_t *model = (const sf_msbs_model_t *)m;
    return model ? model->data.kind : SF_MSB_MODEL_OTHER;
}

sf_result_t sf_msb_model_get_name(const sf_msb_model_t *m, char **out) {
    if (!m || !out) return SF_ERR_INVALID_ARG;
    const sf_msbs_model_t *model = (const sf_msbs_model_t *)m;
    *out = sf_strdup(NULL, model->data.name ? model->data.name : "");
    return *out ? SF_OK : SF_ERR_OOM;
}
