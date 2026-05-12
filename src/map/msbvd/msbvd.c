/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbvd_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBVD_LIST_MODELS = 0,
    MSBVD_LIST_EVENTS,
    MSBVD_LIST_REGIONS,
    MSBVD_LIST_ROUTES,
    MSBVD_LIST_LAYERS,
    MSBVD_LIST_PARTS,
    MSBVD_LIST_DRAWING_TREE,
    MSBVD_LIST_COLLISION_TREE,
    MSBVD_LIST_COUNT,
};

static const char *const k_msbvd_list_names[MSBVD_LIST_COUNT] = {
    [MSBVD_LIST_MODELS] = "MODEL_PARAM_ST",
    [MSBVD_LIST_EVENTS] = "EVENT_PARAM_ST",
    [MSBVD_LIST_REGIONS] = "POINT_PARAM_ST",
    [MSBVD_LIST_ROUTES] = "ROUTE_PARAM_ST",
    [MSBVD_LIST_LAYERS] = "LAYER_PARAM_ST",
    [MSBVD_LIST_PARTS] = "PARTS_PARAM_ST",
    [MSBVD_LIST_DRAWING_TREE] = "MAPSTUDIO_TREE_ST",
    [MSBVD_LIST_COLLISION_TREE] = "MAPSTUDIO_TREE_ST",
};

typedef struct msbvd_read_ctx {
    sf_msbvd_t         *msb;
    const sf_allocator_t *alloc;
    int32_t              index;
} msbvd_read_ctx_t;

static sf_result_t msbvd_read_empty(int32_t entry_count, int32_t *out_count) {
    if (entry_count != 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (out_count) *out_count = 0;
    return SF_OK;
}

static sf_result_t msbvd_read_list_cb(const char *name, int32_t entry_count,
                                       sf_binary_reader_t *r, void *ctx) {
    msbvd_read_ctx_t *read_ctx = (msbvd_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msb || read_ctx->index < 0 ||
        read_ctx->index >= MSBVD_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msbvd_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSBVD_LIST_MODELS:
        return msbvd_model_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBVD_LIST_EVENTS:
        return msbvd_event_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBVD_LIST_REGIONS:
        return msbvd_point_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBVD_LIST_ROUTES:
        return msbvd_read_empty(entry_count, &read_ctx->msb->route_count);
    case MSBVD_LIST_LAYERS:
        return msbvd_read_empty(entry_count, &read_ctx->msb->layer_count);
    case MSBVD_LIST_PARTS:
        return msbvd_parts_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBVD_LIST_DRAWING_TREE:
        return msbvd_read_empty(entry_count, &read_ctx->msb->drawing_tree_count);
    case MSBVD_LIST_COLLISION_TREE:
        return msbvd_read_empty(entry_count, &read_ctx->msb->collision_tree_count);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbvd_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[40];
    snprintf(next_name, sizeof next_name, "MsbvdNextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

static sf_result_t msbvd_write_no_entry(sf_binary_writer_t *w, const void *entry,
                                         size_t index, void *ctx) {
    (void)w;
    (void)entry;
    (void)index;
    (void)ctx;
    return SF_ERR_INTERNAL;
}

static sf_result_t msbvd_empty_param_write(sf_binary_writer_t *w, const char *name,
                                            int list_index) {
    char next_key[40];
    snprintf(next_key, sizeof next_key, "MsbvdNextList%d", list_index);
    return msb_legacy_entry_list_write(w, name, next_key, NULL, 0, 0,
                                       msbvd_write_no_entry, NULL);
}

sf_result_t sf_msbvd_read_from_memory(sf_msbvd_t **out, const uint8_t *data, size_t size,
                                       const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t rc = sf_istream_open_memory(&stream, data, size, alloc);
    if (rc != SF_OK) return rc;

    sf_binary_reader_t *reader = NULL;
    rc = sf_binary_reader_create(&reader, stream, false, alloc);
    if (rc != SF_OK) {
        sf_istream_close(stream);
        return rc;
    }

    rc = msb_legacy_assert_header(reader);
    if (rc != SF_OK) {
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return rc;
    }

    msb_legacy_layout_t layout;
    rc = msb_legacy_read_header(reader, &layout, alloc);
    if (rc != SF_OK) {
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return rc;
    }

    sf_msbvd_t *msb = (sf_msbvd_t *)sf_xalloc(alloc, sizeof(*msb));
    if (!msb) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msb, 0, sizeof(*msb));
    msb->alloc = alloc;

    if (layout.list_count != MSBVD_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbvd_read_ctx_t read_ctx = { msb, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msbvd_read_list_cb, &read_ctx, alloc);
        if (rc == SF_OK && read_ctx.index != MSBVD_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);
    if (rc != SF_OK) {
        sf_msbvd_destroy(msb);
        return rc;
    }
    *out = msb;
    return SF_OK;
}

sf_result_t sf_msbvd_write_to_memory(const sf_msbvd_t *msb, uint8_t **out_data,
                                      size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msb != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t rc = sf_ostream_open_memory(&stream, alloc);
    if (rc != SF_OK) return rc;

    sf_binary_writer_t *writer = NULL;
    rc = sf_binary_writer_create(&writer, stream, false, alloc);
    if (rc != SF_OK) {
        sf_ostream_close(stream);
        return rc;
    }

    rc = msb_legacy_write_header(writer);
    if (rc != SF_OK) goto fail;

    for (int i = 0; i < MSBVD_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msbvd_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        if (i == MSBVD_LIST_MODELS) rc = msbvd_model_param_write(writer, msb);
        else if (i == MSBVD_LIST_EVENTS) rc = msbvd_event_param_write(writer, msb);
        else if (i == MSBVD_LIST_REGIONS) rc = msbvd_point_param_write(writer, msb);
        else if (i == MSBVD_LIST_PARTS) rc = msbvd_parts_param_write(writer, msb);
        else rc = msbvd_empty_param_write(writer, k_msbvd_list_names[i], i);
        if (rc != SF_OK) goto fail;
    }

    rc = msbvd_fill_next_param(writer, MSBVD_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbvd_destroy(sf_msbvd_t *msb) {
    if (!msb) return;
    const sf_allocator_t *alloc = msb->alloc;
    msbvd_model_param_free(msb->models, msb->model_count, alloc);
    msbvd_event_param_free(msb->events, msb->event_count, alloc);
    msbvd_point_param_free(msb->regions, msb->region_count, alloc);
    msbvd_parts_param_free(msb->parts, msb->part_count, alloc);
    sf_xfree(alloc, msb->models);
    sf_xfree(alloc, msb->events);
    sf_xfree(alloc, msb->regions);
    sf_xfree(alloc, msb->parts);
    sf_xfree(alloc, msb);
}

int32_t sf_msbvd_model_count(const sf_msbvd_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msbvd_event_count(const sf_msbvd_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msbvd_region_count(const sf_msbvd_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbvd_part_count(const sf_msbvd_t *m)   { return m ? m->part_count : 0; }

const sf_msbvd_model_t *sf_msbvd_model_at(const sf_msbvd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}
const sf_msbvd_event_t *sf_msbvd_event_at(const sf_msbvd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}
const sf_msbvd_region_t *sf_msbvd_region_at(const sf_msbvd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}
const sf_msbvd_part_t *sf_msbvd_part_at(const sf_msbvd_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
