/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MCG navigation graph (DeS/DS1).
 *
 * Mirrors:
 *   SoulsFormats/Formats/MCG.cs
 */

#include "souls_formats/sf_mcg.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct sf_mcg_node {
    sf_vec3_t position;
    int32_t  *connected_node_indices;
    size_t    connected_node_count;
    size_t    connected_node_capacity;
    int32_t  *connected_edge_indices;
    size_t    connected_edge_count;
    size_t    connected_edge_capacity;
    int32_t   unk18;
    int32_t   unk1c;
};

struct sf_mcg_edge {
    int32_t  node_index_a;
    int32_t  node_index_b;
    int32_t  mcp_room_index;
    uint32_t map_id;
    float    unk20;
    int32_t *unk_indices_a;
    size_t   unk_indices_a_count;
    size_t   unk_indices_a_capacity;
    int32_t *unk_indices_b;
    size_t   unk_indices_b_count;
    size_t   unk_indices_b_capacity;
};

struct sf_mcg {
    const sf_allocator_t *alloc;
    bool                  big_endian;
    int32_t               unk04;
    int32_t               unk18;
    int32_t               unk1c;
    sf_mcg_node_t        *nodes;
    size_t                node_count;
    size_t                node_capacity;
    sf_mcg_edge_t        *edges;
    size_t                edge_count;
    size_t                edge_capacity;
};

static void node_release(sf_mcg_node_t *n, const sf_allocator_t *a) {
    if (!n) return;
    sf_xfree(a, n->connected_node_indices);
    sf_xfree(a, n->connected_edge_indices);
    memset(n, 0, sizeof(*n));
}

static void edge_release(sf_mcg_edge_t *e, const sf_allocator_t *a) {
    if (!e) return;
    sf_xfree(a, e->unk_indices_a);
    sf_xfree(a, e->unk_indices_b);
    memset(e, 0, sizeof(*e));
}

void sf_mcg_destroy(sf_mcg_t *mcg) {
    if (!mcg) return;
    const sf_allocator_t *a = mcg->alloc;
    for (size_t i = 0; i < mcg->node_count; i++) node_release(&mcg->nodes[i], a);
    for (size_t i = 0; i < mcg->edge_count; i++) edge_release(&mcg->edges[i], a);
    sf_xfree(a, mcg->nodes);
    sf_xfree(a, mcg->edges);
    sf_xfree(a, mcg);
}

sf_result_t sf_mcg_create_empty(sf_mcg_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_mcg_t *mcg = (sf_mcg_t *)sf_xalloc(alloc, sizeof(*mcg));
    if (!mcg) return SF_ERR_OOM;
    memset(mcg, 0, sizeof(*mcg));
    mcg->alloc = alloc;
    *out = mcg;
    return SF_OK;
}

static sf_result_t reserve_nodes(sf_mcg_t *mcg, size_t needed) {
    if (needed <= mcg->node_capacity) return SF_OK;
    size_t cap = mcg->node_capacity ? mcg->node_capacity * 2 : 8;
    if (cap < needed) cap = needed;
    sf_mcg_node_t *arr = (sf_mcg_node_t *)sf_xrealloc(
        mcg->alloc, mcg->nodes,
        mcg->node_capacity * sizeof(sf_mcg_node_t),
        cap * sizeof(sf_mcg_node_t));
    if (!arr) return SF_ERR_OOM;
    mcg->nodes = arr;
    mcg->node_capacity = cap;
    return SF_OK;
}

static sf_result_t reserve_edges(sf_mcg_t *mcg, size_t needed) {
    if (needed <= mcg->edge_capacity) return SF_OK;
    size_t cap = mcg->edge_capacity ? mcg->edge_capacity * 2 : 8;
    if (cap < needed) cap = needed;
    sf_mcg_edge_t *arr = (sf_mcg_edge_t *)sf_xrealloc(
        mcg->alloc, mcg->edges,
        mcg->edge_capacity * sizeof(sf_mcg_edge_t),
        cap * sizeof(sf_mcg_edge_t));
    if (!arr) return SF_ERR_OOM;
    mcg->edges = arr;
    mcg->edge_capacity = cap;
    return SF_OK;
}

sf_result_t sf_mcg_append_node(sf_mcg_t *mcg, sf_mcg_node_t **out) {
    SF_CHECK_ARG(mcg != NULL);
    sf_result_t r = reserve_nodes(mcg, mcg->node_count + 1);
    if (r != SF_OK) return r;
    sf_mcg_node_t *n = &mcg->nodes[mcg->node_count++];
    memset(n, 0, sizeof(*n));
    n->unk18 = -1;
    if (out) *out = n;
    return SF_OK;
}

sf_result_t sf_mcg_append_edge(sf_mcg_t *mcg, sf_mcg_edge_t **out) {
    SF_CHECK_ARG(mcg != NULL);
    sf_result_t r = reserve_edges(mcg, mcg->edge_count + 1);
    if (r != SF_OK) return r;
    sf_mcg_edge_t *e = &mcg->edges[mcg->edge_count++];
    memset(e, 0, sizeof(*e));
    if (out) *out = e;
    return SF_OK;
}

bool    sf_mcg_big_endian(const sf_mcg_t *m) { return m ? m->big_endian : false; }
void    sf_mcg_set_big_endian(sf_mcg_t *m, bool be) { if (m) m->big_endian = be; }
int32_t sf_mcg_unk04(const sf_mcg_t *m) { return m ? m->unk04 : 0; }
void    sf_mcg_set_unk04(sf_mcg_t *m, int32_t v) { if (m) m->unk04 = v; }
int32_t sf_mcg_unk18(const sf_mcg_t *m) { return m ? m->unk18 : 0; }
void    sf_mcg_set_unk18(sf_mcg_t *m, int32_t v) { if (m) m->unk18 = v; }
int32_t sf_mcg_unk1c(const sf_mcg_t *m) { return m ? m->unk1c : 0; }
void    sf_mcg_set_unk1c(sf_mcg_t *m, int32_t v) { if (m) m->unk1c = v; }

size_t sf_mcg_node_count(const sf_mcg_t *m) { return m ? m->node_count : 0; }
const sf_mcg_node_t *sf_mcg_node(const sf_mcg_t *m, size_t i) {
    if (!m || i >= m->node_count) return NULL;
    return &m->nodes[i];
}
sf_mcg_node_t *sf_mcg_node_mut(sf_mcg_t *m, size_t i) {
    if (!m || i >= m->node_count) return NULL;
    return &m->nodes[i];
}

size_t sf_mcg_edge_count(const sf_mcg_t *m) { return m ? m->edge_count : 0; }
const sf_mcg_edge_t *sf_mcg_edge(const sf_mcg_t *m, size_t i) {
    if (!m || i >= m->edge_count) return NULL;
    return &m->edges[i];
}
sf_mcg_edge_t *sf_mcg_edge_mut(sf_mcg_t *m, size_t i) {
    if (!m || i >= m->edge_count) return NULL;
    return &m->edges[i];
}

sf_vec3_t sf_mcg_node_position(const sf_mcg_node_t *n) {
    sf_vec3_t z = {0};
    return n ? n->position : z;
}
size_t  sf_mcg_node_connected_node_count(const sf_mcg_node_t *n) {
    return n ? n->connected_node_count : 0;
}
int32_t sf_mcg_node_connected_node_index(const sf_mcg_node_t *n, size_t i) {
    if (!n || i >= n->connected_node_count) return 0;
    return n->connected_node_indices[i];
}
size_t  sf_mcg_node_connected_edge_count(const sf_mcg_node_t *n) {
    return n ? n->connected_edge_count : 0;
}
int32_t sf_mcg_node_connected_edge_index(const sf_mcg_node_t *n, size_t i) {
    if (!n || i >= n->connected_edge_count) return 0;
    return n->connected_edge_indices[i];
}
int32_t sf_mcg_node_unk18(const sf_mcg_node_t *n) { return n ? n->unk18 : -1; }
int32_t sf_mcg_node_unk1c(const sf_mcg_node_t *n) { return n ? n->unk1c : 0; }

void sf_mcg_node_set_position(sf_mcg_node_t *n, sf_vec3_t v) { if (n) n->position = v; }
void sf_mcg_node_set_unk18   (sf_mcg_node_t *n, int32_t v) { if (n) n->unk18 = v; }
void sf_mcg_node_set_unk1c   (sf_mcg_node_t *n, int32_t v) { if (n) n->unk1c = v; }

static sf_result_t append_i32(int32_t **arr, size_t *count, size_t *cap, int32_t v) {
    if (*count + 1 > *cap) {
        size_t new_cap = *cap ? *cap * 2 : 4;
        if (new_cap < *count + 1) new_cap = *count + 1;
        int32_t *new_arr = (int32_t *)sf_xrealloc(NULL, *arr,
                                                   *cap * sizeof(int32_t),
                                                   new_cap * sizeof(int32_t));
        if (!new_arr) return SF_ERR_OOM;
        *arr = new_arr;
        *cap = new_cap;
    }
    (*arr)[(*count)++] = v;
    return SF_OK;
}

sf_result_t sf_mcg_node_append_connected_node_index(sf_mcg_node_t *n, int32_t v) {
    SF_CHECK_ARG(n != NULL);
    return append_i32(&n->connected_node_indices, &n->connected_node_count,
                      &n->connected_node_capacity, v);
}
sf_result_t sf_mcg_node_append_connected_edge_index(sf_mcg_node_t *n, int32_t v) {
    SF_CHECK_ARG(n != NULL);
    return append_i32(&n->connected_edge_indices, &n->connected_edge_count,
                      &n->connected_edge_capacity, v);
}

int32_t  sf_mcg_edge_node_index_a  (const sf_mcg_edge_t *e) { return e ? e->node_index_a : 0; }
int32_t  sf_mcg_edge_node_index_b  (const sf_mcg_edge_t *e) { return e ? e->node_index_b : 0; }
int32_t  sf_mcg_edge_mcp_room_index(const sf_mcg_edge_t *e) { return e ? e->mcp_room_index : 0; }
uint32_t sf_mcg_edge_map_id        (const sf_mcg_edge_t *e) { return e ? e->map_id : 0; }
float    sf_mcg_edge_unk20         (const sf_mcg_edge_t *e) { return e ? e->unk20 : 0.0f; }
size_t   sf_mcg_edge_unk_indices_a_count(const sf_mcg_edge_t *e) {
    return e ? e->unk_indices_a_count : 0;
}
int32_t  sf_mcg_edge_unk_indices_a_index(const sf_mcg_edge_t *e, size_t i) {
    if (!e || i >= e->unk_indices_a_count) return 0;
    return e->unk_indices_a[i];
}
size_t   sf_mcg_edge_unk_indices_b_count(const sf_mcg_edge_t *e) {
    return e ? e->unk_indices_b_count : 0;
}
int32_t  sf_mcg_edge_unk_indices_b_index(const sf_mcg_edge_t *e, size_t i) {
    if (!e || i >= e->unk_indices_b_count) return 0;
    return e->unk_indices_b[i];
}

void sf_mcg_edge_set_node_index_a  (sf_mcg_edge_t *e, int32_t v) { if (e) e->node_index_a = v; }
void sf_mcg_edge_set_node_index_b  (sf_mcg_edge_t *e, int32_t v) { if (e) e->node_index_b = v; }
void sf_mcg_edge_set_mcp_room_index(sf_mcg_edge_t *e, int32_t v) { if (e) e->mcp_room_index = v; }
void sf_mcg_edge_set_map_id        (sf_mcg_edge_t *e, uint32_t v) { if (e) e->map_id = v; }
void sf_mcg_edge_set_unk20         (sf_mcg_edge_t *e, float v) { if (e) e->unk20 = v; }

sf_result_t sf_mcg_edge_append_unk_indices_a(sf_mcg_edge_t *e, int32_t v) {
    SF_CHECK_ARG(e != NULL);
    return append_i32(&e->unk_indices_a, &e->unk_indices_a_count, &e->unk_indices_a_capacity, v);
}
sf_result_t sf_mcg_edge_append_unk_indices_b(sf_mcg_edge_t *e, int32_t v) {
    SF_CHECK_ARG(e != NULL);
    return append_i32(&e->unk_indices_b, &e->unk_indices_b_count, &e->unk_indices_b_capacity, v);
}

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t read_node(sf_binary_reader_t *br, sf_mcg_node_t *n,
                             const sf_allocator_t *alloc) {
    int32_t connection_count = 0;
    int32_t node_indices_offset = 0;
    int32_t edge_indices_offset = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &connection_count);
    if (r == SF_OK) r = sf_binary_reader_read_vec3(br, &n->position);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &node_indices_offset);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &edge_indices_offset);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &n->unk18);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &n->unk1c);
    if (r != SF_OK) return r;
    if (connection_count < 0) return SF_ERR_OUT_OF_RANGE;

    if (connection_count > 0) {
        size_t bytes = (size_t)connection_count * sizeof(int32_t);
        int32_t *narr = (int32_t *)sf_xalloc(alloc, bytes);
        int32_t *earr = (int32_t *)sf_xalloc(alloc, bytes);
        if (!narr || !earr) {
            sf_xfree(alloc, narr); sf_xfree(alloc, earr);
            return SF_ERR_OOM;
        }
        r = sf_binary_reader_get_i32s(br, node_indices_offset, (size_t)connection_count, narr);
        if (r == SF_OK)
            r = sf_binary_reader_get_i32s(br, edge_indices_offset, (size_t)connection_count, earr);
        if (r != SF_OK) {
            sf_xfree(alloc, narr); sf_xfree(alloc, earr);
            return r;
        }
        n->connected_node_indices = narr;
        n->connected_node_count = (size_t)connection_count;
        n->connected_node_capacity = (size_t)connection_count;
        n->connected_edge_indices = earr;
        n->connected_edge_count = (size_t)connection_count;
        n->connected_edge_capacity = (size_t)connection_count;
    }
    return SF_OK;
}

static sf_result_t read_edge(sf_binary_reader_t *br, sf_mcg_edge_t *e,
                             const sf_allocator_t *alloc) {
    int32_t count_a = 0, offset_a = 0, count_b = 0, offset_b = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &e->node_index_a);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &count_a);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &offset_a);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &e->node_index_b);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &count_b);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &offset_b);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &e->mcp_room_index);
    if (r == SF_OK) r = sf_binary_reader_read_u32(br, &e->map_id);
    if (r == SF_OK) r = sf_binary_reader_read_f32(br, &e->unk20);
    if (r != SF_OK) return r;
    if (count_a < 0 || count_b < 0) return SF_ERR_OUT_OF_RANGE;

    if (count_a > 0) {
        size_t bytes = (size_t)count_a * sizeof(int32_t);
        int32_t *arr = (int32_t *)sf_xalloc(alloc, bytes);
        if (!arr) return SF_ERR_OOM;
        r = sf_binary_reader_get_i32s(br, offset_a, (size_t)count_a, arr);
        if (r != SF_OK) { sf_xfree(alloc, arr); return r; }
        e->unk_indices_a = arr;
        e->unk_indices_a_count = (size_t)count_a;
        e->unk_indices_a_capacity = (size_t)count_a;
    }
    if (count_b > 0) {
        size_t bytes = (size_t)count_b * sizeof(int32_t);
        int32_t *arr = (int32_t *)sf_xalloc(alloc, bytes);
        if (!arr) return SF_ERR_OOM;
        r = sf_binary_reader_get_i32s(br, offset_b, (size_t)count_b, arr);
        if (r != SF_OK) { sf_xfree(alloc, arr); return r; }
        e->unk_indices_b = arr;
        e->unk_indices_b_count = (size_t)count_b;
        e->unk_indices_b_capacity = (size_t)count_b;
    }
    return SF_OK;
}

sf_result_t sf_mcg_read_from_memory(sf_mcg_t **out, const void *data,
                                    size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, true, alloc);
    if (r != SF_OK) { sf_istream_close(stream); return r; }

    sf_mcg_t *mcg = NULL;
    r = sf_mcg_create_empty(&mcg, alloc);
    if (r != SF_OK) goto cleanup;

    const int32_t magic_opts[2] = { 1, 0x1000000 };
    int32_t magic = 0;
    r = sf_binary_reader_assert_i32(br, 2, magic_opts, &magic);
    if (r != SF_OK) goto cleanup;
    mcg->big_endian = (magic == 1);
    sf_binary_reader_set_big_endian(br, mcg->big_endian);

    int32_t node_count = 0, nodes_offset = 0, edge_count = 0, edges_offset = 0;
    r = sf_binary_reader_read_i32(br, &mcg->unk04);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &node_count);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &nodes_offset);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &edge_count);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &edges_offset);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &mcg->unk18);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &mcg->unk1c);
    if (r != SF_OK) goto cleanup;
    if (node_count < 0 || edge_count < 0 || nodes_offset < 0 || edges_offset < 0) {
        r = SF_ERR_OUT_OF_RANGE; goto cleanup;
    }

    r = reserve_nodes(mcg, (size_t)node_count);
    if (r == SF_OK) r = reserve_edges(mcg, (size_t)edge_count);
    if (r != SF_OK) goto cleanup;

    r = sf_binary_reader_step_in(br, nodes_offset);
    if (r != SF_OK) goto cleanup;
    for (int32_t i = 0; i < node_count; i++) {
        sf_mcg_node_t *n = NULL;
        r = sf_mcg_append_node(mcg, &n);
        if (r != SF_OK) break;
        r = read_node(br, n, alloc);
        if (r != SF_OK) break;
    }
    sf_binary_reader_step_out(br);
    if (r != SF_OK) goto cleanup;

    r = sf_binary_reader_step_in(br, edges_offset);
    if (r != SF_OK) goto cleanup;
    for (int32_t i = 0; i < edge_count; i++) {
        sf_mcg_edge_t *e = NULL;
        r = sf_mcg_append_edge(mcg, &e);
        if (r != SF_OK) break;
        r = read_edge(br, e, alloc);
        if (r != SF_OK) break;
    }
    sf_binary_reader_step_out(br);
    if (r != SF_OK) goto cleanup;

    *out = mcg;
    mcg = NULL;

cleanup:
    if (mcg) sf_mcg_destroy(mcg);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    return r;
}

/*===========================================================================
 * Write
 *===========================================================================*/

sf_result_t sf_mcg_write_to_memory(const sf_mcg_t *mcg, void **out_data,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(mcg != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    if (mcg->node_count > (size_t)INT32_MAX || mcg->edge_count > (size_t)INT32_MAX)
        return SF_ERR_OUT_OF_RANGE;

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, mcg->big_endian, alloc);
    if (r != SF_OK) { sf_ostream_close(stream); return r; }

    int32_t *edge_idx_a_offsets = NULL;
    int32_t *edge_idx_b_offsets = NULL;
    int32_t *node_node_offsets  = NULL;
    int32_t *node_edge_offsets  = NULL;

    if (mcg->edge_count > 0) {
        edge_idx_a_offsets = (int32_t *)sf_xalloc(alloc, mcg->edge_count * sizeof(int32_t));
        edge_idx_b_offsets = (int32_t *)sf_xalloc(alloc, mcg->edge_count * sizeof(int32_t));
        if (!edge_idx_a_offsets || !edge_idx_b_offsets) { r = SF_ERR_OOM; goto cleanup; }
        memset(edge_idx_a_offsets, 0, mcg->edge_count * sizeof(int32_t));
        memset(edge_idx_b_offsets, 0, mcg->edge_count * sizeof(int32_t));
    }
    if (mcg->node_count > 0) {
        node_node_offsets = (int32_t *)sf_xalloc(alloc, mcg->node_count * sizeof(int32_t));
        node_edge_offsets = (int32_t *)sf_xalloc(alloc, mcg->node_count * sizeof(int32_t));
        if (!node_node_offsets || !node_edge_offsets) { r = SF_ERR_OOM; goto cleanup; }
        memset(node_node_offsets, 0, mcg->node_count * sizeof(int32_t));
        memset(node_edge_offsets, 0, mcg->node_count * sizeof(int32_t));
    }

    r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, mcg->unk04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)mcg->node_count);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "NodesOffset");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)mcg->edge_count);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "EdgesOffset");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, mcg->unk18);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, mcg->unk1c);
    if (r != SF_OK) goto cleanup;

    for (size_t i = 0; i < mcg->edge_count; i++) {
        const sf_mcg_edge_t *e = &mcg->edges[i];
        int64_t pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        edge_idx_a_offsets[i] = (int32_t)pos;
        r = sf_binary_writer_write_i32s(bw, e->unk_indices_a_count, e->unk_indices_a);
        if (r != SF_OK) goto cleanup;
        pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        edge_idx_b_offsets[i] = (int32_t)pos;
        r = sf_binary_writer_write_i32s(bw, e->unk_indices_b_count, e->unk_indices_b);
        if (r != SF_OK) goto cleanup;
    }

    for (size_t i = 0; i < mcg->node_count; i++) {
        const sf_mcg_node_t *n = &mcg->nodes[i];
        int64_t pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        node_node_offsets[i] = (n->connected_node_count == 0) ? 0 : (int32_t)pos;
        r = sf_binary_writer_write_i32s(bw, n->connected_node_count, n->connected_node_indices);
        if (r != SF_OK) goto cleanup;
        pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        node_edge_offsets[i] = (n->connected_edge_count == 0) ? 0 : (int32_t)pos;
        r = sf_binary_writer_write_i32s(bw, n->connected_edge_count, n->connected_edge_indices);
        if (r != SF_OK) goto cleanup;
    }

    int64_t edges_pos = sf_binary_writer_position(bw);
    if (edges_pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_writer_fill_i32(bw, "EdgesOffset", (int32_t)edges_pos);
    if (r != SF_OK) goto cleanup;
    for (size_t i = 0; i < mcg->edge_count; i++) {
        const sf_mcg_edge_t *e = &mcg->edges[i];
        r = sf_binary_writer_write_i32(bw, e->node_index_a);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)e->unk_indices_a_count);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, edge_idx_a_offsets[i]);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, e->node_index_b);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)e->unk_indices_b_count);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, edge_idx_b_offsets[i]);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, e->mcp_room_index);
        if (r == SF_OK) r = sf_binary_writer_write_u32(bw, e->map_id);
        if (r == SF_OK) r = sf_binary_writer_write_f32(bw, e->unk20);
        if (r != SF_OK) goto cleanup;
    }

    int64_t nodes_pos = sf_binary_writer_position(bw);
    if (nodes_pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_writer_fill_i32(bw, "NodesOffset", (int32_t)nodes_pos);
    if (r != SF_OK) goto cleanup;
    for (size_t i = 0; i < mcg->node_count; i++) {
        const sf_mcg_node_t *n = &mcg->nodes[i];
        r = sf_binary_writer_write_i32(bw, (int32_t)n->connected_node_count);
        if (r == SF_OK) r = sf_binary_writer_write_vec3(bw, n->position);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, node_node_offsets[i]);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, node_edge_offsets[i]);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, n->unk18);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, n->unk1c);
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
    sf_xfree(alloc, edge_idx_a_offsets);
    sf_xfree(alloc, edge_idx_b_offsets);
    sf_xfree(alloc, node_node_offsets);
    sf_xfree(alloc, node_edge_offsets);
    if (bw) sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}
