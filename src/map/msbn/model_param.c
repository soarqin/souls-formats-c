/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbn_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

void msbn_model_param_free(sf_msbn_model_t *models, int32_t count, const sf_allocator_t *a) {
    if (!models || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, models[i].data.name);
        memset(&models[i], 0, sizeof(models[i]));
    }
}

static sf_result_t msbn_model_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                       msbn_model_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    if (type != MSBN_MODEL_MAP_PIECE || name_offset == 0) return SF_ERR_BAD_MAGIC;
    out->kind = SF_MSB_MODEL_MAP_PIECE;
    return sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
}

sf_result_t msbn_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbn_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->model_count = count;
    if (count == 0) return SF_OK;
    out->models = (sf_msbn_model_t *)sf_xalloc(a, (size_t)count * sizeof(*out->models));
    if (!out->models) return SF_ERR_OOM;
    memset(out->models, 0, (size_t)count * sizeof(*out->models));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->models); out->models = NULL; out->model_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msbn_model_read_one(r, offsets[i], &out->models[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msbn_model_param_free(out->models, count, a); sf_xfree(a, out->models); out->models = NULL; out->model_count = 0; }
    return rc;
}

static sf_result_t msbn_model_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msbn_model_write_entry(sf_binary_writer_t *w, const void *entry,
                                          size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbn_model_t *model = (const sf_msbn_model_t *)entry;
    if (model->data.kind != SF_MSB_MODEL_MAP_PIECE) return SF_ERR_UNSUPPORTED_VERSION;
    char name_key[32];
    snprintf(name_key, sizeof name_key, "MsbnModelName%zu", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_u32(w, MSBN_MODEL_MAP_PIECE); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msbn_model_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, model->data.name ? model->data.name : "", true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 4);
}

sf_result_t msbn_model_param_write(sf_binary_writer_t *w, const sf_msbn_t *msbn) {
    if (!w || !msbn || msbn->model_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "MODEL_PARAM_ST", "MsbnNextList0", msbn->models,
                                       (size_t)msbn->model_count, sizeof(*msbn->models),
                                       msbn_model_write_entry, NULL);
}
