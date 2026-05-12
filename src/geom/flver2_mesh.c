/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FLVER2 Mesh records.
 *
 * Mirrors upstream SoulsFormats/Formats/FLVER/FLVER2/Mesh.cs. The mesh
 * stores GLOBAL indices into the FLVER2-level FaceSet / VertexBuffer /
 * Bone pools; bounding-box bytes are echoed back as-read (no recompute).
 */

#include "souls_formats/sf_flver2.h"

#include "internal/flver2_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static sf_result_t mesh_label(char *buf, size_t buf_size, const char *prefix, size_t index) {
    int written = snprintf(buf, buf_size, "%s%zu", prefix, index);
    return (written < 0 || (size_t)written >= buf_size) ? SF_ERR_INTERNAL : SF_OK;
}

static sf_result_t mesh_alloc_indices(int32_t count, size_t *out_count,
                                      int32_t **out_indices, const sf_allocator_t *a) {
    *out_count = 0;
    *out_indices = NULL;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    if (count == 0) return SF_OK;
    if ((size_t)count > SIZE_MAX / sizeof(int32_t)) return SF_ERR_OUT_OF_RANGE;
    int32_t *p = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(int32_t));
    if (!p) return SF_ERR_OOM;
    *out_indices = p;
    *out_count = (size_t)count;
    return SF_OK;
}

sf_result_t sfi_flver2_mesh_read(sf_binary_reader_t *br, const sf_flver2_header_t *hdr,
                                 sf_flver2_mesh_t *out, const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL && hdr != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t r;
    r = sf_binary_reader_read_bool(br, &out->use_bone_weights);  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);                   if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);                   if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);                   if (r != SF_OK) return r;

    r = sf_binary_reader_read_i32(br, &out->material_index);     if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);                  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);                  if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->node_index);         if (r != SF_OK) return r;

    int32_t bone_count = 0, bounding_box_offset = 0, bone_offset = 0;
    int32_t face_set_count = 0, face_set_offset = 0;
    int32_t vertex_buffer_count = 0, vertex_buffer_offset = 0;
    r = sf_binary_reader_read_i32(br, &bone_count);              if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &bounding_box_offset);     if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &bone_offset);             if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &face_set_count);          if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &face_set_offset);         if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &vertex_buffer_count);     if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &vertex_buffer_offset);    if (r != SF_OK) return r;

    if (bounding_box_offset < 0 || bone_offset < 0 ||
        face_set_offset < 0 || vertex_buffer_offset < 0) {
        return SF_ERR_OUT_OF_RANGE;
    }

    if (bounding_box_offset != 0) {
        r = sf_binary_reader_step_in(br, bounding_box_offset);
        if (r != SF_OK) return r;
        out->has_bounding_box = true;
        r = sf_binary_reader_read_vec3(br, &out->bbox_min);
        if (r == SF_OK) r = sf_binary_reader_read_vec3(br, &out->bbox_max);
        if (r == SF_OK && hdr->version >= 0x2001Au) {
            r = sf_binary_reader_read_vec3(br, &out->bbox_unk);
        }
        sf_result_t step = sf_binary_reader_step_out(br);
        if (r != SF_OK) return r;
        if (step != SF_OK) return step;
    }

    r = mesh_alloc_indices(bone_count, &out->bone_index_count, &out->bone_indices, a);
    if (r != SF_OK) return r;
    if (out->bone_index_count > 0) {
        r = sf_binary_reader_get_i32s(br, bone_offset, out->bone_index_count, out->bone_indices);
        if (r != SF_OK) return r;
    }

    r = mesh_alloc_indices(face_set_count, &out->face_set_index_count,
                           &out->face_set_indices, a);
    if (r != SF_OK) return r;
    if (out->face_set_index_count > 0) {
        r = sf_binary_reader_get_i32s(br, face_set_offset, out->face_set_index_count,
                                      out->face_set_indices);
        if (r != SF_OK) return r;
    }

    r = mesh_alloc_indices(vertex_buffer_count, &out->vertex_buffer_index_count,
                           &out->vertex_buffer_indices, a);
    if (r != SF_OK) return r;
    if (out->vertex_buffer_index_count > 0) {
        r = sf_binary_reader_get_i32s(br, vertex_buffer_offset,
                                      out->vertex_buffer_index_count,
                                      out->vertex_buffer_indices);
        if (r != SF_OK) return r;
    }

    return SF_OK;
}

sf_result_t sfi_flver2_mesh_write(sf_binary_writer_t *bw, const sf_flver2_header_t *hdr,
                                  const sf_flver2_mesh_t *m, size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && m != NULL);
    (void)hdr;
    if (m->bone_index_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    if (m->face_set_index_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    if (m->vertex_buffer_index_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    char bbox_label[48], bone_label[48], face_label[48], vb_label[48];
    sf_result_t r;
    r = mesh_label(bbox_label, sizeof(bbox_label), "MeshBoundingBox", index);
    if (r != SF_OK) return r;
    r = mesh_label(bone_label, sizeof(bone_label), "MeshBoneIndices", index);
    if (r != SF_OK) return r;
    r = mesh_label(face_label, sizeof(face_label), "MeshFaceSetIndices", index);
    if (r != SF_OK) return r;
    r = mesh_label(vb_label, sizeof(vb_label), "MeshVertexBufferIndices", index);
    if (r != SF_OK) return r;

    r = sf_binary_writer_write_u8(bw, m->use_bone_weights ? 1u : 0u); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                             if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                             if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                             if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, m->material_index);            if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                            if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                            if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, m->node_index);                if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)m->bone_index_count); if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, bbox_label), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, bone_label), return r);
    r = sf_binary_writer_write_i32(bw, (int32_t)m->face_set_index_count);
    if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, face_label), return r);
    r = sf_binary_writer_write_i32(bw, (int32_t)m->vertex_buffer_index_count);
    if (r != SF_OK) return r;
    return sf_binary_writer_reserve_i32(bw, vb_label);
}

sf_result_t sfi_flver2_mesh_write_bounding_box(sf_binary_writer_t *bw,
                                               const sf_flver2_header_t *hdr,
                                               const sf_flver2_mesh_t *m, size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && m != NULL);
    char label[48];
    sf_result_t r = mesh_label(label, sizeof(label), "MeshBoundingBox", index);
    if (r != SF_OK) return r;
    if (!m->has_bounding_box) {
        return sf_binary_writer_fill_i32(bw, label, 0);
    }
    int64_t pos = sf_binary_writer_position(bw);
    if (pos < 0 || pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, label, (int32_t)pos), return r);
    r = sf_binary_writer_write_vec3(bw, m->bbox_min);          if (r != SF_OK) return r;
    r = sf_binary_writer_write_vec3(bw, m->bbox_max);          if (r != SF_OK) return r;
    if (hdr->version >= 0x2001Au) {
        r = sf_binary_writer_write_vec3(bw, m->bbox_unk);      if (r != SF_OK) return r;
    }
    return SF_OK;
}

sf_result_t sfi_flver2_mesh_write_bone_indices(sf_binary_writer_t *bw,
                                               const sf_flver2_mesh_t *m, size_t index,
                                               int32_t bone_indices_start) {
    SF_CHECK_ARG(bw != NULL && m != NULL);
    if (m->bone_index_count > 0 && m->bone_indices == NULL) return SF_ERR_INVALID_ARG;
    char label[48];
    sf_result_t r = mesh_label(label, sizeof(label), "MeshBoneIndices", index);
    if (r != SF_OK) return r;
    if (m->bone_index_count == 0) {
        return sf_binary_writer_fill_i32(bw, label, bone_indices_start);
    }
    int64_t pos = sf_binary_writer_position(bw);
    if (pos < 0 || pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, label, (int32_t)pos), return r);
    return sf_binary_writer_write_i32s(bw, m->bone_index_count, m->bone_indices);
}

sf_result_t sfi_flver2_mesh_fill_face_set_indices(sf_binary_writer_t *bw,
                                                  const sf_flver2_mesh_t *m, size_t index) {
    SF_CHECK_ARG(bw != NULL && m != NULL);
    if (m->face_set_index_count > 0 && m->face_set_indices == NULL) return SF_ERR_INVALID_ARG;
    char label[48];
    sf_result_t r = mesh_label(label, sizeof(label), "MeshFaceSetIndices", index);
    if (r != SF_OK) return r;
    int64_t pos = sf_binary_writer_position(bw);
    if (pos < 0 || pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, label, (int32_t)pos), return r);
    return sf_binary_writer_write_i32s(bw, m->face_set_index_count, m->face_set_indices);
}

sf_result_t sfi_flver2_mesh_fill_vertex_buffer_indices(sf_binary_writer_t *bw,
                                                       const sf_flver2_mesh_t *m, size_t index) {
    SF_CHECK_ARG(bw != NULL && m != NULL);
    if (m->vertex_buffer_index_count > 0 && m->vertex_buffer_indices == NULL) {
        return SF_ERR_INVALID_ARG;
    }
    char label[48];
    sf_result_t r = mesh_label(label, sizeof(label), "MeshVertexBufferIndices", index);
    if (r != SF_OK) return r;
    int64_t pos = sf_binary_writer_position(bw);
    if (pos < 0 || pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, label, (int32_t)pos), return r);
    return sf_binary_writer_write_i32s(bw, m->vertex_buffer_index_count,
                                       m->vertex_buffer_indices);
}

void sfi_flver2_mesh_destroy_inplace(sf_flver2_mesh_t *m, const sf_allocator_t *a) {
    if (!m) return;
    sf_xfree(a, m->bone_indices);
    sf_xfree(a, m->face_set_indices);
    sf_xfree(a, m->vertex_buffer_indices);
    memset(m, 0, sizeof(*m));
}

bool sf_flver2_mesh_use_bone_weights(const sf_flver2_mesh_t *m) {
    return m ? m->use_bone_weights : false;
}
int32_t sf_flver2_mesh_material_index(const sf_flver2_mesh_t *m) {
    return m ? m->material_index : -1;
}
int32_t sf_flver2_mesh_node_index(const sf_flver2_mesh_t *m) {
    return m ? m->node_index : -1;
}
size_t sf_flver2_mesh_bone_index_count(const sf_flver2_mesh_t *m) {
    return m ? m->bone_index_count : 0;
}
int32_t sf_flver2_mesh_bone_index(const sf_flver2_mesh_t *m, size_t i) {
    return (m && i < m->bone_index_count) ? m->bone_indices[i] : -1;
}
size_t sf_flver2_mesh_face_set_index_count(const sf_flver2_mesh_t *m) {
    return m ? m->face_set_index_count : 0;
}
int32_t sf_flver2_mesh_face_set_index(const sf_flver2_mesh_t *m, size_t i) {
    return (m && i < m->face_set_index_count) ? m->face_set_indices[i] : -1;
}
size_t sf_flver2_mesh_vertex_buffer_index_count(const sf_flver2_mesh_t *m) {
    return m ? m->vertex_buffer_index_count : 0;
}
int32_t sf_flver2_mesh_vertex_buffer_index(const sf_flver2_mesh_t *m, size_t i) {
    return (m && i < m->vertex_buffer_index_count) ? m->vertex_buffer_indices[i] : -1;
}
