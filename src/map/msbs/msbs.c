/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSB root dispatcher shell.
 *
 * Mirrors the eight-list root order in:
 *   SoulsFormats/Formats/MSB/MSBS/MSBS.cs
 */

#include "msbs_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_internal.h"
#include "souls_formats/sf_io.h"

#include <stdio.h>
#include <string.h>

enum {
    MSBS_LIST_MODELS = 0,
    MSBS_LIST_EVENTS,
    MSBS_LIST_REGIONS,
    MSBS_LIST_ROUTES,
    MSBS_LIST_LAYERS,
    MSBS_LIST_PARTS,
    MSBS_LIST_PARTS_POSES,
    MSBS_LIST_BONE_NAMES,
    MSBS_LIST_COUNT,
};

typedef struct msbs_list_spec {
    const char *name;
    int32_t     version;
} msbs_list_spec_t;

static const msbs_list_spec_t k_msbs_lists[MSBS_LIST_COUNT] = {
    [MSBS_LIST_MODELS]      = { "MODEL_PARAM_ST", 35 },
    [MSBS_LIST_EVENTS]      = { "EVENT_PARAM_ST", 35 },
    [MSBS_LIST_REGIONS]     = { "POINT_PARAM_ST", 35 },
    [MSBS_LIST_ROUTES]      = { "ROUTE_PARAM_ST", 35 },
    [MSBS_LIST_LAYERS]      = { "LAYER_PARAM_ST", 0x23 },
    [MSBS_LIST_PARTS]       = { "PARTS_PARAM_ST", 35 },
    [MSBS_LIST_PARTS_POSES] = { "MAPSTUDIO_PARTS_POSE_ST", 0 },
    [MSBS_LIST_BONE_NAMES]  = { "MAPSTUDIO_BONE_NAME_STRING", 0 },
};

typedef struct msbs_read_ctx {
    sf_msbs_t            *msbs;
    const sf_allocator_t *alloc;
    int32_t               index;
} msbs_read_ctx_t;

static sf_result_t msbs_alloc_entries(void **out, int32_t count, size_t elem_size,
                                      const sf_allocator_t *a) {
    if (!out || elem_size == 0) return SF_ERR_INVALID_ARG;
    *out = NULL;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    if (count == 0) return SF_OK;

    size_t size = (size_t)count * elem_size;
    void *entries = sf_xalloc(a, size);
    if (!entries) return SF_ERR_OOM;
    memset(entries, 0, size);
    *out = entries;
    return SF_OK;
}

sf_result_t msbs_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    (void)r;
    if (!out) return SF_ERR_INVALID_ARG;
    out->event_count = count;
    return msbs_alloc_entries((void **)&out->events, count, sizeof(*out->events), a);
}

sf_result_t msbs_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    (void)r;
    if (!out) return SF_ERR_INVALID_ARG;
    out->region_count = count;
    return msbs_alloc_entries((void **)&out->regions, count, sizeof(*out->regions), a);
}

sf_result_t msbs_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    (void)r;
    if (!out) return SF_ERR_INVALID_ARG;
    out->part_count = count;
    return msbs_alloc_entries((void **)&out->parts, count, sizeof(*out->parts), a);
}

sf_result_t msbs_route_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    (void)r;
    if (!out) return SF_ERR_INVALID_ARG;
    out->route_count = count;
    return msbs_alloc_entries((void **)&out->routes, count, sizeof(*out->routes), a);
}

static sf_result_t msbs_expect_zero_entries(int32_t count) {
    return (count == 0) ? SF_OK : SF_ERR_UNSUPPORTED_VERSION;
}

static sf_result_t msbs_read_list_cb(const char *name, int32_t entry_count,
                                     sf_binary_reader_t *r, void *ctx) {
    msbs_read_ctx_t *read_ctx = (msbs_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msbs || read_ctx->index < 0 || read_ctx->index >= MSBS_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }

    int32_t list_index = read_ctx->index++;
    const msbs_list_spec_t *spec = &k_msbs_lists[list_index];
    if (!name || strcmp(name, spec->name) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSBS_LIST_MODELS:
        return msbs_model_param_read(r, entry_count, read_ctx->msbs, read_ctx->alloc);
    case MSBS_LIST_EVENTS:
        return msbs_event_param_read(r, entry_count, read_ctx->msbs, read_ctx->alloc);
    case MSBS_LIST_REGIONS:
        return msbs_point_param_read(r, entry_count, read_ctx->msbs, read_ctx->alloc);
    case MSBS_LIST_ROUTES:
        return msbs_route_param_read(r, entry_count, read_ctx->msbs, read_ctx->alloc);
    case MSBS_LIST_LAYERS:
    case MSBS_LIST_PARTS_POSES:
    case MSBS_LIST_BONE_NAMES:
        return msbs_expect_zero_entries(entry_count);
    case MSBS_LIST_PARTS:
        return msbs_parts_param_read(r, entry_count, read_ctx->msbs, read_ctx->alloc);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msbs_write_empty_param(sf_binary_writer_t *w, const char *name,
                                          int32_t version, int reserve_id) {
    if (!w || !name) return SF_ERR_INVALID_ARG;

    char next_name[32];
    char name_offset_name[32];
    snprintf(next_name, sizeof next_name, "MsbsNextList%d", reserve_id);
    snprintf(name_offset_name, sizeof name_offset_name, "MsbsNameOff%d", reserve_id);

    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, version); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 1);       if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, name_offset_name); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, next_name);        if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, name_offset_name, sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, name, true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t msbs_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "MsbsNextList%d", reserve_id);
    return sf_binary_writer_fill_i64(w, next_name, offset);
}

static sf_result_t msbs_write_current_counts_are_supported(const sf_msbs_t *msbs) {
    if (!msbs) return SF_ERR_INVALID_ARG;
    if (msbs->event_count != 0 || msbs->region_count != 0 || msbs->route_count != 0 ||
        msbs->part_count != 0) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }
    return SF_OK;
}

sf_result_t msbs_event_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    (void)w;
    return (!msbs || msbs->event_count != 0) ? SF_ERR_UNSUPPORTED_VERSION : SF_OK;
}

sf_result_t msbs_point_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    (void)w;
    return (!msbs || msbs->region_count != 0) ? SF_ERR_UNSUPPORTED_VERSION : SF_OK;
}

sf_result_t msbs_parts_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    (void)w;
    return (!msbs || msbs->part_count != 0) ? SF_ERR_UNSUPPORTED_VERSION : SF_OK;
}

sf_result_t msbs_route_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    (void)w;
    return (!msbs || msbs->route_count != 0) ? SF_ERR_UNSUPPORTED_VERSION : SF_OK;
}

sf_result_t sf_msbs_read_from_memory(sf_msbs_t **out, const uint8_t *data, size_t size,
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

    sf_msbs_t *msbs = (sf_msbs_t *)sf_xalloc(alloc, sizeof(*msbs));
    if (!msbs) {
        msb_common_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msbs, 0, sizeof(*msbs));
    msbs->alloc = alloc;

    if (layout.list_count != MSBS_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbs_read_ctx_t ctx = { msbs, alloc, 0 };
        rc = msb_common_iter_lists(reader, &layout, msbs_read_list_cb, &ctx, alloc);
        if (rc == SF_OK && ctx.index != MSBS_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_common_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);

    if (rc != SF_OK) {
        sf_msbs_destroy(msbs);
        return rc;
    }

    *out = msbs;
    return SF_OK;
}

sf_result_t sf_msbs_write_to_memory(const sf_msbs_t *msbs, uint8_t **out_data,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msbs != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_result_t rc = msbs_write_current_counts_are_supported(msbs);
    if (rc != SF_OK) return rc;

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

    for (int i = 0; i < MSBS_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msbs_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        if (i == MSBS_LIST_MODELS) {
            rc = msbs_model_param_write(writer, msbs);
        } else {
            rc = msbs_write_empty_param(writer, k_msbs_lists[i].name, k_msbs_lists[i].version, i);
        }
        if (rc != SF_OK) goto fail;
    }

    rc = msbs_fill_next_param(writer, MSBS_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;

    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbs_destroy(sf_msbs_t *msbs) {
    if (!msbs) return;
    const sf_allocator_t *alloc = msbs->alloc;
    msbs_model_param_free(msbs->models, msbs->model_count, alloc);
    sf_xfree(alloc, msbs->models);
    sf_xfree(alloc, msbs->events);
    sf_xfree(alloc, msbs->regions);
    sf_xfree(alloc, msbs->routes);
    sf_xfree(alloc, msbs->parts);
    sf_xfree(alloc, msbs);
}

int32_t sf_msbs_model_count(const sf_msbs_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msbs_event_count(const sf_msbs_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msbs_region_count(const sf_msbs_t *m) { return m ? m->region_count : 0; }
int32_t sf_msbs_part_count(const sf_msbs_t *m)   { return m ? m->part_count : 0; }
int32_t sf_msbs_route_count(const sf_msbs_t *m)  { return m ? m->route_count : 0; }

const sf_msbs_model_t *sf_msbs_model_at(const sf_msbs_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}

const sf_msbs_event_t *sf_msbs_event_at(const sf_msbs_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}

const sf_msbs_region_t *sf_msbs_region_at(const sf_msbs_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}

const sf_msbs_part_t *sf_msbs_part_at(const sf_msbs_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}

const sf_msbs_route_t *sf_msbs_route_at(const sf_msbs_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->route_count) ? &m->routes[idx] : NULL;
}
