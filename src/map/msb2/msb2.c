/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msb2_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSB2_LIST_MODELS = 0,
    MSB2_LIST_EVENTS,
    MSB2_LIST_REGIONS,
    MSB2_LIST_PARTS,
    MSB2_LIST_PARTS_POSES,
    MSB2_LIST_COUNT,
};

static const char *const k_msb2_list_names[MSB2_LIST_COUNT] = {
    [MSB2_LIST_MODELS] = "MODEL_PARAM_ST",
    [MSB2_LIST_EVENTS] = "EVENT_PARAM_ST",
    [MSB2_LIST_REGIONS] = "POINT_PARAM_ST",
    [MSB2_LIST_PARTS] = "PARTS_PARAM_ST",
    [MSB2_LIST_PARTS_POSES] = "MAPSTUDIO_PARTS_POSE_ST",
};

typedef struct msb2_read_ctx {
    sf_msb2_t            *msb2;
    const sf_allocator_t *alloc;
    int32_t               index;
} msb2_read_ctx_t;

static sf_result_t msb2_read_empty(int32_t entry_count, int32_t *out_count) {
    if (entry_count != 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (out_count) *out_count = 0;
    return SF_OK;
}

static sf_result_t msb2_read_list_cb(const char *name, int32_t entry_count,
                                     sf_binary_reader_t *r, void *ctx) {
    msb2_read_ctx_t *read_ctx = (msb2_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msb2 || read_ctx->index < 0 ||
        read_ctx->index >= MSB2_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msb2_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSB2_LIST_MODELS:
        return msb2_model_param_read(r, entry_count, read_ctx->msb2, read_ctx->alloc);
    case MSB2_LIST_EVENTS:
        return msb2_event_param_read(r, entry_count, read_ctx->msb2, read_ctx->alloc);
    case MSB2_LIST_REGIONS:
        return msb2_point_param_read(r, entry_count, read_ctx->msb2, read_ctx->alloc);
    case MSB2_LIST_PARTS:
        return msb2_parts_param_read(r, entry_count, read_ctx->msb2, read_ctx->alloc);
    case MSB2_LIST_PARTS_POSES:
        return msb2_read_empty(entry_count, &read_ctx->msb2->parts_pose_count);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msb2_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "Msb2NextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

static sf_result_t msb2_write_no_entry(sf_binary_writer_t *w, const void *entry,
                                       size_t index, void *ctx) {
    (void)w;
    (void)entry;
    (void)index;
    (void)ctx;
    return SF_ERR_INTERNAL;
}

static sf_result_t msb2_empty_param_write(sf_binary_writer_t *w, const char *name, int list_index) {
    char next_key[32];
    snprintf(next_key, sizeof next_key, "Msb2NextList%d", list_index);
    return msb_legacy_entry_list_write(w, name, next_key, NULL, 0, 0,
                                       msb2_write_no_entry, NULL);
}

sf_result_t sf_msb2_read_from_memory(sf_msb2_t **out, const uint8_t *data, size_t size,
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

    sf_msb2_t *msb2 = (sf_msb2_t *)sf_xalloc(alloc, sizeof(*msb2));
    if (!msb2) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msb2, 0, sizeof(*msb2));
    msb2->alloc = alloc;

    if (layout.list_count != MSB2_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msb2_read_ctx_t read_ctx = { msb2, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msb2_read_list_cb, &read_ctx, alloc);
        if (rc == SF_OK && read_ctx.index != MSB2_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);
    if (rc != SF_OK) {
        sf_msb2_destroy(msb2);
        return rc;
    }
    *out = msb2;
    return SF_OK;
}

sf_result_t sf_msb2_write_to_memory(const sf_msb2_t *msb2, uint8_t **out_data,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msb2 != NULL && out_data != NULL && out_size != NULL);
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

    for (int i = 0; i < MSB2_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msb2_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        if (i == MSB2_LIST_MODELS) rc = msb2_model_param_write(writer, msb2);
        else if (i == MSB2_LIST_EVENTS) rc = msb2_event_param_write(writer, msb2);
        else if (i == MSB2_LIST_REGIONS) rc = msb2_point_param_write(writer, msb2);
        else if (i == MSB2_LIST_PARTS) rc = msb2_parts_param_write(writer, msb2);
        else rc = msb2_empty_param_write(writer, k_msb2_list_names[i], i);
        if (rc != SF_OK) goto fail;
    }

    rc = msb2_fill_next_param(writer, MSB2_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msb2_destroy(sf_msb2_t *msb2) {
    if (!msb2) return;
    const sf_allocator_t *alloc = msb2->alloc;
    msb2_model_param_free(msb2->models, msb2->model_count, alloc);
    msb2_event_param_free(msb2->events, msb2->event_count, alloc);
    msb2_point_param_free(msb2->regions, msb2->region_count, alloc);
    msb2_parts_param_free(msb2->parts, msb2->part_count, alloc);
    sf_xfree(alloc, msb2->models);
    sf_xfree(alloc, msb2->events);
    sf_xfree(alloc, msb2->regions);
    sf_xfree(alloc, msb2->parts);
    sf_xfree(alloc, msb2);
}

int32_t sf_msb2_model_count(const sf_msb2_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msb2_event_count(const sf_msb2_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msb2_region_count(const sf_msb2_t *m) { return m ? m->region_count : 0; }
int32_t sf_msb2_part_count(const sf_msb2_t *m)   { return m ? m->part_count : 0; }

const sf_msb2_model_t *sf_msb2_model_at(const sf_msb2_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}
const sf_msb2_event_t *sf_msb2_event_at(const sf_msb2_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}
const sf_msb2_region_t *sf_msb2_region_at(const sf_msb2_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}
const sf_msb2_part_t *sf_msb2_part_at(const sf_msb2_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
