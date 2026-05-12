/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbb_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum { MSBB_LIST_MODELS = 0, MSBB_LIST_EVENTS, MSBB_LIST_REGIONS, MSBB_LIST_PARTS, MSBB_LIST_COUNT };
static const char *const k_msbb_list_names[MSBB_LIST_COUNT] = {
    [MSBB_LIST_MODELS] = "MODEL_PARAM_ST", [MSBB_LIST_EVENTS] = "EVENT_PARAM_ST",
    [MSBB_LIST_REGIONS] = "POINT_PARAM_ST", [MSBB_LIST_PARTS] = "PARTS_PARAM_ST",
};
typedef struct msbb_read_ctx { sf_msbb_t *msbb; const sf_allocator_t *alloc; int32_t index; } msbb_read_ctx_t;

static sf_result_t msbb_read_list_cb(const char *name, int32_t entry_count, sf_binary_reader_t *r, void *ctx) {
    msbb_read_ctx_t *read_ctx = (msbb_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msbb || read_ctx->index < 0 || read_ctx->index >= MSBB_LIST_COUNT) return SF_ERR_INTERNAL;
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msbb_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;
    switch (list_index) {
    case MSBB_LIST_MODELS: return msbb_model_param_read(r, entry_count, read_ctx->msbb, read_ctx->alloc);
    case MSBB_LIST_EVENTS: return msbb_event_param_read(r, entry_count, read_ctx->msbb, read_ctx->alloc);
    case MSBB_LIST_REGIONS: return msbb_point_param_read(r, entry_count, read_ctx->msbb, read_ctx->alloc);
    case MSBB_LIST_PARTS: return msbb_parts_param_read(r, entry_count, read_ctx->msbb, read_ctx->alloc);
    default: return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbb_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32]; snprintf(next_name, sizeof next_name, "MsbbNextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

sf_result_t sf_msbb_read_from_memory(sf_msbb_t **out, const uint8_t *data, size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL)); *out = NULL; alloc = sf_alloc_or_default(alloc);
    sf_istream_t *stream = NULL; sf_result_t rc = sf_istream_open_memory(&stream, data, size, alloc); if (rc != SF_OK) return rc;
    sf_binary_reader_t *reader = NULL; rc = sf_binary_reader_create(&reader, stream, false, alloc); if (rc != SF_OK) { sf_istream_close(stream); return rc; }
    rc = msb_legacy_assert_header(reader); if (rc != SF_OK) { sf_binary_reader_destroy(reader); sf_istream_close(stream); return rc; }
    msb_legacy_layout_t layout; rc = msb_legacy_read_header(reader, &layout, alloc);
    if (rc != SF_OK) { sf_binary_reader_destroy(reader); sf_istream_close(stream); return rc; }
    sf_msbb_t *msbb = (sf_msbb_t *)sf_xalloc(alloc, sizeof(*msbb));
    if (!msbb) { msb_legacy_free_layout(&layout, alloc); sf_binary_reader_destroy(reader); sf_istream_close(stream); return SF_ERR_OOM; }
    memset(msbb, 0, sizeof(*msbb)); msbb->alloc = alloc;
    if (layout.list_count != MSBB_LIST_COUNT) rc = SF_ERR_UNSUPPORTED_VERSION;
    else { msbb_read_ctx_t ctx = { msbb, alloc, 0 }; rc = msb_legacy_iter_lists(reader, &layout, msbb_read_list_cb, &ctx, alloc); if (rc == SF_OK && ctx.index != MSBB_LIST_COUNT) rc = SF_ERR_INTERNAL; }
    msb_legacy_free_layout(&layout, alloc); sf_binary_reader_destroy(reader); sf_istream_close(stream);
    if (rc != SF_OK) { sf_msbb_destroy(msbb); return rc; } *out = msbb; return SF_OK;
}

sf_result_t sf_msbb_write_to_memory(const sf_msbb_t *msbb, uint8_t **out_data, size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msbb != NULL && out_data != NULL && out_size != NULL); *out_data = NULL; *out_size = 0; alloc = sf_alloc_or_default(alloc);
    sf_ostream_t *stream = NULL; sf_result_t rc = sf_ostream_open_memory(&stream, alloc); if (rc != SF_OK) return rc;
    sf_binary_writer_t *writer = NULL; rc = sf_binary_writer_create(&writer, stream, false, alloc); if (rc != SF_OK) { sf_ostream_close(stream); return rc; }
    rc = msb_legacy_write_header(writer); if (rc != SF_OK) goto fail;
    for (int i = 0; i < MSBB_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer); if (i > 0) { rc = msbb_fill_next_param(writer, i - 1, list_start); if (rc != SF_OK) goto fail; }
        if (i == MSBB_LIST_MODELS) rc = msbb_model_param_write(writer, msbb);
        else if (i == MSBB_LIST_EVENTS) rc = msbb_event_param_write(writer, msbb);
        else if (i == MSBB_LIST_REGIONS) rc = msbb_point_param_write(writer, msbb);
        else rc = msbb_parts_param_write(writer, msbb);
        if (rc != SF_OK) goto fail;
    }
    rc = msbb_fill_next_param(writer, MSBB_LIST_COUNT - 1, 0); if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size); sf_ostream_close(stream); return rc;
fail: sf_binary_writer_destroy(writer); sf_ostream_close(stream); return rc;
}

void sf_msbb_destroy(sf_msbb_t *msbb) {
    if (!msbb) return;
    const sf_allocator_t *alloc = msbb->alloc;
    msbb_model_param_free(msbb->models, msbb->model_count, alloc); msbb_event_param_free(msbb->events, msbb->event_count, alloc);
    msbb_point_param_free(msbb->regions, msbb->region_count, alloc); msbb_parts_param_free(msbb->parts, msbb->part_count, alloc);
    sf_xfree(alloc, msbb->models); sf_xfree(alloc, msbb->events); sf_xfree(alloc, msbb->regions); sf_xfree(alloc, msbb->parts); sf_xfree(alloc, msbb);
}
int32_t sf_msbb_model_count(const sf_msbb_t *m) { return m ? m->model_count : 0; }
int32_t sf_msbb_event_count(const sf_msbb_t *m) { return m ? m->event_count : 0; }
int32_t sf_msbb_region_count(const sf_msbb_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbb_part_count(const sf_msbb_t *m) { return m ? m->part_count : 0; }
const sf_msbb_model_t *sf_msbb_model_at(const sf_msbb_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL; }
const sf_msbb_event_t *sf_msbb_event_at(const sf_msbb_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL; }
const sf_msbb_region_t *sf_msbb_region_at(const sf_msbb_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL; }
const sf_msbb_part_t *sf_msbb_part_at(const sf_msbb_t *m, int32_t idx) { return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL; }
