/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbv_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBV_LIST_MODELS = 0,
    MSBV_LIST_EVENTS,
    MSBV_LIST_REGIONS,
    MSBV_LIST_ROUTES,
    MSBV_LIST_LAYERS,
    MSBV_LIST_PARTS,
    MSBV_LIST_DRAWING_TREE,
    MSBV_LIST_COLLISION_TREE,
    MSBV_LIST_COUNT,
};

static const char *const k_msbv_list_names[MSBV_LIST_COUNT] = {
    [MSBV_LIST_MODELS] = "MODEL_PARAM_ST",
    [MSBV_LIST_EVENTS] = "EVENT_PARAM_ST",
    [MSBV_LIST_REGIONS] = "POINT_PARAM_ST",
    [MSBV_LIST_ROUTES] = "ROUTE_PARAM_ST",
    [MSBV_LIST_LAYERS] = "LAYER_PARAM_ST",
    [MSBV_LIST_PARTS] = "PARTS_PARAM_ST",
    [MSBV_LIST_DRAWING_TREE] = "MAPSTUDIO_TREE_ST",
    [MSBV_LIST_COLLISION_TREE] = "MAPSTUDIO_TREE_ST",
};

typedef struct msbv_read_ctx {
    sf_msbv_t         *msb;
    const sf_allocator_t *alloc;
    int32_t              index;
} msbv_read_ctx_t;

static sf_result_t msbv_read_empty(int32_t entry_count, int32_t *out_count) {
    if (entry_count != 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (out_count) *out_count = 0;
    return SF_OK;
}

static sf_result_t msbv_read_list_cb(const char *name, int32_t entry_count,
                                       sf_binary_reader_t *r, void *ctx) {
    msbv_read_ctx_t *read_ctx = (msbv_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msb || read_ctx->index < 0 ||
        read_ctx->index >= MSBV_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msbv_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSBV_LIST_MODELS:
        return msbv_model_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBV_LIST_EVENTS:
        return msbv_event_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBV_LIST_REGIONS:
        return msbv_point_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBV_LIST_ROUTES:
        return msbv_read_empty(entry_count, &read_ctx->msb->route_count);
    case MSBV_LIST_LAYERS:
        return msbv_read_empty(entry_count, &read_ctx->msb->layer_count);
    case MSBV_LIST_PARTS:
        return msbv_parts_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBV_LIST_DRAWING_TREE:
        return msbv_read_empty(entry_count, &read_ctx->msb->drawing_tree_count);
    case MSBV_LIST_COLLISION_TREE:
        return msbv_read_empty(entry_count, &read_ctx->msb->collision_tree_count);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbv_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[40];
    snprintf(next_name, sizeof next_name, "MsbvNextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

static sf_result_t msbv_write_no_entry(sf_binary_writer_t *w, const void *entry,
                                         size_t index, void *ctx) {
    (void)w;
    (void)entry;
    (void)index;
    (void)ctx;
    return SF_ERR_INTERNAL;
}

static sf_result_t msbv_empty_param_write(sf_binary_writer_t *w, const char *name,
                                            int list_index) {
    char next_key[40];
    snprintf(next_key, sizeof next_key, "MsbvNextList%d", list_index);
    return msb_legacy_entry_list_write(w, name, next_key, NULL, 0, 0,
                                       msbv_write_no_entry, NULL);
}

sf_result_t sf_msbv_read_from_memory(sf_msbv_t **out, const uint8_t *data, size_t size,
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

    sf_msbv_t *msb = (sf_msbv_t *)sf_xalloc(alloc, sizeof(*msb));
    if (!msb) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msb, 0, sizeof(*msb));
    msb->alloc = alloc;

    if (layout.list_count != MSBV_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbv_read_ctx_t read_ctx = { msb, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msbv_read_list_cb, &read_ctx, alloc);
        if (rc == SF_OK && read_ctx.index != MSBV_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);
    if (rc != SF_OK) {
        sf_msbv_destroy(msb);
        return rc;
    }
    *out = msb;
    return SF_OK;
}

sf_result_t sf_msbv_write_to_memory(const sf_msbv_t *msb, uint8_t **out_data,
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

    for (int i = 0; i < MSBV_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msbv_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        if (i == MSBV_LIST_MODELS) rc = msbv_model_param_write(writer, msb);
        else if (i == MSBV_LIST_EVENTS) rc = msbv_event_param_write(writer, msb);
        else if (i == MSBV_LIST_REGIONS) rc = msbv_point_param_write(writer, msb);
        else if (i == MSBV_LIST_PARTS) rc = msbv_parts_param_write(writer, msb);
        else rc = msbv_empty_param_write(writer, k_msbv_list_names[i], i);
        if (rc != SF_OK) goto fail;
    }

    rc = msbv_fill_next_param(writer, MSBV_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbv_destroy(sf_msbv_t *msb) {
    if (!msb) return;
    const sf_allocator_t *alloc = msb->alloc;
    msbv_model_param_free(msb->models, msb->model_count, alloc);
    msbv_event_param_free(msb->events, msb->event_count, alloc);
    msbv_point_param_free(msb->regions, msb->region_count, alloc);
    msbv_parts_param_free(msb->parts, msb->part_count, alloc);
    sf_xfree(alloc, msb->models);
    sf_xfree(alloc, msb->events);
    sf_xfree(alloc, msb->regions);
    sf_xfree(alloc, msb->parts);
    sf_xfree(alloc, msb);
}

int32_t sf_msbv_model_count(const sf_msbv_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msbv_event_count(const sf_msbv_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msbv_region_count(const sf_msbv_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbv_part_count(const sf_msbv_t *m)   { return m ? m->part_count : 0; }

const sf_msbv_model_t *sf_msbv_model_at(const sf_msbv_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}
const sf_msbv_event_t *sf_msbv_event_at(const sf_msbv_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}
const sf_msbv_region_t *sf_msbv_region_at(const sf_msbv_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}
const sf_msbv_part_t *sf_msbv_part_at(const sf_msbv_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
