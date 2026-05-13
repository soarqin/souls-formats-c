/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — EDGE (Sekiro grapple/hang/hug edges).
 *
 * Mirrors:
 *   SoulsFormats/Formats/EDGE.cs
 */

#include "souls_formats/sf_edge.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct sf_edge {
    sf_vec3_t      v1;
    sf_vec3_t      v2;
    sf_vec3_t      v3;
    float          unk2c;
    int32_t        unk30;
    sf_edge_type_t edge_type;
    uint8_t        variation_id;
    uint8_t        unk36;
};

struct sf_edge_file {
    const sf_allocator_t *alloc;
    int32_t               id;
    sf_edge_t            *edges;
    size_t                edge_count;
    size_t                edge_capacity;
};

void sf_edge_destroy(sf_edge_file_t *edge) {
    if (!edge) return;
    const sf_allocator_t *alloc = edge->alloc;
    sf_xfree(alloc, edge->edges);
    sf_xfree(alloc, edge);
}

sf_result_t sf_edge_create_empty(sf_edge_file_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_edge_file_t *edge = (sf_edge_file_t *)sf_xalloc(alloc, sizeof(*edge));
    if (!edge) return SF_ERR_OOM;
    memset(edge, 0, sizeof(*edge));
    edge->alloc = alloc;
    *out = edge;
    return SF_OK;
}

static sf_result_t edge_reserve_capacity(sf_edge_file_t *edge, size_t new_capacity) {
    if (new_capacity <= edge->edge_capacity) return SF_OK;
    size_t cap = edge->edge_capacity ? edge->edge_capacity * 2 : 8;
    if (cap < new_capacity) cap = new_capacity;
    size_t old_bytes = edge->edge_capacity * sizeof(sf_edge_t);
    size_t new_bytes = cap * sizeof(sf_edge_t);
    sf_edge_t *new_edges = (sf_edge_t *)sf_xrealloc(edge->alloc, edge->edges,
                                                    old_bytes, new_bytes);
    if (!new_edges) return SF_ERR_OOM;
    edge->edges = new_edges;
    edge->edge_capacity = cap;
    return SF_OK;
}

sf_result_t sf_edge_append(sf_edge_file_t *edge, sf_edge_t **out_edge) {
    SF_CHECK_ARG(edge != NULL);
    sf_result_t r = edge_reserve_capacity(edge, edge->edge_count + 1);
    if (r != SF_OK) return r;
    sf_edge_t *slot = &edge->edges[edge->edge_count++];
    memset(slot, 0, sizeof(*slot));
    slot->edge_type = SF_EDGE_TYPE_GRAPPLE;
    slot->unk2c     = 1.0f;
    if (out_edge) *out_edge = slot;
    return SF_OK;
}

int32_t sf_edge_id(const sf_edge_file_t *edge) { return edge ? edge->id : 0; }
void    sf_edge_set_id(sf_edge_file_t *edge, int32_t id) { if (edge) edge->id = id; }

size_t sf_edge_count(const sf_edge_file_t *edge) {
    return edge ? edge->edge_count : 0;
}

const sf_edge_t *sf_edge_get(const sf_edge_file_t *edge, size_t index) {
    if (!edge || index >= edge->edge_count) return NULL;
    return &edge->edges[index];
}

sf_vec3_t sf_edge_v1(const sf_edge_t *edge) {
    sf_vec3_t z = {0};
    return edge ? edge->v1 : z;
}
sf_vec3_t sf_edge_v2(const sf_edge_t *edge) {
    sf_vec3_t z = {0};
    return edge ? edge->v2 : z;
}
sf_vec3_t sf_edge_v3(const sf_edge_t *edge) {
    sf_vec3_t z = {0};
    return edge ? edge->v3 : z;
}
float          sf_edge_unk2c       (const sf_edge_t *edge) { return edge ? edge->unk2c : 0.0f; }
int32_t        sf_edge_unk30       (const sf_edge_t *edge) { return edge ? edge->unk30 : 0; }
sf_edge_type_t sf_edge_edge_type   (const sf_edge_t *edge) {
    return edge ? edge->edge_type : SF_EDGE_TYPE_GRAPPLE;
}
uint8_t        sf_edge_variation_id(const sf_edge_t *edge) { return edge ? edge->variation_id : 0; }
uint8_t        sf_edge_unk36       (const sf_edge_t *edge) { return edge ? edge->unk36 : 0; }

void sf_edge_set_v1          (sf_edge_t *edge, sf_vec3_t v) { if (edge) edge->v1 = v; }
void sf_edge_set_v2          (sf_edge_t *edge, sf_vec3_t v) { if (edge) edge->v2 = v; }
void sf_edge_set_v3          (sf_edge_t *edge, sf_vec3_t v) { if (edge) edge->v3 = v; }
void sf_edge_set_unk2c       (sf_edge_t *edge, float    v) { if (edge) edge->unk2c = v; }
void sf_edge_set_unk30       (sf_edge_t *edge, int32_t  v) { if (edge) edge->unk30 = v; }
void sf_edge_set_edge_type   (sf_edge_t *edge, sf_edge_type_t v) {
    if (edge) edge->edge_type = v;
}
void sf_edge_set_variation_id(sf_edge_t *edge, uint8_t  v) { if (edge) edge->variation_id = v; }
void sf_edge_set_unk36       (sf_edge_t *edge, uint8_t  v) { if (edge) edge->unk36 = v; }

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t edge_read_one(sf_binary_reader_t *br, sf_edge_t *edge) {
    sf_result_t r = sf_binary_reader_read_vec3(br, &edge->v1);
    if (r == SF_OK) r = sf_binary_reader_assert_f32_one(br, 1.0f);
    if (r == SF_OK) r = sf_binary_reader_read_vec3(br, &edge->v2);
    if (r == SF_OK) r = sf_binary_reader_assert_f32_one(br, 1.0f);
    if (r == SF_OK) r = sf_binary_reader_read_vec3(br, &edge->v3);
    if (r == SF_OK) r = sf_binary_reader_read_f32(br, &edge->unk2c);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &edge->unk30);

    uint8_t type_byte = 0;
    if (r == SF_OK) r = sf_binary_reader_read_u8(br, &type_byte);
    edge->edge_type = (sf_edge_type_t)type_byte;

    if (r == SF_OK) r = sf_binary_reader_read_u8(br, &edge->variation_id);
    if (r == SF_OK) r = sf_binary_reader_read_u8(br, &edge->unk36);
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    return r;
}

sf_result_t sf_edge_read_from_memory(sf_edge_file_t **out, const void *data,
                                     size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) {
        sf_istream_close(stream);
        return r;
    }

    sf_edge_file_t *edge = NULL;
    r = sf_edge_create_empty(&edge, alloc);
    if (r != SF_OK) goto cleanup;

    r = sf_binary_reader_assert_i32_one(br, 4);
    if (r != SF_OK) goto cleanup;

    int32_t edge_count = 0;
    r = sf_binary_reader_read_i32(br, &edge_count);
    if (r != SF_OK) goto cleanup;
    if (edge_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }

    r = sf_binary_reader_read_i32(br, &edge->id);
    if (r != SF_OK) goto cleanup;
    r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) goto cleanup;

    r = edge_reserve_capacity(edge, (size_t)edge_count);
    if (r != SF_OK) goto cleanup;

    for (int32_t i = 0; i < edge_count; i++) {
        sf_edge_t *slot = NULL;
        r = sf_edge_append(edge, &slot);
        if (r != SF_OK) goto cleanup;
        r = edge_read_one(br, slot);
        if (r != SF_OK) goto cleanup;
    }

    *out = edge;
    edge = NULL;

cleanup:
    if (edge) sf_edge_destroy(edge);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    return r;
}

/*===========================================================================
 * Write
 *===========================================================================*/

static sf_result_t edge_write_one(sf_binary_writer_t *bw, const sf_edge_t *edge) {
    sf_result_t r = sf_binary_writer_write_vec3(bw, edge->v1);
    if (r == SF_OK) r = sf_binary_writer_write_f32(bw, 1.0f);
    if (r == SF_OK) r = sf_binary_writer_write_vec3(bw, edge->v2);
    if (r == SF_OK) r = sf_binary_writer_write_f32(bw, 1.0f);
    if (r == SF_OK) r = sf_binary_writer_write_vec3(bw, edge->v3);
    if (r == SF_OK) r = sf_binary_writer_write_f32(bw, edge->unk2c);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, edge->unk30);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, (uint8_t)edge->edge_type);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, edge->variation_id);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, edge->unk36);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    return r;
}

sf_result_t sf_edge_write_to_memory(const sf_edge_file_t *edge,
                                    void **out_data, size_t *out_size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(edge != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) {
        sf_ostream_close(stream);
        return r;
    }

    if (edge->edge_count > (size_t)INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }

    r = sf_binary_writer_write_i32(bw, 4);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)edge->edge_count);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, edge->id);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) goto cleanup;

    for (size_t i = 0; i < edge->edge_count; i++) {
        r = edge_write_one(bw, &edge->edges[i]);
        if (r != SF_OK) goto cleanup;
    }

    uint8_t *bytes = NULL;
    size_t   bytes_len = 0;
    r = sf_binary_writer_finish_bytes(bw, &bytes, &bytes_len);
    bw = NULL;
    if (r != SF_OK) goto cleanup;

    *out_data = bytes;
    *out_size = bytes_len;

cleanup:
    if (bw) sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}
