/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbdr_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum { MSBDR_LIST_MODELS = 0, MSBDR_LIST_EVENTS, MSBDR_LIST_REGIONS, MSBDR_LIST_PARTS, MSBDR_LIST_TREES, MSBDR_LIST_COUNT };
static const char *const k_msbdr_list_names[MSBDR_LIST_COUNT] = {
    [MSBDR_LIST_MODELS] = "MODEL_PARAM_ST", [MSBDR_LIST_EVENTS] = "EVENT_PARAM_ST",
    [MSBDR_LIST_REGIONS] = "POINT_PARAM_ST", [MSBDR_LIST_PARTS] = "PARTS_PARAM_ST",
    [MSBDR_LIST_TREES] = "MAPSTUDIO_TREE_ST",
};
typedef struct msbdr_read_ctx { sf_msbdr_t *msbdr; const sf_allocator_t *alloc; int32_t index; } msbdr_read_ctx_t;

static sf_result_t msbdr_read_list_cb(const char *name, int32_t entry_count, sf_binary_reader_t *r, void *ctx) {
    msbdr_read_ctx_t *read_ctx = (msbdr_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msbdr || read_ctx->index < 0 || read_ctx->index >= MSBDR_LIST_COUNT) return SF_ERR_INTERNAL;
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msbdr_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;
    switch (list_index) {
    case MSBDR_LIST_MODELS: return msbdr_model_param_read(r, entry_count, read_ctx->msbdr, read_ctx->alloc);
    case MSBDR_LIST_EVENTS: return msbdr_event_param_read(r, entry_count, read_ctx->msbdr, read_ctx->alloc);
    case MSBDR_LIST_REGIONS: return msbdr_point_param_read(r, entry_count, read_ctx->msbdr, read_ctx->alloc);
    case MSBDR_LIST_PARTS: return msbdr_parts_param_read(r, entry_count, read_ctx->msbdr, read_ctx->alloc);
    case MSBDR_LIST_TREES: if (entry_count != 0) return SF_ERR_UNSUPPORTED_VERSION; read_ctx->msbdr->trees_count = 0; return SF_OK;
    default: return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbdr_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32]; snprintf(next_name, sizeof next_name, "MsbdrNextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

sf_result_t sf_msbdr_read_from_memory(sf_msbdr_t **out, const uint8_t *data, size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL)); *out = NULL; alloc = sf_alloc_or_default(alloc);
    sf_istream_t *stream = NULL; sf_result_t rc = sf_istream_open_memory(&stream, data, size, alloc); if (rc != SF_OK) return rc;
    sf_binary_reader_t *reader = NULL; rc = sf_binary_reader_create(&reader, stream, false, alloc); if (rc != SF_OK) { sf_istream_close(stream); return rc; }
    rc = msb_legacy_assert_header(reader); if (rc != SF_OK) { sf_binary_reader_destroy(reader); sf_istream_close(stream); return rc; }
    msb_legacy_layout_t layout; rc = msb_legacy_read_header(reader, &layout, alloc);
    if (rc != SF_OK) { sf_binary_reader_destroy(reader); sf_istream_close(stream); return rc; }
    sf_msbdr_t *msbdr = (sf_msbdr_t *)sf_xalloc(alloc, sizeof(*msbdr));
    if (!msbdr) { msb_legacy_free_layout(&layout, alloc); sf_binary_reader_destroy(reader); sf_istream_close(stream); return SF_ERR_OOM; }
    memset(msbdr, 0, sizeof(*msbdr)); msbdr->alloc = alloc;
    if (layout.list_count != MSBDR_LIST_COUNT) rc = SF_ERR_UNSUPPORTED_VERSION;
    else { msbdr_read_ctx_t ctx = { msbdr, alloc, 0 }; rc = msb_legacy_iter_lists(reader, &layout, msbdr_read_list_cb, &ctx, alloc); if (rc == SF_OK && ctx.index != MSBDR_LIST_COUNT) rc = SF_ERR_INTERNAL; }
    msb_legacy_free_layout(&layout, alloc); sf_binary_reader_destroy(reader); sf_istream_close(stream);
    if (rc != SF_OK) { sf_msbdr_destroy(msbdr); return rc; } *out = msbdr; return SF_OK;
}

sf_result_t sf_msbdr_write_to_memory(const sf_msbdr_t *msbdr, uint8_t **out_data, size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msbdr != NULL && out_data != NULL && out_size != NULL); *out_data = NULL; *out_size = 0; alloc = sf_alloc_or_default(alloc);
    sf_ostream_t *stream = NULL; sf_result_t rc = sf_ostream_open_memory(&stream, alloc); if (rc != SF_OK) return rc;
    sf_binary_writer_t *writer = NULL; rc = sf_binary_writer_create(&writer, stream, false, alloc); if (rc != SF_OK) { sf_ostream_close(stream); return rc; }
    rc = msb_legacy_write_header(writer); if (rc != SF_OK) goto fail;
    for (int i = 0; i < MSBDR_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer); if (i > 0) { rc = msbdr_fill_next_param(writer, i - 1, list_start); if (rc != SF_OK) goto fail; }
        if (i == MSBDR_LIST_MODELS) rc = msbdr_model_param_write(writer, msbdr);
        else if (i == MSBDR_LIST_EVENTS) rc = msbdr_event_param_write(writer, msbdr);
        else if (i == MSBDR_LIST_REGIONS) rc = msbdr_point_param_write(writer, msbdr);
        else if (i == MSBDR_LIST_PARTS) rc = msbdr_parts_param_write(writer, msbdr);
        else rc = msb_legacy_reserve_list(writer, "MAPSTUDIO_TREE_ST", MSBDR_LIST_TREES);
        if (rc != SF_OK) goto fail;
    }
    rc = msb_legacy_fill_list(writer, MSBDR_LIST_TREES, 0, 0); if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size); sf_ostream_close(stream); return rc;
fail: sf_binary_writer_destroy(writer); sf_ostream_close(stream); return rc;
}

void sf_msbdr_destroy(sf_msbdr_t *msbdr) {
    if (!msbdr) return;
    const sf_allocator_t *alloc = msbdr->alloc;
    msbdr_model_param_free(msbdr->models, msbdr->model_count, alloc); msbdr_event_param_free(msbdr->events, msbdr->event_count, alloc);
    msbdr_point_param_free(msbdr->regions, msbdr->region_count, alloc); msbdr_parts_param_free(msbdr->parts, msbdr->part_count, alloc);
    sf_xfree(alloc, msbdr->models); sf_xfree(alloc, msbdr->events); sf_xfree(alloc, msbdr->regions); sf_xfree(alloc, msbdr->parts); sf_xfree(alloc, msbdr);
}
int32_t sf_msbdr_model_count(const sf_msbdr_t *m) { return m ? m->model_count : 0; }
int32_t sf_msbdr_event_count(const sf_msbdr_t *m) { return m ? m->event_count : 0; }
int32_t sf_msbdr_region_count(const sf_msbdr_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbdr_part_count(const sf_msbdr_t *m) { return m ? m->part_count : 0; }
int32_t sf_msbdr_tree_count(const sf_msbdr_t *m) { return m ? m->trees_count : 0; }
const sf_msbdr_model_t *sf_msbdr_model_at(const sf_msbdr_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL; }
const sf_msbdr_event_t *sf_msbdr_event_at(const sf_msbdr_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL; }
const sf_msbdr_region_t *sf_msbdr_region_at(const sf_msbdr_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL; }
const sf_msbdr_part_t *sf_msbdr_part_at(const sf_msbdr_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL; }
