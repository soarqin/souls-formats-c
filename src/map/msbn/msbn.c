/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbn_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

enum { MSBN_LIST_MODELS = 0, MSBN_LIST_PARTS, MSBN_LIST_COUNT };

static const char *const k_msbn_list_names[MSBN_LIST_COUNT] = {
    [MSBN_LIST_MODELS] = "MODEL_PARAM_ST",
    [MSBN_LIST_PARTS] = "PARTS_PARAM_ST",
};

typedef struct msbn_read_ctx {
    sf_msbn_t            *msbn;
    const sf_allocator_t *alloc;
    int32_t               index;
} msbn_read_ctx_t;

static sf_result_t msbn_read_list_cb(const char *name, int32_t entry_count,
                                     sf_binary_reader_t *r, void *ctx) {
    msbn_read_ctx_t *read_ctx = (msbn_read_ctx_t *)ctx;
    if (!read_ctx || !read_ctx->msbn || read_ctx->index < 0 ||
        read_ctx->index >= MSBN_LIST_COUNT) return SF_ERR_INTERNAL;
    int32_t list_index = read_ctx->index++;
    if (!name || strcmp(name, k_msbn_list_names[list_index]) != 0) return SF_ERR_BAD_MAGIC;
    if (list_index == MSBN_LIST_MODELS) {
        return msbn_model_param_read(r, entry_count, read_ctx->msbn, read_ctx->alloc);
    }
    return msbn_parts_param_read(r, entry_count, read_ctx->msbn, read_ctx->alloc);
}

static sf_result_t msbn_fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "MsbnNextList%d", reserve_id);
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, next_name, (int32_t)offset);
}

sf_result_t sf_msbn_read_from_memory(sf_msbn_t **out, const uint8_t *data, size_t size,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t rc = sf_istream_open_memory(&stream, data, size, alloc);
    if (rc != SF_OK) return rc;
    sf_binary_reader_t *reader = NULL;
    rc = sf_binary_reader_create(&reader, stream, false, alloc);
    if (rc != SF_OK) { sf_istream_close(stream); return rc; }

    msb_legacy_layout_t layout;
    rc = msb_legacy_read_header(reader, &layout, alloc);
    if (rc != SF_OK) { sf_binary_reader_destroy(reader); sf_istream_close(stream); return rc; }

    sf_msbn_t *msbn = (sf_msbn_t *)sf_xalloc(alloc, sizeof(*msbn));
    if (!msbn) {
        msb_legacy_free_layout(&layout, alloc);
        sf_binary_reader_destroy(reader);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(msbn, 0, sizeof(*msbn));
    msbn->alloc = alloc;

    if (layout.list_count != MSBN_LIST_COUNT) {
        rc = SF_ERR_UNSUPPORTED_VERSION;
    } else {
        msbn_read_ctx_t read_ctx = { msbn, alloc, 0 };
        rc = msb_legacy_iter_lists(reader, &layout, msbn_read_list_cb, &read_ctx, alloc);
        if (rc == SF_OK && read_ctx.index != MSBN_LIST_COUNT) rc = SF_ERR_INTERNAL;
    }

    msb_legacy_free_layout(&layout, alloc);
    sf_binary_reader_destroy(reader);
    sf_istream_close(stream);
    if (rc != SF_OK) { sf_msbn_destroy(msbn); return rc; }
    *out = msbn;
    return SF_OK;
}

sf_result_t sf_msbn_write_to_memory(const sf_msbn_t *msbn, uint8_t **out_data,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(msbn != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);
    sf_ostream_t *stream = NULL;
    sf_result_t rc = sf_ostream_open_memory(&stream, alloc);
    if (rc != SF_OK) return rc;
    sf_binary_writer_t *writer = NULL;
    rc = sf_binary_writer_create(&writer, stream, false, alloc);
    if (rc != SF_OK) { sf_ostream_close(stream); return rc; }

    for (int i = 0; i < MSBN_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(writer);
        if (i > 0) {
            rc = msbn_fill_next_param(writer, i - 1, list_start);
            if (rc != SF_OK) goto fail;
        }
        rc = (i == MSBN_LIST_MODELS) ? msbn_model_param_write(writer, msbn)
                                     : msbn_parts_param_write(writer, msbn);
        if (rc != SF_OK) goto fail;
    }
    rc = msbn_fill_next_param(writer, MSBN_LIST_COUNT - 1, 0);
    if (rc != SF_OK) goto fail;
    rc = sf_binary_writer_finish_bytes(writer, out_data, out_size);
    sf_ostream_close(stream);
    return rc;

fail:
    sf_binary_writer_destroy(writer);
    sf_ostream_close(stream);
    return rc;
}

void sf_msbn_destroy(sf_msbn_t *msbn) {
    if (!msbn) return;
    const sf_allocator_t *alloc = msbn->alloc;
    msbn_model_param_free(msbn->models, msbn->model_count, alloc);
    msbn_parts_param_free(msbn->parts, msbn->part_count, alloc);
    sf_xfree(alloc, msbn->models);
    sf_xfree(alloc, msbn->parts);
    sf_xfree(alloc, msbn);
}

int32_t sf_msbn_model_count(const sf_msbn_t *m) { return m ? m->model_count : 0; }
int32_t sf_msbn_part_count(const sf_msbn_t *m)  { return m ? m->part_count : 0; }

const sf_msbn_model_t *sf_msbn_model_at(const sf_msbn_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->model_count) ? &m->models[idx] : NULL;
}

const sf_msbn_part_t *sf_msbn_part_at(const sf_msbn_t *m, int32_t idx) {
    return (m && idx >= 0 && idx < m->part_count) ? &m->parts[idx] : NULL;
}
