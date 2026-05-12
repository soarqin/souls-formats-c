/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbvd_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum { MSBVD_MODEL_MAP_PIECE = 0, MSBVD_MODEL_OBJECT = 1, MSBVD_MODEL_ENEMY = 2,
       MSBVD_MODEL_DUMMY = 4 };

static sf_result_t msbvd_model_kind_from_type(uint32_t type, sf_msb_model_kind_t *out) {
    if (!out) return SF_ERR_INVALID_ARG;
    switch (type) {
    case MSBVD_MODEL_MAP_PIECE: *out = SF_MSB_MODEL_MAP_PIECE; return SF_OK;
    case MSBVD_MODEL_OBJECT: *out = SF_MSB_MODEL_OBJECT; return SF_OK;
    case MSBVD_MODEL_ENEMY: *out = SF_MSB_MODEL_CHARACTER; return SF_OK;
    case MSBVD_MODEL_DUMMY: *out = SF_MSB_MODEL_PLAYER; return SF_OK;
    default: return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbvd_model_type_from_kind(sf_msb_model_kind_t kind, uint32_t *out) {
    if (!out) return SF_ERR_INVALID_ARG;
    switch (kind) {
    case SF_MSB_MODEL_MAP_PIECE: *out = MSBVD_MODEL_MAP_PIECE; return SF_OK;
    case SF_MSB_MODEL_OBJECT: *out = MSBVD_MODEL_OBJECT; return SF_OK;
    case SF_MSB_MODEL_CHARACTER: *out = MSBVD_MODEL_ENEMY; return SF_OK;
    case SF_MSB_MODEL_PLAYER: *out = MSBVD_MODEL_DUMMY; return SF_OK;
    default: return SF_ERR_UNSUPPORTED_VERSION;
    }
}

void msbvd_model_param_free(sf_msbvd_model_t *models, int32_t count, const sf_allocator_t *a) {
    if (!models || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, models[i].data.name);
        sf_xfree(a, models[i].data.resource_path);
        memset(&models[i], 0, sizeof(models[i]));
    }
}

static sf_result_t msbvd_model_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                         msbvd_model_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0, id = 0, resource_offset = 0, render_offset = 0, row_offset = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &resource_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->instance_count); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &render_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &row_offset); if (rc != SF_OK) return rc;
    (void)id;
    if (name_offset == 0 || resource_offset == 0 || render_offset == 0 || row_offset != 0) {
        return SF_ERR_BAD_MAGIC;
    }
    rc = msbvd_model_kind_from_type(type, &out->kind); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + resource_offset, &out->resource_path, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + render_offset); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_i32_one(r, 0);
}

sf_result_t msbvd_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvd_t *out,
                                    const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->model_count = count;
    if (count == 0) return SF_OK;
    out->models = (sf_msbvd_model_t *)sf_xalloc(a, (size_t)count * sizeof(*out->models));
    if (!out->models) return SF_ERR_OOM;
    memset(out->models, 0, (size_t)count * sizeof(*out->models));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->models); out->models = NULL; out->model_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msbvd_model_read_one(r, offsets[i], &out->models[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msbvd_model_param_free(out->models, count, a); sf_xfree(a, out->models); out->models = NULL; out->model_count = 0; }
    return rc;
}

static sf_result_t msbvd_model_fill_rel_i32(sf_binary_writer_t *w, const char *key,
                                             int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msbvd_model_write_entry(sf_binary_writer_t *w, const void *entry,
                                            size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbvd_model_t *model = (const sf_msbvd_model_t *)entry;
    char name_key[40], resource_key[40], render_key[40], row_key[40];
    snprintf(name_key, sizeof name_key, "MsbvdModelName%zu", index);
    snprintf(resource_key, sizeof resource_key, "MsbvdModelRes%zu", index);
    snprintf(render_key, sizeof render_key, "MsbvdModelRender%zu", index);
    snprintf(row_key, sizeof row_key, "MsbvdModelRow%zu", index);
    uint32_t type = 0;
    sf_result_t rc = msbvd_model_type_from_kind(model->data.kind, &type); if (rc != SF_OK) return rc;
    int64_t start = sf_binary_writer_position(w);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_u32(w, type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, (int32_t)index); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, resource_key), return rc);
    rc = sf_binary_writer_write_i32(w, model->data.instance_count); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, render_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, row_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, msbvd_model_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, model->data.name ? model->data.name : "", true); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbvd_model_fill_rel_i32(w, resource_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, model->data.resource_path ? model->data.resource_path : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbvd_model_fill_rel_i32(w, render_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    return sf_binary_writer_fill_i32(w, row_key, 0);
}

sf_result_t msbvd_model_param_write(sf_binary_writer_t *w, const sf_msbvd_t *msb) {
    if (!w || !msb || msb->model_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "MODEL_PARAM_ST", "MsbvdNextList0", msb->models,
                                       (size_t)msb->model_count, sizeof(*msb->models),
                                       msbvd_model_write_entry, NULL);
}
