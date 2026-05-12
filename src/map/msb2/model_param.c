/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msb2_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum { MSB2_MODEL_MAP_PIECE = 0, MSB2_MODEL_OBJECT = 1, MSB2_MODEL_COLLISION = 3,
       MSB2_MODEL_NAVMESH = 4 };

static sf_result_t msb2_model_kind_from_type(uint32_t type, sf_msb_model_kind_t *out) {
    if (!out) return SF_ERR_INVALID_ARG;
    switch (type) {
    case MSB2_MODEL_MAP_PIECE: *out = SF_MSB_MODEL_MAP_PIECE; return SF_OK;
    case MSB2_MODEL_OBJECT: *out = SF_MSB_MODEL_OBJECT; return SF_OK;
    case MSB2_MODEL_COLLISION: *out = SF_MSB_MODEL_COLLISION; return SF_OK;
    case MSB2_MODEL_NAVMESH: *out = SF_MSB_MODEL_NAVMESH; return SF_OK;
    default: return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msb2_model_type_from_kind(sf_msb_model_kind_t kind, uint32_t *out) {
    if (!out) return SF_ERR_INVALID_ARG;
    switch (kind) {
    case SF_MSB_MODEL_MAP_PIECE: *out = MSB2_MODEL_MAP_PIECE; return SF_OK;
    case SF_MSB_MODEL_OBJECT: *out = MSB2_MODEL_OBJECT; return SF_OK;
    case SF_MSB_MODEL_COLLISION: *out = MSB2_MODEL_COLLISION; return SF_OK;
    case SF_MSB_MODEL_NAVMESH: *out = MSB2_MODEL_NAVMESH; return SF_OK;
    default: return SF_ERR_UNSUPPORTED_VERSION;
    }
}

void msb2_model_param_free(sf_msb2_model_t *models, int32_t count, const sf_allocator_t *a) {
    if (!models || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, models[i].data.name);
        sf_xfree(a, models[i].data.sib_path);
        memset(&models[i], 0, sizeof(models[i]));
    }
}

static sf_result_t msb2_model_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                       msb2_model_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0, id = 0, sib_offset = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &sib_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->instance_count); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    (void)id;
    rc = msb2_model_kind_from_type(type, &out->kind); if (rc != SF_OK) return rc;
    if (name_offset == 0 || sib_offset == 0) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    return sf_binary_reader_get_ascii(r, (int64_t)entry_offset + sib_offset, &out->sib_path, NULL);
}

sf_result_t msb2_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->model_count = count;
    if (count == 0) return SF_OK;
    out->models = (sf_msb2_model_t *)sf_xalloc(a, (size_t)count * sizeof(*out->models));
    if (!out->models) return SF_ERR_OOM;
    memset(out->models, 0, (size_t)count * sizeof(*out->models));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->models); out->models = NULL; out->model_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msb2_model_read_one(r, offsets[i], &out->models[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msb2_model_param_free(out->models, count, a); sf_xfree(a, out->models); out->models = NULL; out->model_count = 0; }
    return rc;
}

static sf_result_t msb2_model_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msb2_model_write_entry(sf_binary_writer_t *w, const void *entry,
                                          size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msb2_model_t *model = (const sf_msb2_model_t *)entry;
    char name_key[32], sib_key[32];
    snprintf(name_key, sizeof name_key, "Msb2ModelName%zu", index);
    snprintf(sib_key, sizeof sib_key, "Msb2ModelSib%zu", index);
    uint32_t type = 0;
    sf_result_t rc = msb2_model_type_from_kind(model->data.kind, &type); if (rc != SF_OK) return rc;
    int64_t start = sf_binary_writer_position(w);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_u32(w, type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, (int32_t)index); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, sib_key), return rc);
    rc = sf_binary_writer_write_i32(w, model->data.instance_count); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_model_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, model->data.name ? model->data.name : "", true); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_model_fill_rel_i32(w, sib_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, model->data.sib_path ? model->data.sib_path : "", true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 4);
}

sf_result_t msb2_model_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2) {
    if (!w || !msb2 || msb2->model_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "MODEL_PARAM_ST", "Msb2NextList0", msb2->models,
                                       (size_t)msb2->model_count, sizeof(*msb2->models),
                                       msb2_model_write_entry, NULL);
}
