/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_smd4.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_math.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

struct sf_smd4 {
    const sf_allocator_t *alloc;
    int32_t version;
    float bb_min_x, bb_min_y, bb_min_z;
    float bb_max_x, bb_max_y, bb_max_z;
    sf_smd4_unk10_t *unk10s;
    size_t unk10_count;
    size_t unk10_cap;
    sf_smd4_node_t *nodes;
    size_t node_count;
    size_t node_cap;
    sf_smd4_mesh_t *meshes;
    size_t mesh_count;
    size_t mesh_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static size_t smd4_vertex_size(uint8_t format) {
    switch (format) {
        case 0: return 16u;
        case 1: return 24u;
        case 2: return 36u;
        default: return 0u;
    }
}

static int32_t smd4_face_count(const sf_smd4_mesh_t *m) {
    if (m->index_count < 3) return 0;
    return (int32_t)(m->index_count / 3u);
}

static sf_result_t smd4_grow_unk10s(sf_smd4_t *s) {
    if (s->unk10_count < s->unk10_cap) return SF_OK;
    size_t new_cap = s->unk10_cap == 0 ? 4u : s->unk10_cap * 2u;
    sf_smd4_unk10_t *na = (sf_smd4_unk10_t *)sf_xalloc(s->alloc, new_cap * sizeof(*na));
    if (!na) return SF_ERR_OOM;
    if (s->unk10s) {
        memcpy(na, s->unk10s, s->unk10_count * sizeof(*na));
        sf_xfree(s->alloc, s->unk10s);
    }
    s->unk10s = na;
    s->unk10_cap = new_cap;
    return SF_OK;
}

static sf_result_t smd4_grow_nodes(sf_smd4_t *s) {
    if (s->node_count < s->node_cap) return SF_OK;
    size_t new_cap = s->node_cap == 0 ? 4u : s->node_cap * 2u;
    sf_smd4_node_t *na = (sf_smd4_node_t *)sf_xalloc(s->alloc, new_cap * sizeof(*na));
    if (!na) return SF_ERR_OOM;
    if (s->nodes) {
        memcpy(na, s->nodes, s->node_count * sizeof(*na));
        sf_xfree(s->alloc, s->nodes);
    }
    s->nodes = na;
    s->node_cap = new_cap;
    return SF_OK;
}

static sf_result_t smd4_grow_meshes(sf_smd4_t *s) {
    if (s->mesh_count < s->mesh_cap) return SF_OK;
    size_t new_cap = s->mesh_cap == 0 ? 4u : s->mesh_cap * 2u;
    sf_smd4_mesh_t *na = (sf_smd4_mesh_t *)sf_xalloc(s->alloc, new_cap * sizeof(*na));
    if (!na) return SF_ERR_OOM;
    memset(na, 0, new_cap * sizeof(*na));
    if (s->meshes) {
        memcpy(na, s->meshes, s->mesh_count * sizeof(*na));
        sf_xfree(s->alloc, s->meshes);
    }
    s->meshes = na;
    s->mesh_cap = new_cap;
    return SF_OK;
}

static void smd4_copy_name_from_heap(char dst[33], char *src) {
    size_t n = src ? strlen(src) : 0u;
    if (n > 32u) n = 32u;
    if (n > 0u) memcpy(dst, src, n);
    dst[n] = '\0';
}

sf_result_t sf_smd4_create(sf_smd4_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_smd4_t *s = (sf_smd4_t *)sf_xalloc(alloc, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    s->version = SF_SMD4_VERSION_0x40001;
    *out = s;
    return SF_OK;
}

void sf_smd4_destroy(sf_smd4_t *s) {
    if (!s) return;
    if (s->meshes) {
        for (size_t i = 0; i < s->mesh_count; i++) {
            sf_xfree(s->alloc, s->meshes[i].indices);
            sf_xfree(s->alloc, s->meshes[i].vertices);
        }
        sf_xfree(s->alloc, s->meshes);
    }
    sf_xfree(s->alloc, s->nodes);
    sf_xfree(s->alloc, s->unk10s);
    sf_xfree(s->alloc, s);
}

bool sf_smd4_is(const void *bytes, size_t size) {
    if (!bytes || size < 128) return false;
    return memcmp(bytes, "SMD4", 4) == 0;
}

int32_t sf_smd4_version(const sf_smd4_t *s) { return s ? s->version : 0; }
void sf_smd4_set_version(sf_smd4_t *s, int32_t version) {
    if (s) s->version = version;
}

void sf_smd4_get_bounding_box(const sf_smd4_t *s,
                              float *min_x, float *min_y, float *min_z,
                              float *max_x, float *max_y, float *max_z) {
    if (!s) return;
    if (min_x) *min_x = s->bb_min_x;
    if (min_y) *min_y = s->bb_min_y;
    if (min_z) *min_z = s->bb_min_z;
    if (max_x) *max_x = s->bb_max_x;
    if (max_y) *max_y = s->bb_max_y;
    if (max_z) *max_z = s->bb_max_z;
}

void sf_smd4_set_bounding_box(sf_smd4_t *s,
                              float min_x, float min_y, float min_z,
                              float max_x, float max_y, float max_z) {
    if (!s) return;
    s->bb_min_x = min_x; s->bb_min_y = min_y; s->bb_min_z = min_z;
    s->bb_max_x = max_x; s->bb_max_y = max_y; s->bb_max_z = max_z;
}

size_t sf_smd4_unk10_count(const sf_smd4_t *s) { return s ? s->unk10_count : 0u; }

sf_result_t sf_smd4_get_unk10(const sf_smd4_t *s, size_t index, sf_smd4_unk10_t *out) {
    SF_CHECK_ARG(s != NULL && out != NULL);
    if (index >= s->unk10_count) return SF_ERR_OUT_OF_RANGE;
    *out = s->unk10s[index];
    return SF_OK;
}

sf_result_t sf_smd4_add_unk10(sf_smd4_t *s, sf_smd4_unk10_t unk10) {
    SF_CHECK_ARG(s != NULL);
    TRY(smd4_grow_unk10s(s));
    unk10.name[32] = '\0';
    s->unk10s[s->unk10_count++] = unk10;
    return SF_OK;
}

size_t sf_smd4_node_count(const sf_smd4_t *s) { return s ? s->node_count : 0u; }

sf_result_t sf_smd4_get_node(const sf_smd4_t *s, size_t index, sf_smd4_node_t *out) {
    SF_CHECK_ARG(s != NULL && out != NULL);
    if (index >= s->node_count) return SF_ERR_OUT_OF_RANGE;
    *out = s->nodes[index];
    return SF_OK;
}

sf_result_t sf_smd4_add_node(sf_smd4_t *s, sf_smd4_node_t node) {
    SF_CHECK_ARG(s != NULL);
    TRY(smd4_grow_nodes(s));
    node.name[32] = '\0';
    s->nodes[s->node_count++] = node;
    return SF_OK;
}

size_t sf_smd4_mesh_count(const sf_smd4_t *s) { return s ? s->mesh_count : 0u; }

sf_result_t sf_smd4_get_mesh(const sf_smd4_t *s, size_t index,
                             const sf_smd4_mesh_t **out) {
    SF_CHECK_ARG(s != NULL && out != NULL);
    if (index >= s->mesh_count) return SF_ERR_OUT_OF_RANGE;
    *out = &s->meshes[index];
    return SF_OK;
}

sf_result_t sf_smd4_add_mesh(sf_smd4_t *s, sf_smd4_mesh_t mesh) {
    SF_CHECK_ARG(s != NULL);
    if (smd4_vertex_size(mesh.vertex_format) == 0u) return SF_ERR_INVALID_ARG;
    TRY(smd4_grow_meshes(s));

    sf_smd4_mesh_t stored = mesh;
    stored.indices = NULL;
    stored.vertices = NULL;

    if (mesh.index_count > 0) {
        stored.indices = (uint16_t *)sf_xalloc(s->alloc, mesh.index_count * sizeof(uint16_t));
        if (!stored.indices) return SF_ERR_OOM;
        memcpy(stored.indices, mesh.indices, mesh.index_count * sizeof(uint16_t));
    }
    if (mesh.vertex_count > 0) {
        stored.vertices = (sf_smd4_vertex_t *)sf_xalloc(
            s->alloc, mesh.vertex_count * sizeof(sf_smd4_vertex_t));
        if (!stored.vertices) {
            sf_xfree(s->alloc, stored.indices);
            return SF_ERR_OOM;
        }
        memcpy(stored.vertices, mesh.vertices,
               mesh.vertex_count * sizeof(sf_smd4_vertex_t));
    }
    s->meshes[s->mesh_count++] = stored;
    return SF_OK;
}

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t smd4_read_unk10(sf_binary_reader_t *r, sf_smd4_unk10_t *out) {
    char *name = NULL;
    TRY(sf_binary_reader_read_u8(r, &out->unk00));
    TRY(sf_binary_reader_read_u8(r, &out->unk01));
    TRY(sf_binary_reader_read_u8(r, &out->unk02));
    TRY(sf_binary_reader_read_u8(r, &out->unk03));
    sf_result_t e = sf_binary_reader_read_fix_str(r, 32, &name, NULL);
    if (e != SF_OK) return e;
    smd4_copy_name_from_heap(out->name, name);
    sf_free(NULL, name);
    return SF_OK;
}

static sf_result_t smd4_read_node(sf_binary_reader_t *r, sf_smd4_node_t *out) {
    char *name = NULL;
    sf_result_t e = sf_binary_reader_read_fix_str(r, 32, &name, NULL);
    if (e != SF_OK) return e;
    smd4_copy_name_from_heap(out->name, name);
    sf_free(NULL, name);

    sf_vec3_t v;
    TRY(sf_binary_reader_read_vec3(r, &v));
    out->translation_x = v.x; out->translation_y = v.y; out->translation_z = v.z;
    TRY(sf_binary_reader_read_vec3(r, &v));
    out->rotation_x = v.x; out->rotation_y = v.y; out->rotation_z = v.z;
    TRY(sf_binary_reader_read_vec3(r, &v));
    out->scale_x = v.x; out->scale_y = v.y; out->scale_z = v.z;
    TRY(sf_binary_reader_read_vec3(r, &v));
    out->bb_min_x = v.x; out->bb_min_y = v.y; out->bb_min_z = v.z;
    TRY(sf_binary_reader_read_vec3(r, &v));
    out->bb_max_x = v.x; out->bb_max_y = v.y; out->bb_max_z = v.z;

    TRY(sf_binary_reader_read_i16(r, &out->parent_index));
    TRY(sf_binary_reader_read_i16(r, &out->first_child_index));
    TRY(sf_binary_reader_read_i16(r, &out->next_sibling_index));
    TRY(sf_binary_reader_read_i16(r, &out->prev_sibling_index));
    TRY(sf_binary_reader_read_i32(r, &out->unk64));
    TRY(sf_binary_reader_read_i32(r, &out->unk68));
    TRY(sf_binary_reader_read_i32(r, &out->unk6c));
    return sf_binary_reader_read_i32s(r, 8, out->unk70);
}

static sf_result_t smd4_read_vertex(sf_binary_reader_t *r, uint8_t format,
                                    sf_smd4_vertex_t *out) {
    sf_vec3_t pos;
    sf_vec2_t uv;
    memset(out, 0, sizeof(*out));
    out->bone_indices[0] = -1;
    out->bone_indices[1] = -1;
    out->bone_indices[2] = -1;
    out->bone_indices[3] = -1;

    if (format == 0) {
        TRY(sf_binary_reader_read_vec3(r, &pos));
        out->x = pos.x; out->y = pos.y; out->z = pos.z;
        TRY(sf_binary_reader_read_i16(r, &out->bone_indices[0]));
        TRY(sf_binary_reader_assert_i16_one(r, 0));
        out->bone_weights[0] = 1.0f;
    } else if (format == 1) {
        TRY(sf_binary_reader_read_vec3(r, &pos));
        out->x = pos.x; out->y = pos.y; out->z = pos.z;
        TRY(sf_binary_reader_read_vec2(r, &uv));
        out->uv_x = uv.x; out->uv_y = uv.y;
        TRY(sf_binary_reader_read_i16(r, &out->bone_indices[0]));
        TRY(sf_binary_reader_assert_i16_one(r, 0));
        out->bone_weights[0] = 1.0f;
    } else if (format == 2) {
        TRY(sf_binary_reader_read_vec3(r, &pos));
        out->x = pos.x; out->y = pos.y; out->z = pos.z;
        TRY(sf_binary_reader_read_i16s(r, 4, out->bone_indices));
        TRY(sf_binary_reader_read_f32s(r, 4, out->bone_weights));
    } else {
        return SF_ERR_UNSUPPORTED_VERSION;
    }
    return SF_OK;
}

static sf_result_t smd4_read_mesh(sf_binary_reader_t *r, sf_smd4_t *s,
                                  int32_t data_offset, int32_t version) {
    if (version != SF_SMD4_VERSION_0x40001) return SF_ERR_UNSUPPORTED_VERSION;

    uint8_t vertex_format = 0;
    uint8_t unk01 = 0;
    bool unk02 = false, unk03 = false;
    uint16_t vertex_index_count = 0;
    int16_t unk06 = 0;
    int16_t bone_indices[28];
    int32_t vertex_indices_length = 0;
    int32_t vertex_indices_offset = 0;
    int32_t vertex_buffer_length = 0;
    int32_t vertex_buffer_offset = 0;

    TRY(sf_binary_reader_read_u8(r, &vertex_format));
    TRY(sf_binary_reader_read_u8(r, &unk01));
    TRY(sf_binary_reader_read_bool(r, &unk02));
    TRY(sf_binary_reader_read_bool(r, &unk03));
    TRY(sf_binary_reader_read_u16(r, &vertex_index_count));
    TRY(sf_binary_reader_read_i16(r, &unk06));
    TRY(sf_binary_reader_read_i16s(r, 28, bone_indices));
    TRY(sf_binary_reader_read_i32(r, &vertex_indices_length));
    if (vertex_indices_length != (int32_t)vertex_index_count * 2)
        return SF_ERR_BAD_MAGIC;
    TRY(sf_binary_reader_read_i32(r, &vertex_indices_offset));
    TRY(sf_binary_reader_read_i32(r, &vertex_buffer_length));
    TRY(sf_binary_reader_read_i32(r, &vertex_buffer_offset));

    size_t vsize = smd4_vertex_size(vertex_format);
    if (vsize == 0u) return SF_ERR_UNSUPPORTED_VERSION;
    if (vertex_buffer_length < 0 || ((uint32_t)vertex_buffer_length % vsize) != 0u)
        return SF_ERR_BAD_MAGIC;
    int32_t vertex_count_i = vertex_buffer_length / (int32_t)vsize;
    if (vertex_count_i < 0) return SF_ERR_BAD_MAGIC;
    size_t vertex_count = (size_t)vertex_count_i;

    sf_smd4_mesh_t m;
    memset(&m, 0, sizeof(m));
    m.vertex_format = vertex_format;
    m.unk01 = unk01;
    m.unk02 = unk02;
    m.unk03 = unk03;
    m.unk06 = unk06;
    memcpy(m.bone_indices, bone_indices, sizeof(m.bone_indices));

    uint16_t *indices = NULL;
    sf_smd4_vertex_t *vertices = NULL;

    if (vertex_index_count > 0) {
        indices = (uint16_t *)sf_xalloc(s->alloc, vertex_index_count * sizeof(uint16_t));
        if (!indices) return SF_ERR_OOM;
        sf_result_t e = sf_binary_reader_get_u16s(
            r, (int64_t)data_offset + (int64_t)vertex_indices_offset,
            vertex_index_count, indices);
        if (e != SF_OK) {
            sf_xfree(s->alloc, indices);
            return e;
        }
    }
    if (vertex_count > 0) {
        vertices = (sf_smd4_vertex_t *)sf_xalloc(
            s->alloc, vertex_count * sizeof(sf_smd4_vertex_t));
        if (!vertices) {
            sf_xfree(s->alloc, indices);
            return SF_ERR_OOM;
        }
        sf_result_t e = sf_binary_reader_step_in(
            r, (int64_t)data_offset + (int64_t)vertex_buffer_offset);
        if (e != SF_OK) {
            sf_xfree(s->alloc, indices);
            sf_xfree(s->alloc, vertices);
            return e;
        }
        for (size_t i = 0; i < vertex_count; i++) {
            e = smd4_read_vertex(r, vertex_format, &vertices[i]);
            if (e != SF_OK) {
                sf_binary_reader_step_out(r);
                sf_xfree(s->alloc, indices);
                sf_xfree(s->alloc, vertices);
                return e;
            }
        }
        e = sf_binary_reader_step_out(r);
        if (e != SF_OK) {
            sf_xfree(s->alloc, indices);
            sf_xfree(s->alloc, vertices);
            return e;
        }
    }

    m.indices = indices;
    m.index_count = vertex_index_count;
    m.vertices = vertices;
    m.vertex_count = vertex_count;

    sf_result_t e = smd4_grow_meshes(s);
    if (e != SF_OK) {
        sf_xfree(s->alloc, indices);
        sf_xfree(s->alloc, vertices);
        return e;
    }
    s->meshes[s->mesh_count++] = m;
    return SF_OK;
}

sf_result_t sf_smd4_read_from_memory(sf_smd4_t **out, const void *bytes, size_t size,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *is = NULL;
    sf_binary_reader_t *r = NULL;
    sf_smd4_t *s = NULL;
    sf_result_t e = SF_OK;

    alloc = sf_alloc_or_default(alloc);

    e = sf_istream_open_memory(&is, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, is, true, alloc);
    if (e != SF_OK) { sf_istream_close(is); return e; }

    e = sf_binary_reader_assert_ascii(r, "SMD4"); if (e != SF_OK) goto done;

    int32_t version = 0, data_offset = 0, data_size = 0;
    int32_t count_unk10 = 0, node_count = 0, mesh_count = 0;
    e = sf_binary_reader_read_i32(r, &version); if (e != SF_OK) goto done;
    if (version != SF_SMD4_VERSION_0x40001) { e = SF_ERR_UNSUPPORTED_VERSION; goto done; }
    e = sf_binary_reader_read_i32(r, &data_offset); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &data_size);   if (e != SF_OK) goto done;
    (void)data_size;
    e = sf_binary_reader_read_i32(r, &count_unk10); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &node_count);  if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &mesh_count);  if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, mesh_count); if (e != SF_OK) goto done;

    if (count_unk10 < 0 || node_count < 0 || mesh_count < 0) {
        e = SF_ERR_BAD_MAGIC; goto done;
    }

    sf_vec3_t bb_min, bb_max;
    e = sf_binary_reader_read_vec3(r, &bb_min); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_vec3(r, &bb_max); if (e != SF_OK) goto done;

    int32_t true_face_count = 0, total_face_count = 0;
    e = sf_binary_reader_read_i32(r, &true_face_count);  if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &total_face_count); if (e != SF_OK) goto done;
    (void)true_face_count;
    (void)total_face_count;
    e = sf_binary_reader_assert_pattern(r, 32, 0); if (e != SF_OK) goto done;

    e = sf_smd4_create(&s, alloc); if (e != SF_OK) goto done;
    s->version = version;
    s->bb_min_x = bb_min.x; s->bb_min_y = bb_min.y; s->bb_min_z = bb_min.z;
    s->bb_max_x = bb_max.x; s->bb_max_y = bb_max.y; s->bb_max_z = bb_max.z;

    for (int32_t i = 0; i < count_unk10; i++) {
        sf_smd4_unk10_t u;
        memset(&u, 0, sizeof(u));
        e = smd4_read_unk10(r, &u); if (e != SF_OK) goto done;
        e = smd4_grow_unk10s(s);    if (e != SF_OK) goto done;
        s->unk10s[s->unk10_count++] = u;
    }
    for (int32_t i = 0; i < node_count; i++) {
        sf_smd4_node_t n;
        memset(&n, 0, sizeof(n));
        e = smd4_read_node(r, &n); if (e != SF_OK) goto done;
        e = smd4_grow_nodes(s);    if (e != SF_OK) goto done;
        s->nodes[s->node_count++] = n;
    }
    for (int32_t i = 0; i < mesh_count; i++) {
        e = smd4_read_mesh(r, s, data_offset, version);
        if (e != SF_OK) goto done;
    }

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(is);
    if (e != SF_OK) { sf_smd4_destroy(s); return e; }
    *out = s;
    return SF_OK;
}

/*===========================================================================
 * Write
 *===========================================================================*/

static sf_result_t smd4_write_unk10(sf_binary_writer_t *w, const sf_smd4_unk10_t *u) {
    TRY(sf_binary_writer_write_u8(w, u->unk00));
    TRY(sf_binary_writer_write_u8(w, u->unk01));
    TRY(sf_binary_writer_write_u8(w, u->unk02));
    TRY(sf_binary_writer_write_u8(w, u->unk03));
    return sf_binary_writer_write_fix_str(w, u->name, 32, 0);
}

static sf_result_t smd4_write_node(sf_binary_writer_t *w, const sf_smd4_node_t *n) {
    TRY(sf_binary_writer_write_fix_str(w, n->name, 32, 0));
    sf_vec3_t v;
    v.x = n->translation_x; v.y = n->translation_y; v.z = n->translation_z;
    TRY(sf_binary_writer_write_vec3(w, v));
    v.x = n->rotation_x; v.y = n->rotation_y; v.z = n->rotation_z;
    TRY(sf_binary_writer_write_vec3(w, v));
    v.x = n->scale_x; v.y = n->scale_y; v.z = n->scale_z;
    TRY(sf_binary_writer_write_vec3(w, v));
    v.x = n->bb_min_x; v.y = n->bb_min_y; v.z = n->bb_min_z;
    TRY(sf_binary_writer_write_vec3(w, v));
    v.x = n->bb_max_x; v.y = n->bb_max_y; v.z = n->bb_max_z;
    TRY(sf_binary_writer_write_vec3(w, v));
    TRY(sf_binary_writer_write_i16(w, n->parent_index));
    TRY(sf_binary_writer_write_i16(w, n->first_child_index));
    TRY(sf_binary_writer_write_i16(w, n->next_sibling_index));
    TRY(sf_binary_writer_write_i16(w, n->prev_sibling_index));
    TRY(sf_binary_writer_write_i32(w, n->unk64));
    TRY(sf_binary_writer_write_i32(w, n->unk68));
    TRY(sf_binary_writer_write_i32(w, n->unk6c));
    return sf_binary_writer_write_i32s(w, 8, n->unk70);
}

static sf_result_t smd4_write_vertex(sf_binary_writer_t *w, uint8_t format,
                                     const sf_smd4_vertex_t *vt) {
    sf_vec3_t pos = { vt->x, vt->y, vt->z };
    sf_vec2_t uv = { vt->uv_x, vt->uv_y };
    if (format == 0) {
        TRY(sf_binary_writer_write_vec3(w, pos));
        TRY(sf_binary_writer_write_i16(w, vt->bone_indices[0]));
        return sf_binary_writer_write_i16(w, 0);
    } else if (format == 1) {
        TRY(sf_binary_writer_write_vec3(w, pos));
        TRY(sf_binary_writer_write_vec2(w, uv));
        TRY(sf_binary_writer_write_i16(w, vt->bone_indices[0]));
        return sf_binary_writer_write_i16(w, 0);
    } else if (format == 2) {
        TRY(sf_binary_writer_write_vec3(w, pos));
        TRY(sf_binary_writer_write_i16s(w, 4, vt->bone_indices));
        return sf_binary_writer_write_f32s(w, 4, vt->bone_weights);
    }
    return SF_ERR_UNSUPPORTED_VERSION;
}

static sf_result_t smd4_write_mesh_header(sf_binary_writer_t *w,
                                          const sf_smd4_mesh_t *m, size_t index) {
    char name_vi[64], name_vb[64];
    snprintf(name_vi, sizeof(name_vi), "VertexIndicesOffset_%zu", index);
    snprintf(name_vb, sizeof(name_vb), "VertexBufferOffset_%zu", index);

    TRY(sf_binary_writer_write_u8(w, m->vertex_format));
    TRY(sf_binary_writer_write_u8(w, m->unk01));
    TRY(sf_binary_writer_write_bool(w, m->unk02));
    TRY(sf_binary_writer_write_bool(w, m->unk03));
    TRY(sf_binary_writer_write_u16(w, (uint16_t)m->index_count));
    TRY(sf_binary_writer_write_i16(w, m->unk06));
    TRY(sf_binary_writer_write_i16s(w, 28, m->bone_indices));
    TRY(sf_binary_writer_write_i32(w, (int32_t)(m->index_count * 2u)));
    TRY(sf_binary_writer_reserve_i32(w, name_vi));
    TRY(sf_binary_writer_write_i32(w,
        (int32_t)(m->vertex_count * smd4_vertex_size(m->vertex_format))));
    return sf_binary_writer_reserve_i32(w, name_vb);
}

sf_result_t sf_smd4_write_to_memory(const sf_smd4_t *s, void **out_bytes,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(s != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;
    if (s->version != SF_SMD4_VERSION_0x40001) return SF_ERR_UNSUPPORTED_VERSION;

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&os, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, os, true, alloc);
    if (e != SF_OK) { sf_ostream_close(os); return e; }

    e = sf_binary_writer_write_bytes(w, "SMD4", 4); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, s->version); if (e != SF_OK) goto done;
    e = sf_binary_writer_reserve_i32(w, "DataOffset"); if (e != SF_OK) goto done;
    e = sf_binary_writer_reserve_i32(w, "DataSize");   if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)s->unk10_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)s->node_count);  if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)s->mesh_count);  if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)s->mesh_count);  if (e != SF_OK) goto done;

    sf_vec3_t bb_min = { s->bb_min_x, s->bb_min_y, s->bb_min_z };
    sf_vec3_t bb_max = { s->bb_max_x, s->bb_max_y, s->bb_max_z };
    e = sf_binary_writer_write_vec3(w, bb_min); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_vec3(w, bb_max); if (e != SF_OK) goto done;

    int32_t face_count = 0;
    for (size_t i = 0; i < s->mesh_count; i++)
        face_count += smd4_face_count(&s->meshes[i]);
    int32_t index_count = face_count * 3;
    e = sf_binary_writer_write_i32(w, face_count);  if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, index_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_pattern(w, 32, 0);   if (e != SF_OK) goto done;

    for (size_t i = 0; i < s->unk10_count; i++) {
        e = smd4_write_unk10(w, &s->unk10s[i]); if (e != SF_OK) goto done;
    }
    for (size_t i = 0; i < s->node_count; i++) {
        e = smd4_write_node(w, &s->nodes[i]); if (e != SF_OK) goto done;
    }
    for (size_t i = 0; i < s->mesh_count; i++) {
        e = smd4_write_mesh_header(w, &s->meshes[i], i); if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_pad(w, 0x800); if (e != SF_OK) goto done;
    int64_t data_start = sf_binary_writer_position(w);
    if (data_start < 0 || data_start > INT32_MAX) { e = SF_ERR_OUT_OF_RANGE; goto done; }
    e = sf_binary_writer_fill_i32(w, "DataOffset", (int32_t)data_start);
    if (e != SF_OK) goto done;

    char name_vi[64], name_vb[64];
    for (size_t i = 0; i < s->mesh_count; i++) {
        const sf_smd4_mesh_t *m = &s->meshes[i];
        snprintf(name_vi, sizeof(name_vi), "VertexIndicesOffset_%zu", i);
        snprintf(name_vb, sizeof(name_vb), "VertexBufferOffset_%zu", i);

        int64_t pos = sf_binary_writer_position(w);
        int64_t rel = pos - data_start;
        if (rel < 0 || rel > INT32_MAX) { e = SF_ERR_OUT_OF_RANGE; goto done; }
        e = sf_binary_writer_fill_i32(w, name_vi, (int32_t)rel); if (e != SF_OK) goto done;
        if (m->index_count > 0) {
            e = sf_binary_writer_write_u16s(w, m->index_count, m->indices);
            if (e != SF_OK) goto done;
        }
        e = sf_binary_writer_pad(w, 0x10); if (e != SF_OK) goto done;

        pos = sf_binary_writer_position(w);
        rel = pos - data_start;
        if (rel < 0 || rel > INT32_MAX) { e = SF_ERR_OUT_OF_RANGE; goto done; }
        e = sf_binary_writer_fill_i32(w, name_vb, (int32_t)rel); if (e != SF_OK) goto done;
        for (size_t j = 0; j < m->vertex_count; j++) {
            e = smd4_write_vertex(w, m->vertex_format, &m->vertices[j]);
            if (e != SF_OK) goto done;
        }
    }
    e = sf_binary_writer_pad(w, 0x800); if (e != SF_OK) goto done;

    int64_t data_end = sf_binary_writer_position(w);
    if (data_end - data_start > INT32_MAX) { e = SF_ERR_OUT_OF_RANGE; goto done; }
    e = sf_binary_writer_fill_i32(w, "DataSize", (int32_t)(data_end - data_start));
    if (e != SF_OK) goto done;

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(os);
    return e;
}
