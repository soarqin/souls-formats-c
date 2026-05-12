/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbfa_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBFA_LIST_MODELS = 0,
    MSBFA_LIST_EVENTS,
    MSBFA_LIST_REGIONS,
    MSBFA_LIST_ROUTES,
    MSBFA_LIST_LAYERS,
    MSBFA_LIST_PARTS,
    MSBFA_LIST_DRAWING_TREE,
    MSBFA_LIST_COLLISION_TREE,
    MSBFA_LIST_COUNT,
};

static const char *const k_msbfa_list_names[MSBFA_LIST_COUNT] = {
    [MSBFA_LIST_MODELS] = "MODEL_PARAM_ST",
    [MSBFA_LIST_EVENTS] = "EVENT_PARAM_ST",
    [MSBFA_LIST_REGIONS] = "POINT_PARAM_ST",
    [MSBFA_LIST_ROUTES] = "ROUTE_PARAM_ST",
    [MSBFA_LIST_LAYERS] = "LAYER_PARAM_ST",
    [MSBFA_LIST_PARTS] = "PARTS_PARAM_ST",
    [MSBFA_LIST_DRAWING_TREE] = "MAPSTUDIO_TREE_ST",
    [MSBFA_LIST_COLLISION_TREE] = "MAPSTUDIO_TREE_ST",
};

typedef struct msbfa_read_ctx {
    sf_msbfa_t         *msb;
    const sf_allocator_t *alloc;
    int32_t              index;
} msbfa_read_ctx_t;

static sf_result_t msbfa_read_empty(int32_t entry_count, int32_t *out_count) {
    if (entry_count != 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (out_count) *out_count = 0;
    return SF_OK;
}

static sf_result_t msbfa_read_list_cb(const char *name, int32_t entry_count,
                                       sf_binary_reader_t *r, void *ctx) {
    msbfa_read_ctx_t *read_ctx = (msbfa_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msb || read_ctx->index < 0 ||
        read_ctx->index >= MSBFA_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msbfa_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSBFA_LIST_MODELS:
        return msbfa_model_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBFA_LIST_EVENTS:
        return msbfa_event_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBFA_LIST_REGIONS:
        return msbfa_point_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBFA_LIST_ROUTES:
        return msbfa_read_empty(entry_count, &read_ctx->msb->route_count);
    case MSBFA_LIST_LAYERS:
        return msbfa_read_empty(entry_count, &read_ctx->msb->layer_count);
    case MSBFA_LIST_PARTS:
        return msbfa_parts_param_read(r, entry_count, read_ctx->msb, read_ctx->alloc);
    case MSBFA_LIST_DRAWING_TREE:
        return msbfa_read_empty(entry_count, &read_ctx->msb->drawing_tree_count);
    case MSBFA_LIST_COLLISION_TREE:
        return msbfa_read_empty(entry_count, &read_ctx->msb->collision_tree_count);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbfa_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[40];
    snprintf(next_name, sizeof next_name, "MsbfaNextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

static sf_result_t msbfa_write_no_entry(sf_binary_writer_t *w, const void *entry,
                                         size_t index, void *ctx) {
    (void)w;
    (void)entry;
    (void)index;
    (void)ctx;
    return SF_ERR_INTERNAL;
}

static sf_result_t msbfa_empty_param_write(sf_binary_writer_t *w, const char *name,
                                            int list_index) {
    char next_key[40];
    snprintf(next_key, sizeof next_key, "MsbfaNextList%d", list_index);
    return msb_legacy_entry_list_write(w, name, next_key, NULL, 0, 0,
                                       msbfa_write_no_entry, NULL);
}

sf_result_t sf_msbfa_read_from_memory(sf_msbfa_t **out, const uint8_t *data, size_t size,
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

    sf_msbfa_t *msb = (sf_msbfa_t *)sf_xalloc(alloc, sizeof(*msb));
    if (!msb) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msb, 0, sizeof(*msb));
    msb->alloc = alloc;

    if (layout.list_count != MSBFA_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbfa_read_ctx_t read_ctx = { msb, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msbfa_read_list_cb, &read_ctx, alloc);
        if (rc == SF_OK && read_ctx.index != MSBFA_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);
    if (rc != SF_OK) {
        sf_msbfa_destroy(msb);
        return rc;
    }
    *out = msb;
    return SF_OK;
}

sf_result_t sf_msbfa_write_to_memory(const sf_msbfa_t *msb, uint8_t **out_data,
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

    for (int i = 0; i < MSBFA_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msbfa_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        if (i == MSBFA_LIST_MODELS) rc = msbfa_model_param_write(writer, msb);
        else if (i == MSBFA_LIST_EVENTS) rc = msbfa_event_param_write(writer, msb);
        else if (i == MSBFA_LIST_REGIONS) rc = msbfa_point_param_write(writer, msb);
        else if (i == MSBFA_LIST_PARTS) rc = msbfa_parts_param_write(writer, msb);
        else rc = msbfa_empty_param_write(writer, k_msbfa_list_names[i], i);
        if (rc != SF_OK) goto fail;
    }

    rc = msbfa_fill_next_param(writer, MSBFA_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbfa_destroy(sf_msbfa_t *msb) {
    if (!msb) return;
    const sf_allocator_t *alloc = msb->alloc;
    msbfa_model_param_free(msb->models, msb->model_count, alloc);
    msbfa_event_param_free(msb->events, msb->event_count, alloc);
    msbfa_point_param_free(msb->regions, msb->region_count, alloc);
    msbfa_parts_param_free(msb->parts, msb->part_count, alloc);
    sf_xfree(alloc, msb->models);
    sf_xfree(alloc, msb->events);
    sf_xfree(alloc, msb->regions);
    sf_xfree(alloc, msb->parts);
    sf_xfree(alloc, msb);
}

int32_t sf_msbfa_model_count(const sf_msbfa_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msbfa_event_count(const sf_msbfa_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msbfa_region_count(const sf_msbfa_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbfa_part_count(const sf_msbfa_t *m)   { return m ? m->part_count : 0; }

const sf_msbfa_model_t *sf_msbfa_model_at(const sf_msbfa_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}
const sf_msbfa_event_t *sf_msbfa_event_at(const sf_msbfa_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}
const sf_msbfa_region_t *sf_msbfa_region_at(const sf_msbfa_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}
const sf_msbfa_part_t *sf_msbfa_part_at(const sf_msbfa_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
