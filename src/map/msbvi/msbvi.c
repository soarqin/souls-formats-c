/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Armored Core VI MSB root dispatcher shell.
 *
 * Mirrors the six-list root order in:
 *   SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs
 *
 * KEY DIFFERENCE from MSBE: the Layers segment is a typed LayerParam
 * (AC6 actually ships layer entries), so the dispatcher routes it to
 * msbvi_layer_param_read/write instead of treating it as an EmptyParam.
 */

#include "msbvi_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_internal.h"
#include "souls_formats/sf_io.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBVI_LIST_MODELS = 0,
    MSBVI_LIST_EVENTS,
    MSBVI_LIST_REGIONS,
    MSBVI_LIST_ROUTES,
    MSBVI_LIST_LAYERS,
    MSBVI_LIST_PARTS,
    MSBVI_LIST_COUNT,
};

typedef struct msbvi_list_spec {
    const char *name;
    int32_t     version;
} msbvi_list_spec_t;

static const msbvi_list_spec_t k_msbvi_lists[MSBVI_LIST_COUNT] = {
    [MSBVI_LIST_MODELS]  = { "MODEL_PARAM_ST", 52 },
    [MSBVI_LIST_EVENTS]  = { "EVENT_PARAM_ST", 52 },
    [MSBVI_LIST_REGIONS] = { "POINT_PARAM_ST", 52 },
    [MSBVI_LIST_ROUTES]  = { "ROUTE_PARAM_ST", 52 },
    [MSBVI_LIST_LAYERS]  = { "LAYER_PARAM_ST", 52 },
    [MSBVI_LIST_PARTS]   = { "PARTS_PARAM_ST", 52 },
};

typedef struct msbvi_read_ctx {
    sf_msbvi_t           *msbvi;
    const sf_allocator_t *alloc;
    int32_t               index;
} msbvi_read_ctx_t;

static sf_result_t msbvi_read_list_cb(const char *name, int32_t entry_count,
                                      sf_binary_reader_t *r, void *ctx) {
    msbvi_read_ctx_t *read_ctx = (msbvi_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msbvi ||
        read_ctx->index < 0 || read_ctx->index >= MSBVI_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }

    int32_t list_index = read_ctx->index++;
    const msbvi_list_spec_t *spec = &k_msbvi_lists[list_index];
    if (!name || strcmp(name, spec->name) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSBVI_LIST_MODELS:
        return msbvi_model_param_read(r, entry_count, read_ctx->msbvi, read_ctx->alloc);
    case MSBVI_LIST_EVENTS:
        return msbvi_event_param_read(r, entry_count, read_ctx->msbvi, read_ctx->alloc);
    case MSBVI_LIST_REGIONS:
        return msbvi_point_param_read(r, entry_count, read_ctx->msbvi, read_ctx->alloc);
    case MSBVI_LIST_ROUTES:
        return msbvi_route_param_read(r, entry_count, read_ctx->msbvi, read_ctx->alloc);
    case MSBVI_LIST_LAYERS:
        return msbvi_layer_param_read(r, entry_count, read_ctx->msbvi, read_ctx->alloc);
    case MSBVI_LIST_PARTS:
        return msbvi_parts_param_read(r, entry_count, read_ctx->msbvi, read_ctx->alloc);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbvi_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "MsbviNextList%d", reserve_id);
    return sf_binary_writer_fill_i64(w, next_name, offset);
}

sf_result_t sf_msbvi_read_from_memory(sf_msbvi_t **out, const uint8_t *data, size_t size,
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

    msb_layout_t layout;
    rc = msb_common_read_header(reader, &layout, alloc);
    if (rc != SF_OK) {
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return rc;
    }

    sf_msbvi_t *msbvi = (sf_msbvi_t *)sf_xalloc(alloc, sizeof(*msbvi));
    if (!msbvi) {
        msb_common_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msbvi, 0, sizeof(*msbvi));
    msbvi->alloc = alloc;

    if (layout.list_count != MSBVI_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbvi_read_ctx_t ctx = { msbvi, alloc, 0 };
        rc = msb_common_iter_lists(reader, &layout, msbvi_read_list_cb, &ctx, alloc);
        if (rc == SF_OK && ctx.index != MSBVI_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_common_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);

    if (rc != SF_OK) {
        sf_msbvi_destroy(msbvi);
        return rc;
    }

    *out = msbvi;
    return SF_OK;
}

sf_result_t sf_msbvi_write_to_memory(const sf_msbvi_t *msbvi, uint8_t **out_data,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msbvi != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_result_t rc = SF_OK;

    sf_ostream_t *stream = NULL;
    rc = sf_ostream_open_memory(&stream, alloc);
    if (rc != SF_OK) return rc;

    sf_binary_writer_t *writer = NULL;
    rc = sf_binary_writer_create(&writer, stream, false, alloc);
    if (rc != SF_OK) {
        sf_ostream_close(stream);
        return rc;
    }

    rc = msb_common_write_header(writer);
    if (rc != SF_OK) goto fail;

    for (int i = 0; i < MSBVI_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msbvi_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        switch (i) {
        case MSBVI_LIST_MODELS:  rc = msbvi_model_param_write(writer, msbvi); break;
        case MSBVI_LIST_EVENTS:  rc = msbvi_event_param_write(writer, msbvi); break;
        case MSBVI_LIST_REGIONS: rc = msbvi_point_param_write(writer, msbvi); break;
        case MSBVI_LIST_ROUTES:  rc = msbvi_route_param_write(writer, msbvi); break;
        case MSBVI_LIST_LAYERS:  rc = msbvi_layer_param_write(writer, msbvi); break;
        case MSBVI_LIST_PARTS:   rc = msbvi_parts_param_write(writer, msbvi); break;
        default:
            rc = SF_ERR_INTERNAL;
            break;
        }
        if (rc != SF_OK) goto fail;
    }

    rc = msbvi_fill_next_param(writer, MSBVI_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;

    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbvi_destroy(sf_msbvi_t *msbvi) {
    if (!msbvi) return;
    const sf_allocator_t *alloc = msbvi->alloc;
    msbvi_model_param_free(msbvi->models, msbvi->model_count, alloc);
    msbvi_event_param_free(msbvi->events, msbvi->event_count, alloc);
    msbvi_point_param_free(msbvi->regions, msbvi->region_count, alloc);
    msbvi_route_param_free(msbvi->routes, msbvi->route_count, alloc);
    msbvi_layer_param_free(msbvi->layers, msbvi->layer_count, alloc);
    msbvi_parts_param_free(msbvi->parts, msbvi->part_count, alloc);
    sf_xfree(alloc, msbvi->models);
    sf_xfree(alloc, msbvi->events);
    sf_xfree(alloc, msbvi->regions);
    sf_xfree(alloc, msbvi->routes);
    sf_xfree(alloc, msbvi->layers);
    sf_xfree(alloc, msbvi->parts);
    sf_xfree(alloc, msbvi);
}

int32_t sf_msbvi_model_count (const sf_msbvi_t *m) { return m ? m->model_count  : 0; }
int32_t sf_msbvi_event_count (const sf_msbvi_t *m) { return m ? m->event_count  : 0; }
int32_t sf_msbvi_region_count(const sf_msbvi_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbvi_part_count  (const sf_msbvi_t *m) { return m ? m->part_count   : 0; }
int32_t sf_msbvi_route_count (const sf_msbvi_t *m) { return m ? m->route_count  : 0; }
int32_t sf_msbvi_layer_count (const sf_msbvi_t *m) { return m ? m->layer_count  : 0; }

const sf_msbvi_model_t *sf_msbvi_model_at(const sf_msbvi_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}

const sf_msbvi_event_t *sf_msbvi_event_at(const sf_msbvi_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}

const sf_msbvi_region_t *sf_msbvi_region_at(const sf_msbvi_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}

const sf_msbvi_part_t *sf_msbvi_part_at(const sf_msbvi_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}

const sf_msbvi_route_t *sf_msbvi_route_at(const sf_msbvi_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->route_count) ? &m->routes[idx] : NULL;
}

const sf_msbvi_layer_t *sf_msbvi_layer_at(const sf_msbvi_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->layer_count) ? &m->layers[idx] : NULL;
}
