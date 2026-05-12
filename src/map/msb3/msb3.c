/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msb3_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSB3_LIST_MODELS = 0,
    MSB3_LIST_EVENTS,
    MSB3_LIST_REGIONS,
    MSB3_LIST_ROUTES,
    MSB3_LIST_LAYERS,
    MSB3_LIST_PARTS,
    MSB3_LIST_PARTS_POSES,
    MSB3_LIST_BONE_NAMES,
    MSB3_LIST_COUNT,
};

static const char *const k_msb3_list_names[MSB3_LIST_COUNT] = {
    [MSB3_LIST_MODELS] = "MODEL_PARAM_ST",
    [MSB3_LIST_EVENTS] = "EVENT_PARAM_ST",
    [MSB3_LIST_REGIONS] = "POINT_PARAM_ST",
    [MSB3_LIST_ROUTES] = "ROUTE_PARAM_ST",
    [MSB3_LIST_LAYERS] = "LAYER_PARAM_ST",
    [MSB3_LIST_PARTS] = "PARTS_PARAM_ST",
    [MSB3_LIST_PARTS_POSES] = "MAPSTUDIO_PARTS_POSE_ST",
    [MSB3_LIST_BONE_NAMES] = "MAPSTUDIO_BONE_NAME_STRING",
};

typedef struct msb3_read_ctx {
    sf_msb3_t            *msb3;
    const sf_allocator_t *alloc;
    int32_t               index;
} msb3_read_ctx_t;

static sf_result_t msb3_read_empty(int32_t entry_count, int32_t *out_count) {
    if (entry_count != 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (out_count) *out_count = 0;
    return SF_OK;
}

static sf_result_t msb3_read_list_cb(const char *name, int32_t entry_count,
                                     sf_binary_reader_t *r, void *ctx) {
    msb3_read_ctx_t *read_ctx = (msb3_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msb3 || read_ctx->index < 0 ||
        read_ctx->index >= MSB3_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msb3_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSB3_LIST_MODELS:
        return msb3_model_param_read(r, entry_count, read_ctx->msb3, read_ctx->alloc);
    case MSB3_LIST_EVENTS:
        return msb3_event_param_read(r, entry_count, read_ctx->msb3, read_ctx->alloc);
    case MSB3_LIST_REGIONS:
        return msb3_point_param_read(r, entry_count, read_ctx->msb3, read_ctx->alloc);
    case MSB3_LIST_ROUTES:
        return msb3_read_empty(entry_count, &read_ctx->msb3->route_count);
    case MSB3_LIST_LAYERS:
        return msb3_read_empty(entry_count, &read_ctx->msb3->layer_count);
    case MSB3_LIST_PARTS:
        return msb3_parts_param_read(r, entry_count, read_ctx->msb3, read_ctx->alloc);
    case MSB3_LIST_PARTS_POSES:
        return msb3_read_empty(entry_count, &read_ctx->msb3->parts_pose_count);
    case MSB3_LIST_BONE_NAMES:
        return msb3_read_empty(entry_count, &read_ctx->msb3->bone_name_count);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msb3_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "Msb3NextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

static sf_result_t msb3_write_no_entry(sf_binary_writer_t *w, const void *entry,
                                       size_t index, void *ctx) {
    (void)w;
    (void)entry;
    (void)index;
    (void)ctx;
    return SF_ERR_INTERNAL;
}

static sf_result_t msb3_empty_param_write(sf_binary_writer_t *w, const char *name, int list_index) {
    char next_key[32];
    snprintf(next_key, sizeof next_key, "Msb3NextList%d", list_index);
    return msb_legacy_entry_list_write(w, name, next_key, NULL, 0, 0,
                                       msb3_write_no_entry, NULL);
}

sf_result_t sf_msb3_read_from_memory(sf_msb3_t **out, const uint8_t *data, size_t size,
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

    sf_msb3_t *msb3 = (sf_msb3_t *)sf_xalloc(alloc, sizeof(*msb3));
    if (!msb3) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msb3, 0, sizeof(*msb3));
    msb3->alloc = alloc;

    if (layout.list_count != MSB3_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msb3_read_ctx_t read_ctx = { msb3, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msb3_read_list_cb, &read_ctx, alloc);
        if (rc == SF_OK && read_ctx.index != MSB3_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);
    if (rc != SF_OK) {
        sf_msb3_destroy(msb3);
        return rc;
    }
    *out = msb3;
    return SF_OK;
}

sf_result_t sf_msb3_write_to_memory(const sf_msb3_t *msb3, uint8_t **out_data,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msb3 != NULL && out_data != NULL && out_size != NULL);
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

    for (int i = 0; i < MSB3_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msb3_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        if (i == MSB3_LIST_MODELS) rc = msb3_model_param_write(writer, msb3);
        else if (i == MSB3_LIST_EVENTS) rc = msb3_event_param_write(writer, msb3);
        else if (i == MSB3_LIST_REGIONS) rc = msb3_point_param_write(writer, msb3);
        else if (i == MSB3_LIST_PARTS) rc = msb3_parts_param_write(writer, msb3);
        else rc = msb3_empty_param_write(writer, k_msb3_list_names[i], i);
        if (rc != SF_OK) goto fail;
    }

    rc = msb3_fill_next_param(writer, MSB3_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msb3_destroy(sf_msb3_t *msb3) {
    if (!msb3) return;
    const sf_allocator_t *alloc = msb3->alloc;
    msb3_model_param_free(msb3->models, msb3->model_count, alloc);
    msb3_event_param_free(msb3->events, msb3->event_count, alloc);
    msb3_point_param_free(msb3->regions, msb3->region_count, alloc);
    msb3_parts_param_free(msb3->parts, msb3->part_count, alloc);
    sf_xfree(alloc, msb3->models);
    sf_xfree(alloc, msb3->events);
    sf_xfree(alloc, msb3->regions);
    sf_xfree(alloc, msb3->parts);
    sf_xfree(alloc, msb3);
}

int32_t sf_msb3_model_count(const sf_msb3_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msb3_event_count(const sf_msb3_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msb3_region_count(const sf_msb3_t *m) { return m ? m->region_count : 0; }
int32_t sf_msb3_part_count(const sf_msb3_t *m)   { return m ? m->part_count : 0; }

const sf_msb3_model_t *sf_msb3_model_at(const sf_msb3_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}
const sf_msb3_event_t *sf_msb3_event_at(const sf_msb3_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}
const sf_msb3_region_t *sf_msb3_region_at(const sf_msb3_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}
const sf_msb3_part_t *sf_msb3_part_at(const sf_msb3_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
