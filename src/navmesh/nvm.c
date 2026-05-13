/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — NVM navmesh polygon mesh (DeS/DS1).
 *
 * Mirrors:
 *   SoulsFormats/Formats/NVM.cs
 */

#include "souls_formats/sf_nvm.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct sf_nvm_triangle {
    int32_t                 vertex_index_1, vertex_index_2, vertex_index_3;
    int32_t                 edge_index_1, edge_index_2, edge_index_3;
    int32_t                 obstacle_count;
    sf_nvm_triangle_flags_t flags;
};

struct sf_nvm_box {
    sf_vec3_t      min_corner;
    sf_vec3_t      max_corner;
    int32_t       *triangle_indices;
    size_t         triangle_index_count;
    size_t         triangle_index_capacity;
    sf_nvm_box_t  *children[4];
};

struct sf_nvm_entity {
    int32_t  entity_id;
    int32_t *triangle_indices;
    size_t   triangle_index_count;
    size_t   triangle_index_capacity;
};

struct sf_nvm {
    const sf_allocator_t *alloc;
    bool                  big_endian;
    sf_vec3_t            *vertices;
    size_t                vertex_count;
    size_t                vertex_capacity;
    sf_nvm_triangle_t    *triangles;
    size_t                triangle_count;
    size_t                triangle_capacity;
    sf_nvm_box_t         *root_box;
    sf_nvm_entity_t      *entities;
    size_t                entity_count;
    size_t                entity_capacity;
};

static void destroy_box(sf_nvm_box_t *b, const sf_allocator_t *a) {
    if (!b) return;
    for (size_t i = 0; i < 4; i++) destroy_box(b->children[i], a);
    sf_xfree(a, b->triangle_indices);
    sf_xfree(a, b);
}

void sf_nvm_destroy(sf_nvm_t *nvm) {
    if (!nvm) return;
    const sf_allocator_t *a = nvm->alloc;
    sf_xfree(a, nvm->vertices);
    sf_xfree(a, nvm->triangles);
    destroy_box(nvm->root_box, a);
    for (size_t i = 0; i < nvm->entity_count; i++)
        sf_xfree(a, nvm->entities[i].triangle_indices);
    sf_xfree(a, nvm->entities);
    sf_xfree(a, nvm);
}

sf_result_t sf_nvm_create_empty(sf_nvm_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_nvm_t *n = (sf_nvm_t *)sf_xalloc(alloc, sizeof(*n));
    if (!n) return SF_ERR_OOM;
    memset(n, 0, sizeof(*n));
    n->alloc = alloc;
    *out = n;
    return SF_OK;
}

bool sf_nvm_big_endian(const sf_nvm_t *n) { return n ? n->big_endian : false; }
void sf_nvm_set_big_endian(sf_nvm_t *n, bool be) { if (n) n->big_endian = be; }

size_t sf_nvm_vertex_count(const sf_nvm_t *n) { return n ? n->vertex_count : 0; }
sf_vec3_t sf_nvm_vertex(const sf_nvm_t *n, size_t i) {
    sf_vec3_t z = {0};
    if (!n || i >= n->vertex_count) return z;
    return n->vertices[i];
}

sf_result_t sf_nvm_append_vertex(sf_nvm_t *n, sf_vec3_t v) {
    SF_CHECK_ARG(n != NULL);
    if (n->vertex_count + 1 > n->vertex_capacity) {
        size_t cap = n->vertex_capacity ? n->vertex_capacity * 2 : 16;
        if (cap < n->vertex_count + 1) cap = n->vertex_count + 1;
        sf_vec3_t *arr = (sf_vec3_t *)sf_xrealloc(
            n->alloc, n->vertices,
            n->vertex_capacity * sizeof(sf_vec3_t),
            cap * sizeof(sf_vec3_t));
        if (!arr) return SF_ERR_OOM;
        n->vertices = arr;
        n->vertex_capacity = cap;
    }
    n->vertices[n->vertex_count++] = v;
    return SF_OK;
}

size_t sf_nvm_triangle_count(const sf_nvm_t *n) { return n ? n->triangle_count : 0; }
const sf_nvm_triangle_t *sf_nvm_triangle(const sf_nvm_t *n, size_t i) {
    if (!n || i >= n->triangle_count) return NULL;
    return &n->triangles[i];
}

sf_result_t sf_nvm_append_triangle(sf_nvm_t *n, sf_nvm_triangle_t **out) {
    SF_CHECK_ARG(n != NULL);
    if (n->triangle_count + 1 > n->triangle_capacity) {
        size_t cap = n->triangle_capacity ? n->triangle_capacity * 2 : 16;
        if (cap < n->triangle_count + 1) cap = n->triangle_count + 1;
        sf_nvm_triangle_t *arr = (sf_nvm_triangle_t *)sf_xrealloc(
            n->alloc, n->triangles,
            n->triangle_capacity * sizeof(sf_nvm_triangle_t),
            cap * sizeof(sf_nvm_triangle_t));
        if (!arr) return SF_ERR_OOM;
        n->triangles = arr;
        n->triangle_capacity = cap;
    }
    sf_nvm_triangle_t *t = &n->triangles[n->triangle_count++];
    memset(t, 0, sizeof(*t));
    if (out) *out = t;
    return SF_OK;
}

int32_t sf_nvm_triangle_vertex_index_1(const sf_nvm_triangle_t *t) { return t ? t->vertex_index_1 : 0; }
int32_t sf_nvm_triangle_vertex_index_2(const sf_nvm_triangle_t *t) { return t ? t->vertex_index_2 : 0; }
int32_t sf_nvm_triangle_vertex_index_3(const sf_nvm_triangle_t *t) { return t ? t->vertex_index_3 : 0; }
int32_t sf_nvm_triangle_edge_index_1  (const sf_nvm_triangle_t *t) { return t ? t->edge_index_1 : 0; }
int32_t sf_nvm_triangle_edge_index_2  (const sf_nvm_triangle_t *t) { return t ? t->edge_index_2 : 0; }
int32_t sf_nvm_triangle_edge_index_3  (const sf_nvm_triangle_t *t) { return t ? t->edge_index_3 : 0; }
int32_t sf_nvm_triangle_obstacle_count(const sf_nvm_triangle_t *t) { return t ? t->obstacle_count : 0; }
sf_nvm_triangle_flags_t sf_nvm_triangle_flags(const sf_nvm_triangle_t *t) {
    return t ? t->flags : SF_NVM_TRI_FLAG_NONE;
}

void sf_nvm_triangle_set_vertex_indices(sf_nvm_triangle_t *t,
                                        int32_t v1, int32_t v2, int32_t v3) {
    if (!t) return;
    t->vertex_index_1 = v1; t->vertex_index_2 = v2; t->vertex_index_3 = v3;
}
void sf_nvm_triangle_set_edge_indices(sf_nvm_triangle_t *t,
                                      int32_t e1, int32_t e2, int32_t e3) {
    if (!t) return;
    t->edge_index_1 = e1; t->edge_index_2 = e2; t->edge_index_3 = e3;
}
void sf_nvm_triangle_set_obstacle_count(sf_nvm_triangle_t *t, int32_t v) {
    if (t) t->obstacle_count = v;
}
void sf_nvm_triangle_set_flags(sf_nvm_triangle_t *t, sf_nvm_triangle_flags_t v) {
    if (t) t->flags = v;
}

const sf_nvm_box_t *sf_nvm_root_box(const sf_nvm_t *n) { return n ? n->root_box : NULL; }
sf_nvm_box_t       *sf_nvm_root_box_mut(sf_nvm_t *n) { return n ? n->root_box : NULL; }

sf_result_t sf_nvm_create_box(sf_nvm_t *nvm, sf_nvm_box_t **out_box) {
    SF_CHECK_ARG(nvm != NULL);
    SF_CHECK_ARG(out_box != NULL);
    sf_nvm_box_t *b = (sf_nvm_box_t *)sf_xalloc(nvm->alloc, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));
    *out_box = b;
    return SF_OK;
}

sf_result_t sf_nvm_set_root_box(sf_nvm_t *nvm, sf_nvm_box_t *box) {
    SF_CHECK_ARG(nvm != NULL);
    if (nvm->root_box) destroy_box(nvm->root_box, nvm->alloc);
    nvm->root_box = box;
    return SF_OK;
}

sf_vec3_t sf_nvm_box_min_corner(const sf_nvm_box_t *b) {
    sf_vec3_t z = {0};
    return b ? b->min_corner : z;
}
sf_vec3_t sf_nvm_box_max_corner(const sf_nvm_box_t *b) {
    sf_vec3_t z = {0};
    return b ? b->max_corner : z;
}
size_t  sf_nvm_box_triangle_index_count(const sf_nvm_box_t *b) {
    return b ? b->triangle_index_count : 0;
}
int32_t sf_nvm_box_triangle_index(const sf_nvm_box_t *b, size_t i) {
    if (!b || i >= b->triangle_index_count) return 0;
    return b->triangle_indices[i];
}
const sf_nvm_box_t *sf_nvm_box_child(const sf_nvm_box_t *b, size_t which) {
    if (!b || which >= 4) return NULL;
    return b->children[which];
}
sf_nvm_box_t *sf_nvm_box_child_mut(sf_nvm_box_t *b, size_t which) {
    if (!b || which >= 4) return NULL;
    return b->children[which];
}
void sf_nvm_box_set_min_corner(sf_nvm_box_t *b, sf_vec3_t v) { if (b) b->min_corner = v; }
void sf_nvm_box_set_max_corner(sf_nvm_box_t *b, sf_vec3_t v) { if (b) b->max_corner = v; }

sf_result_t sf_nvm_box_append_triangle_index(sf_nvm_box_t *b, int32_t idx) {
    SF_CHECK_ARG(b != NULL);
    if (b->triangle_index_count + 1 > b->triangle_index_capacity) {
        size_t cap = b->triangle_index_capacity ? b->triangle_index_capacity * 2 : 4;
        if (cap < b->triangle_index_count + 1) cap = b->triangle_index_count + 1;
        int32_t *arr = (int32_t *)sf_xrealloc(NULL, b->triangle_indices,
            b->triangle_index_capacity * sizeof(int32_t),
            cap * sizeof(int32_t));
        if (!arr) return SF_ERR_OOM;
        b->triangle_indices = arr;
        b->triangle_index_capacity = cap;
    }
    b->triangle_indices[b->triangle_index_count++] = idx;
    return SF_OK;
}

sf_result_t sf_nvm_box_set_child(sf_nvm_box_t *b, size_t which, sf_nvm_box_t *child) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(which < 4);
    b->children[which] = child;
    return SF_OK;
}

size_t sf_nvm_entity_count(const sf_nvm_t *n) { return n ? n->entity_count : 0; }
const sf_nvm_entity_t *sf_nvm_entity(const sf_nvm_t *n, size_t i) {
    if (!n || i >= n->entity_count) return NULL;
    return &n->entities[i];
}
sf_result_t sf_nvm_append_entity(sf_nvm_t *n, sf_nvm_entity_t **out) {
    SF_CHECK_ARG(n != NULL);
    if (n->entity_count + 1 > n->entity_capacity) {
        size_t cap = n->entity_capacity ? n->entity_capacity * 2 : 4;
        if (cap < n->entity_count + 1) cap = n->entity_count + 1;
        sf_nvm_entity_t *arr = (sf_nvm_entity_t *)sf_xrealloc(
            n->alloc, n->entities,
            n->entity_capacity * sizeof(sf_nvm_entity_t),
            cap * sizeof(sf_nvm_entity_t));
        if (!arr) return SF_ERR_OOM;
        n->entities = arr;
        n->entity_capacity = cap;
    }
    sf_nvm_entity_t *e = &n->entities[n->entity_count++];
    memset(e, 0, sizeof(*e));
    if (out) *out = e;
    return SF_OK;
}

int32_t sf_nvm_entity_entity_id(const sf_nvm_entity_t *e) { return e ? e->entity_id : 0; }
size_t  sf_nvm_entity_triangle_index_count(const sf_nvm_entity_t *e) {
    return e ? e->triangle_index_count : 0;
}
int32_t sf_nvm_entity_triangle_index(const sf_nvm_entity_t *e, size_t i) {
    if (!e || i >= e->triangle_index_count) return 0;
    return e->triangle_indices[i];
}
void sf_nvm_entity_set_entity_id(sf_nvm_entity_t *e, int32_t v) { if (e) e->entity_id = v; }
sf_result_t sf_nvm_entity_append_triangle_index(sf_nvm_entity_t *e, int32_t idx) {
    SF_CHECK_ARG(e != NULL);
    if (e->triangle_index_count + 1 > e->triangle_index_capacity) {
        size_t cap = e->triangle_index_capacity ? e->triangle_index_capacity * 2 : 4;
        if (cap < e->triangle_index_count + 1) cap = e->triangle_index_count + 1;
        int32_t *arr = (int32_t *)sf_xrealloc(NULL, e->triangle_indices,
            e->triangle_index_capacity * sizeof(int32_t),
            cap * sizeof(int32_t));
        if (!arr) return SF_ERR_OOM;
        e->triangle_indices = arr;
        e->triangle_index_capacity = cap;
    }
    e->triangle_indices[e->triangle_index_count++] = idx;
    return SF_OK;
}

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t read_triangle(sf_binary_reader_t *br, sf_nvm_triangle_t *t) {
    sf_result_t r = sf_binary_reader_read_i32(br, &t->vertex_index_1);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &t->vertex_index_2);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &t->vertex_index_3);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &t->edge_index_1);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &t->edge_index_2);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &t->edge_index_3);
    int32_t packed = 0;
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &packed);
    if (r != SF_OK) return r;
    if ((packed & 3) != 0) return SF_ERR_BAD_MAGIC;
    t->obstacle_count = (packed >> 2) & 0x3FFF;
    t->flags = (sf_nvm_triangle_flags_t)((uint32_t)packed >> 16);
    return SF_OK;
}

static sf_result_t read_box(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                            sf_nvm_box_t **out_box);

static sf_result_t read_box_at(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                               int32_t offset, sf_nvm_box_t **out_box) {
    *out_box = NULL;
    if (offset == 0) return SF_OK;
    sf_result_t r = sf_binary_reader_step_in(br, offset);
    if (r != SF_OK) return r;
    r = read_box(br, alloc, out_box);
    sf_binary_reader_step_out(br);
    return r;
}

static sf_result_t read_box(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                            sf_nvm_box_t **out_box) {
    sf_nvm_box_t *b = (sf_nvm_box_t *)sf_xalloc(alloc, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));

    int32_t tri_count = 0, tri_offset = 0;
    int32_t child_offsets[4] = {0};
    sf_result_t r = sf_binary_reader_read_vec3(br, &b->min_corner);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &tri_count);
    if (r == SF_OK) r = sf_binary_reader_read_vec3(br, &b->max_corner);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &tri_offset);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &child_offsets[0]);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &child_offsets[1]);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &child_offsets[2]);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &child_offsets[3]);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) { destroy_box(b, alloc); return r; }
    if (tri_count < 0) { destroy_box(b, alloc); return SF_ERR_OUT_OF_RANGE; }

    if (tri_count > 0) {
        b->triangle_indices = (int32_t *)sf_xalloc(alloc, (size_t)tri_count * sizeof(int32_t));
        if (!b->triangle_indices) { destroy_box(b, alloc); return SF_ERR_OOM; }
        b->triangle_index_count = (size_t)tri_count;
        b->triangle_index_capacity = (size_t)tri_count;
        r = sf_binary_reader_get_i32s(br, tri_offset, (size_t)tri_count, b->triangle_indices);
        if (r != SF_OK) { destroy_box(b, alloc); return r; }
    }

    for (size_t i = 0; i < 4; i++) {
        r = read_box_at(br, alloc, child_offsets[i], &b->children[i]);
        if (r != SF_OK) { destroy_box(b, alloc); return r; }
    }

    *out_box = b;
    return SF_OK;
}

static sf_result_t read_entity(sf_binary_reader_t *br, sf_nvm_entity_t *e,
                               const sf_allocator_t *alloc) {
    int32_t index_offset = 0, index_count = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &e->entity_id);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &index_offset);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &index_count);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) return r;
    if (index_count < 0) return SF_ERR_OUT_OF_RANGE;
    if (index_count > 0) {
        e->triangle_indices = (int32_t *)sf_xalloc(alloc, (size_t)index_count * sizeof(int32_t));
        if (!e->triangle_indices) return SF_ERR_OOM;
        e->triangle_index_count = (size_t)index_count;
        e->triangle_index_capacity = (size_t)index_count;
        r = sf_binary_reader_get_i32s(br, index_offset, (size_t)index_count, e->triangle_indices);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

sf_result_t sf_nvm_read_from_memory(sf_nvm_t **out, const void *data, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) { sf_istream_close(stream); return r; }

    sf_nvm_t *nvm = NULL;
    r = sf_nvm_create_empty(&nvm, alloc);
    if (r != SF_OK) goto cleanup;

    const int32_t magic_opts[2] = { 1, 0x1000000 };
    int32_t magic = 0;
    r = sf_binary_reader_assert_i32(br, 2, magic_opts, &magic);
    if (r != SF_OK) goto cleanup;
    nvm->big_endian = (magic != 1);
    sf_binary_reader_set_big_endian(br, nvm->big_endian);

    int32_t vertex_count = 0, triangle_count = 0;
    int32_t root_box_offset = 0, entity_count = 0, entity_offset = 0;
    r = sf_binary_reader_read_i32(br, &vertex_count);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0x80);
    if (r != SF_OK) goto cleanup;
    if (vertex_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_reader_read_i32(br, &triangle_count);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0x80 + vertex_count * 0xC);
    if (r != SF_OK) goto cleanup;
    if (triangle_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_reader_read_i32(br, &root_box_offset);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &entity_count);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &entity_offset);
    if (r != SF_OK) goto cleanup;
    if (entity_count < 0 || root_box_offset < 0 || entity_offset < 0) {
        r = SF_ERR_OUT_OF_RANGE; goto cleanup;
    }
    for (int i = 0; i < 23; i++) {
        r = sf_binary_reader_assert_i32_one(br, 0);
        if (r != SF_OK) goto cleanup;
    }

    for (int32_t i = 0; i < vertex_count; i++) {
        sf_vec3_t v;
        r = sf_binary_reader_read_vec3(br, &v);
        if (r != SF_OK) goto cleanup;
        r = sf_nvm_append_vertex(nvm, v);
        if (r != SF_OK) goto cleanup;
    }

    for (int32_t i = 0; i < triangle_count; i++) {
        sf_nvm_triangle_t *t = NULL;
        r = sf_nvm_append_triangle(nvm, &t);
        if (r != SF_OK) goto cleanup;
        r = read_triangle(br, t);
        if (r != SF_OK) goto cleanup;
    }

    r = sf_binary_reader_step_in(br, root_box_offset);
    if (r != SF_OK) goto cleanup;
    r = read_box(br, alloc, &nvm->root_box);
    sf_binary_reader_step_out(br);
    if (r != SF_OK) goto cleanup;

    r = sf_binary_reader_step_in(br, entity_offset);
    if (r != SF_OK) goto cleanup;
    for (int32_t i = 0; i < entity_count; i++) {
        sf_nvm_entity_t *e = NULL;
        r = sf_nvm_append_entity(nvm, &e);
        if (r != SF_OK) break;
        r = read_entity(br, e, alloc);
        if (r != SF_OK) break;
    }
    sf_binary_reader_step_out(br);
    if (r != SF_OK) goto cleanup;

    *out = nvm;
    nvm = NULL;

cleanup:
    if (nvm) sf_nvm_destroy(nvm);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    return r;
}

/*===========================================================================
 * Write
 *===========================================================================*/

typedef struct nvm_offset_queue {
    int32_t *items;
    size_t   head;
    size_t   tail;
    size_t   capacity;
} nvm_offset_queue_t;

static sf_result_t queue_push(nvm_offset_queue_t *q, int32_t v, const sf_allocator_t *a) {
    if (q->tail == q->capacity) {
        size_t cap = q->capacity ? q->capacity * 2 : 16;
        int32_t *arr = (int32_t *)sf_xrealloc(a, q->items,
            q->capacity * sizeof(int32_t), cap * sizeof(int32_t));
        if (!arr) return SF_ERR_OOM;
        q->items = arr;
        q->capacity = cap;
    }
    q->items[q->tail++] = v;
    return SF_OK;
}

static int32_t queue_pop(nvm_offset_queue_t *q) {
    return q->items[q->head++];
}

static sf_result_t write_box_triangle_indices(sf_binary_writer_t *bw,
                                              const sf_nvm_box_t *box,
                                              nvm_offset_queue_t *q,
                                              const sf_allocator_t *a) {
    if (!box) return SF_OK;
    sf_result_t r = SF_OK;
    for (size_t i = 0; i < 4; i++) {
        r = write_box_triangle_indices(bw, box->children[i], q, a);
        if (r != SF_OK) return r;
    }
    if (box->triangle_index_count == 0) {
        return queue_push(q, 0, a);
    }
    int64_t pos = sf_binary_writer_position(bw);
    if (pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    r = queue_push(q, (int32_t)pos, a);
    if (r != SF_OK) return r;
    return sf_binary_writer_write_i32s(bw, box->triangle_index_count, box->triangle_indices);
}

static sf_result_t write_box(sf_binary_writer_t *bw, sf_nvm_box_t *box,
                             nvm_offset_queue_t *q, int32_t *out_offset) {
    int32_t child_offsets[4] = {0};
    sf_result_t r = SF_OK;
    for (size_t i = 0; i < 4; i++) {
        if (box->children[i]) {
            r = write_box(bw, box->children[i], q, &child_offsets[i]);
            if (r != SF_OK) return r;
        }
    }
    int64_t this_off = sf_binary_writer_position(bw);
    if (this_off > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out_offset = (int32_t)this_off;

    if (box->triangle_index_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    r = sf_binary_writer_write_vec3(bw, box->min_corner);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)box->triangle_index_count);
    if (r == SF_OK) r = sf_binary_writer_write_vec3(bw, box->max_corner);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, queue_pop(q));
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, child_offsets[0]);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, child_offsets[1]);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, child_offsets[2]);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, child_offsets[3]);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    return r;
}

sf_result_t sf_nvm_write_to_memory(const sf_nvm_t *nvm, void **out_data,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(nvm != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);
    SF_CHECK_ARG(nvm->root_box != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    if (nvm->vertex_count > (size_t)INT32_MAX ||
        nvm->triangle_count > (size_t)INT32_MAX ||
        nvm->entity_count > (size_t)INT32_MAX) {
        return SF_ERR_OUT_OF_RANGE;
    }

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, nvm->big_endian, alloc);
    if (r != SF_OK) { sf_ostream_close(stream); return r; }

    int32_t *entity_index_offsets = NULL;
    nvm_offset_queue_t queue = {0};

    r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)nvm->vertex_count);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "VertexOffset");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)nvm->triangle_count);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "TriangleOffset");
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "RootBoxOffset");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)nvm->entity_count);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "EntityOffset");
    for (int i = 0; r == SF_OK && i < 23; i++) {
        r = sf_binary_writer_write_i32(bw, 0);
    }
    if (r != SF_OK) goto cleanup;

    int64_t vertex_pos = sf_binary_writer_position(bw);
    if (vertex_pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_writer_fill_i32(bw, "VertexOffset", (int32_t)vertex_pos);
    if (r != SF_OK) goto cleanup;
    for (size_t i = 0; i < nvm->vertex_count; i++) {
        r = sf_binary_writer_write_vec3(bw, nvm->vertices[i]);
        if (r != SF_OK) goto cleanup;
    }

    int64_t tri_pos = sf_binary_writer_position(bw);
    if (tri_pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_writer_fill_i32(bw, "TriangleOffset", (int32_t)tri_pos);
    if (r != SF_OK) goto cleanup;
    for (size_t i = 0; i < nvm->triangle_count; i++) {
        const sf_nvm_triangle_t *t = &nvm->triangles[i];
        int32_t packed = ((t->obstacle_count & 0x3FFF) << 2) |
                         ((int32_t)((uint32_t)t->flags << 16));
        r = sf_binary_writer_write_i32(bw, t->vertex_index_1);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, t->vertex_index_2);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, t->vertex_index_3);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, t->edge_index_1);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, t->edge_index_2);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, t->edge_index_3);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, packed);
        if (r != SF_OK) goto cleanup;
    }

    r = write_box_triangle_indices(bw, nvm->root_box, &queue, alloc);
    if (r != SF_OK) goto cleanup;

    int32_t root_box_offset = 0;
    r = write_box(bw, nvm->root_box, &queue, &root_box_offset);
    if (r != SF_OK) goto cleanup;
    r = sf_binary_writer_fill_i32(bw, "RootBoxOffset", root_box_offset);
    if (r != SF_OK) goto cleanup;

    if (nvm->entity_count > 0) {
        entity_index_offsets = (int32_t *)sf_xalloc(alloc, nvm->entity_count * sizeof(int32_t));
        if (!entity_index_offsets) { r = SF_ERR_OOM; goto cleanup; }
        for (size_t i = 0; i < nvm->entity_count; i++) {
            int64_t pos = sf_binary_writer_position(bw);
            if (pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
            entity_index_offsets[i] = (int32_t)pos;
            r = sf_binary_writer_write_i32s(bw, nvm->entities[i].triangle_index_count,
                                            nvm->entities[i].triangle_indices);
            if (r != SF_OK) goto cleanup;
        }
    }

    int64_t entity_pos = sf_binary_writer_position(bw);
    if (entity_pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_writer_fill_i32(bw, "EntityOffset", (int32_t)entity_pos);
    if (r != SF_OK) goto cleanup;
    for (size_t i = 0; i < nvm->entity_count; i++) {
        const sf_nvm_entity_t *e = &nvm->entities[i];
        r = sf_binary_writer_write_i32(bw, e->entity_id);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, entity_index_offsets[i]);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)e->triangle_index_count);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
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
    sf_xfree(alloc, queue.items);
    sf_xfree(alloc, entity_index_offsets);
    if (bw) sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}
