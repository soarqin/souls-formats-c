/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Dark Souls 1 MSB root dispatcher.
 *
 * Mirrors the four-list root order in:
 *   SoulsFormats/Formats/MSB/MSB1/MSB1.cs
 */

#include "msb1_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum {
    MSB1_LIST_MODELS = 0,
    MSB1_LIST_EVENTS,
    MSB1_LIST_REGIONS,
    MSB1_LIST_PARTS,
    MSB1_LIST_COUNT,
};

static const char *const k_msb1_list_names[MSB1_LIST_COUNT] = {
    [MSB1_LIST_MODELS]  = "MODEL_PARAM_ST",
    [MSB1_LIST_EVENTS]  = "EVENT_PARAM_ST",
    [MSB1_LIST_REGIONS] = "POINT_PARAM_ST",
    [MSB1_LIST_PARTS]   = "PARTS_PARAM_ST",
};

typedef struct msb1_read_ctx {
    sf_msb1_t            *msb1;
    const sf_allocator_t *alloc;
    int32_t               index;
} msb1_read_ctx_t;

static sf_result_t msb1_read_list_cb(const char *name, int32_t entry_count,
                                     sf_binary_reader_t *r, void *ctx) {
    msb1_read_ctx_t *read_ctx = (msb1_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msb1 || read_ctx->index < 0 ||
        read_ctx->index >= MSB1_LIST_COUNT) {
        return SF_ERR_INTERNAL;
    }

    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msb1_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;

    switch (list_index) {
    case MSB1_LIST_MODELS:
        return msb1_model_param_read(r, entry_count, read_ctx->msb1, read_ctx->alloc);
    case MSB1_LIST_EVENTS:
        return msb1_event_param_read(r, entry_count, read_ctx->msb1, read_ctx->alloc);
    case MSB1_LIST_REGIONS:
        return msb1_point_param_read(r, entry_count, read_ctx->msb1, read_ctx->alloc);
    case MSB1_LIST_PARTS:
        return msb1_parts_param_read(r, entry_count, read_ctx->msb1, read_ctx->alloc);
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t msb1_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "Msb1NextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

sf_result_t sf_msb1_read_from_memory(sf_msb1_t **out, const uint8_t *data, size_t size,
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

    uint32_t probe = 0;
    rc = sf_binary_reader_get_u32(reader, 4, &probe);
    if (rc != SF_OK) {
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return rc;
    }
    sf_binary_reader_set_big_endian(reader, probe > 0xFFFFu);

    msb_legacy_layout_t layout;
    rc = msb_legacy_read_header(reader, &layout, alloc);
    if (rc != SF_OK) {
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return rc;
    }

    sf_msb1_t *msb1 = (sf_msb1_t *)sf_xalloc(alloc, sizeof(*msb1));
    if (!msb1) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msb1, 0, sizeof(*msb1));
    msb1->alloc = alloc;

    if (layout.list_count != MSB1_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msb1_read_ctx_t ctx = { msb1, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msb1_read_list_cb, &ctx, alloc);
        if (rc == SF_OK && ctx.index != MSB1_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);

    if (rc != SF_OK) {
        sf_msb1_destroy(msb1);
        return rc;
    }

    *out = msb1;
    return SF_OK;
}

sf_result_t sf_msb1_write_to_memory(const sf_msb1_t *msb1, uint8_t **out_data,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msb1 != NULL && out_data != NULL && out_size != NULL);
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

    for (int i = 0; i < MSB1_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msb1_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }

        if (i == MSB1_LIST_MODELS) {
            rc = msb1_model_param_write(writer, msb1);
        } else if (i == MSB1_LIST_EVENTS) {
            rc = msb1_event_param_write(writer, msb1);
        } else if (i == MSB1_LIST_REGIONS) {
            rc = msb1_point_param_write(writer, msb1);
        } else {
            rc = msb1_parts_param_write(writer, msb1);
        }
        if (rc != SF_OK) goto fail;
    }

    rc = msb1_fill_next_param(writer, MSB1_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;

    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msb1_destroy(sf_msb1_t *msb1) {
    if (!msb1) return;
    const sf_allocator_t *alloc = msb1->alloc;
    msb1_model_param_free(msb1->models, msb1->model_count, alloc);
    msb1_event_param_free(msb1->events, msb1->event_count, alloc);
    msb1_point_param_free(msb1->regions, msb1->region_count, alloc);
    msb1_parts_param_free(msb1->parts, msb1->part_count, alloc);
    sf_xfree(alloc, msb1->models);
    sf_xfree(alloc, msb1->events);
    sf_xfree(alloc, msb1->regions);
    sf_xfree(alloc, msb1->parts);
    sf_xfree(alloc, msb1);
}

int32_t sf_msb1_model_count(const sf_msb1_t *m)  { return m ? m->model_count : 0; }
int32_t sf_msb1_event_count(const sf_msb1_t *m)  { return m ? m->event_count : 0; }
int32_t sf_msb1_region_count(const sf_msb1_t *m) { return m ? m->region_count : 0; }
int32_t sf_msb1_part_count(const sf_msb1_t *m)   { return m ? m->part_count : 0; }

const sf_msb1_model_t *sf_msb1_model_at(const sf_msb1_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}

const sf_msb1_event_t *sf_msb1_event_at(const sf_msb1_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->event_count) ? &m->events[idx] : NULL;
}

const sf_msb1_region_t *sf_msb1_region_at(const sf_msb1_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->region_count) ? &m->regions[idx] : NULL;
}

const sf_msb1_part_t *sf_msb1_part_at(const sf_msb1_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
