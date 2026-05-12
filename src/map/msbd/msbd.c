/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbd_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBD_LIST_MODELS = 0,
    MSBD_LIST_EVENTS,
    MSBD_LIST_REGIONS,
    MSBD_LIST_PARTS,
    MSBD_LIST_TREES,
    MSBD_LIST_COUNT,
};

static const char *const k_msbd_list_names[MSBD_LIST_COUNT] = {
    [MSBD_LIST_MODELS]  = "MODEL_PARAM_ST",
    [MSBD_LIST_EVENTS]  = "EVENT_PARAM_ST",
    [MSBD_LIST_REGIONS] = "POINT_PARAM_ST",
    [MSBD_LIST_PARTS]   = "PARTS_PARAM_ST",
    [MSBD_LIST_TREES]   = "MAPSTUDIO_TREE_ST",
};

typedef struct msbd_read_ctx {
    sf_msbd_t            *msbd;
    const sf_allocator_t *alloc;
    int32_t               index;
} msbd_read_ctx_t;

static sf_result_t msbd_read_list_cb(const char *name, int32_t entry_count,
                                     sf_binary_reader_t *r, void *ctx) {
    msbd_read_ctx_t *read_ctx = (msbd_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msbd || read_ctx->index < 0 ||
        read_ctx->index >= MSBD_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }

    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msbd_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSBD_LIST_MODELS:
        return msbd_model_param_read(r, entry_count, read_ctx->msbd, read_ctx->alloc);
    case MSBD_LIST_EVENTS:
        return msbd_event_param_read(r, entry_count, read_ctx->msbd, read_ctx->alloc);
    case MSBD_LIST_REGIONS:
        return msbd_point_param_read(r, entry_count, read_ctx->msbd, read_ctx->alloc);
    case MSBD_LIST_PARTS:
        return msbd_parts_param_read(r, entry_count, read_ctx->msbd, read_ctx->alloc);
    case MSBD_LIST_TREES:
        if (entry_count != 0) return SF_ERR_UNSUPPORTED_VERSION;
        read_ctx->msbd->trees_count = 0;
        return SF_OK;
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbd_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "MsbdNextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

sf_result_t sf_msbd_read_from_memory(sf_msbd_t **out, const uint8_t *data, size_t size,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t rc = sf_istream_open_memory(&stream, data, size, alloc);
    if (rc != SF_OK) return rc;

    sf_binary_reader_t *reader = NULL;
    rc = sf_binary_reader_create(&reader, stream, true, alloc);
    if (rc != SF_OK) { sf_istream_close(stream); return rc; }

    msb_legacy_layout_t layout;
    rc = msb_legacy_read_header(reader, &layout, alloc);
    if (rc != SF_OK) { sf_binary_reader_destroy(reader); sf_istream_close(stream); return rc; }

    sf_msbd_t *msbd = (sf_msbd_t *)sf_xalloc(alloc, sizeof(*msbd));
    if (!msbd) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msbd, 0, sizeof(*msbd));
    msbd->alloc = alloc;

    if (layout.list_count != MSBD_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbd_read_ctx_t ctx = { msbd, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msbd_read_list_cb, &ctx, alloc);
        if (rc == SF_OK && ctx.index != MSBD_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);
    if (rc != SF_OK) { sf_msbd_destroy(msbd); return rc; }
    *out = msbd;
    return SF_OK;
}

sf_result_t sf_msbd_write_to_memory(const sf_msbd_t *msbd, uint8_t **out_data,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msbd != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t rc = sf_ostream_open_memory(&stream, alloc);
    if (rc != SF_OK) return rc;

    sf_binary_writer_t *writer = NULL;
    rc = sf_binary_writer_create(&writer, stream, true, alloc);
    if (rc != SF_OK) { sf_ostream_close(stream); return rc; }

    for (int i = 0; i < MSBD_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) { rc = msbd_fill_next_param(writer, i - 1, list_start); if (rc != SF_OK) goto fail; }
        if (i == MSBD_LIST_MODELS) rc = msbd_model_param_write(writer, msbd);
        else if (i == MSBD_LIST_EVENTS) rc = msbd_event_param_write(writer, msbd);
        else if (i == MSBD_LIST_REGIONS) rc = msbd_point_param_write(writer, msbd);
        else if (i == MSBD_LIST_PARTS) rc = msbd_parts_param_write(writer, msbd);
        else rc = msb_legacy_reserve_list(writer, "MAPSTUDIO_TREE_ST", MSBD_LIST_TREES);
        if (rc != SF_OK) goto fail;
    }
    rc = msb_legacy_fill_list(writer, MSBD_LIST_TREES, 0, 0);
    if (rc != SF_OK) goto fail;

    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;
fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbd_destroy(sf_msbd_t *msbd) {
    if (!msbd) return;
    const sf_allocator_t *alloc = msbd->alloc;
    msbd_model_param_free(msbd->models, msbd->model_count, alloc);
    msbd_event_param_free(msbd->events, msbd->event_count, alloc);
    msbd_point_param_free(msbd->regions, msbd->region_count, alloc);
    msbd_parts_param_free(msbd->parts, msbd->part_count, alloc);
    sf_xfree(alloc, msbd->models);
    sf_xfree(alloc, msbd->events);
    sf_xfree(alloc, msbd->regions);
    sf_xfree(alloc, msbd->parts);
    sf_xfree(alloc, msbd);
}

int32_t sf_msbd_model_count(const sf_msbd_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msbd_event_count(const sf_msbd_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msbd_region_count(const sf_msbd_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbd_part_count(const sf_msbd_t *m)   { return m ? m->part_count : 0; }
int32_t sf_msbd_tree_count(const sf_msbd_t *m)   { return m ? m->trees_count : 0; }

const sf_msbd_model_t *sf_msbd_model_at(const sf_msbd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}
const sf_msbd_event_t *sf_msbd_event_at(const sf_msbd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}
const sf_msbd_region_t *sf_msbd_region_at(const sf_msbd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}
const sf_msbd_part_t *sf_msbd_part_at(const sf_msbd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
