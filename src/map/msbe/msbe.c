/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Elden Ring MSB root dispatcher shell.
 *
 * Mirrors the six-list root order in:
 *   SoulsFormats/Formats/MSB/MSBE/MSBE.cs
 */

#include "msbe_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_internal.h"
#include "souls_formats/sf_io.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBE_LIST_MODELS = 0,
    MSBE_LIST_EVENTS,
    MSBE_LIST_REGIONS,
    MSBE_LIST_ROUTES,
    MSBE_LIST_LAYERS,
    MSBE_LIST_PARTS,
    MSBE_LIST_COUNT,
};

typedef struct msbe_list_spec {
    const char *name;
    int32_t     version;
} msbe_list_spec_t;

static const msbe_list_spec_t k_msbe_lists[MSBE_LIST_COUNT] = {
    [MSBE_LIST_MODELS]  = { "MODEL_PARAM_ST", 73 },
    [MSBE_LIST_EVENTS]  = { "EVENT_PARAM_ST", 73 },
    [MSBE_LIST_REGIONS] = { "POINT_PARAM_ST", 73 },
    [MSBE_LIST_ROUTES]  = { "ROUTE_PARAM_ST", 73 },
    [MSBE_LIST_LAYERS]  = { "LAYER_PARAM_ST", 0x49 },
    [MSBE_LIST_PARTS]   = { "PARTS_PARAM_ST", 73 },
};

typedef struct msbe_read_ctx {
    sf_msbe_t            *msbe;
    const sf_allocator_t *alloc;
    int32_t               index;
} msbe_read_ctx_t;

static sf_result_t msbe_expect_zero_entries(int32_t count) {
    return (count == 0) ? SF_OK : SF_ERR_UNSUPPORTED_VERSION;
}

static sf_result_t msbe_read_list_cb(const char *name, int32_t entry_count,
                                     sf_binary_reader_t *r, void *ctx) {
    msbe_read_ctx_t *read_ctx = (msbe_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msbe || read_ctx->index < 0 || read_ctx->index >= MSBE_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }

    int32_t list_index = read_ctx->index++;
    const msbe_list_spec_t *spec = &k_msbe_lists[list_index];
    if (!name || strcmp(name, spec->name) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSBE_LIST_MODELS:
        return msbe_model_param_read(r, entry_count, read_ctx->msbe, read_ctx->alloc);
    case MSBE_LIST_EVENTS:
        return msbe_event_param_read(r, entry_count, read_ctx->msbe, read_ctx->alloc);
    case MSBE_LIST_REGIONS:
        return msbe_point_param_read(r, entry_count, read_ctx->msbe, read_ctx->alloc);
    case MSBE_LIST_ROUTES:
        return msbe_route_param_read(r, entry_count, read_ctx->msbe, read_ctx->alloc);
    case MSBE_LIST_LAYERS:
        return msbe_expect_zero_entries(entry_count);
    case MSBE_LIST_PARTS:
        return msbe_parts_param_read(r, entry_count, read_ctx->msbe, read_ctx->alloc);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbe_write_param_header(sf_binary_writer_t *w, const char *name,
                                           int32_t version, int32_t entry_count, int reserve_id) {
    if (!w || !name) return SF_ERR_INVALID_ARG;

    char next_name[32];
    char name_offset_name[32];
    snprintf(next_name, sizeof next_name, "MsbeNextList%d", reserve_id);
    snprintf(name_offset_name, sizeof name_offset_name, "MsbeNameOff%d", reserve_id);

    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, version);         if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, entry_count + 1); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_offset_name), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, next_name), return rc);

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_offset_name, sf_binary_writer_position(w)), return rc);
    rc = sf_binary_writer_write_utf16(w, name, true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t msbe_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "MsbeNextList%d", reserve_id);
    return sf_binary_writer_fill_i64(w, next_name, offset);
}

sf_result_t sf_msbe_read_from_memory(sf_msbe_t **out, const uint8_t *data, size_t size,
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

    sf_msbe_t *msbe = (sf_msbe_t *)sf_xalloc(alloc, sizeof(*msbe));
    if (!msbe) {
        msb_common_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msbe, 0, sizeof(*msbe));
    msbe->alloc = alloc;

    if (layout.list_count != MSBE_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbe_read_ctx_t ctx = { msbe, alloc, 0 };
        rc = msb_common_iter_lists(reader, &layout, msbe_read_list_cb, &ctx, alloc);
        if (rc == SF_OK && ctx.index != MSBE_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_common_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);

    if (rc != SF_OK) {
        sf_msbe_destroy(msbe);
        return rc;
    }

    *out = msbe;
    return SF_OK;
}

sf_result_t sf_msbe_write_to_memory(const sf_msbe_t *msbe, uint8_t **out_data,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msbe != NULL && out_data != NULL && out_size != NULL);
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

    for (int i = 0; i < MSBE_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msbe_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        switch (i) {
        case MSBE_LIST_MODELS:  rc = msbe_model_param_write(writer, msbe); break;
        case MSBE_LIST_EVENTS:  rc = msbe_event_param_write(writer, msbe); break;
        case MSBE_LIST_REGIONS: rc = msbe_point_param_write(writer, msbe); break;
        case MSBE_LIST_ROUTES:  rc = msbe_route_param_write(writer, msbe); break;
        case MSBE_LIST_LAYERS:
            rc = msbe_write_param_header(writer, k_msbe_lists[i].name, k_msbe_lists[i].version, 0, i);
            break;
        case MSBE_LIST_PARTS:   rc = msbe_parts_param_write(writer, msbe); break;
        default: rc = SF_ERR_INTERNAL; break;
        }
        if (rc != SF_OK) goto fail;
    }

    rc = msbe_fill_next_param(writer, MSBE_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;

    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbe_destroy(sf_msbe_t *msbe) {
    if (!msbe) return;
    const sf_allocator_t *alloc = msbe->alloc;
    msbe_model_param_free(msbe->models, msbe->model_count, alloc);
    msbe_event_param_free(msbe->events, msbe->event_count, alloc);
    msbe_point_param_free(msbe->regions, msbe->region_count, alloc);
    msbe_route_param_free(msbe->routes, msbe->route_count, alloc);
    msbe_parts_param_free(msbe->parts, msbe->part_count, alloc);
    sf_xfree(alloc, msbe->models);
    sf_xfree(alloc, msbe->events);
    sf_xfree(alloc, msbe->regions);
    sf_xfree(alloc, msbe->routes);
    sf_xfree(alloc, msbe->parts);
    sf_xfree(alloc, msbe);
}

int32_t sf_msbe_model_count(const sf_msbe_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msbe_event_count(const sf_msbe_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msbe_region_count(const sf_msbe_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbe_part_count(const sf_msbe_t *m)   { return m ? m->part_count : 0; }
int32_t sf_msbe_route_count(const sf_msbe_t *m)  { return m ? m->route_count : 0; }

const sf_msbe_model_t *sf_msbe_model_at(const sf_msbe_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}

const sf_msbe_event_t *sf_msbe_event_at(const sf_msbe_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}

const sf_msbe_region_t *sf_msbe_region_at(const sf_msbe_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}

const sf_msbe_part_t *sf_msbe_part_at(const sf_msbe_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}

const sf_msbe_route_t *sf_msbe_route_at(const sf_msbe_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->route_count) ? &m->routes[idx] : NULL;
}
