#include "souls_formats/sf_flver2.h"
#include "internal/flver2_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_common.h"
#include <string.h>

SF_API sf_result_t sf_flver2_decode_mesh(const sf_flver2_t *f, size_t mesh_index,
                                         sf_flver2_decoded_mesh_t *out,
                                         const sf_allocator_t *a)
{
    SF_CHECK_ARG(f != NULL && out != NULL && a != NULL);
    if (mesh_index >= (size_t)f->header.mesh_count) {
        return SF_ERR_OUT_OF_RANGE;
    }

    const sf_flver2_mesh_t *mesh = &f->meshes[mesh_index];
    if (mesh->vertex_buffer_index_count == 0) {
        return SF_ERR_OUT_OF_RANGE;
    }

    int32_t vb_idx0 = mesh->vertex_buffer_indices[0];
    if (vb_idx0 < 0 || (size_t)vb_idx0 >= (size_t)f->header.vertex_buffer_count) {
        return SF_ERR_OUT_OF_RANGE;
    }
    const sf_flver2_vertex_buffer_t *vb0 = &f->vertex_buffers[vb_idx0];
    uint32_t vertex_count = vb0->vertex_count;

    for (size_t i = 1; i < mesh->vertex_buffer_index_count; ++i) {
        int32_t vb_idx = mesh->vertex_buffer_indices[i];
        if (vb_idx < 0 || (size_t)vb_idx >= (size_t)f->header.vertex_buffer_count) {
            return SF_ERR_OUT_OF_RANGE;
        }
        if (f->vertex_buffers[vb_idx].vertex_count != (int32_t)vertex_count) {
            return SF_ERR_OUT_OF_RANGE;
        }
    }

    memset(out, 0, sizeof(*out));
    out->vertex_count = vertex_count;

    if (vertex_count == 0) {
        return SF_OK;
    }

    out->positions = sf_xalloc(a, vertex_count * sizeof(sf_vec3_t));
    if (!out->positions) goto error_oom;

    bool has_normal = false;
    bool has_tangent = false;
    bool has_bitangent = false;
    bool has_bone_indices = false;
    bool has_bone_weights = false;
    uint8_t max_uv_count = 0;
    uint8_t max_color_count = 0;

    for (size_t i = 0; i < mesh->vertex_buffer_index_count; ++i) {
        int32_t vb_idx = mesh->vertex_buffer_indices[i];
        const sf_flver2_vertex_buffer_t *vb = &f->vertex_buffers[vb_idx];
        if (vb->layout_index < 0 || (size_t)vb->layout_index >= (size_t)f->header.buffer_layout_count) {
            goto error_invalid;
        }
        const sf_flver2_buffer_layout_t *layout = &f->buffer_layouts[vb->layout_index];
        for (size_t j = 0; j < layout->member_count; ++j) {
            const sf_flver2_layout_member_t *member = &layout->members[j];
            switch (member->semantic) {
                case SF_FLVER_LAYOUT_SEMANTIC_NORMAL: has_normal = true; break;
                case SF_FLVER_LAYOUT_SEMANTIC_TANGENT: has_tangent = true; break;
                case SF_FLVER_LAYOUT_SEMANTIC_BITANGENT: has_bitangent = true; break;
                case SF_FLVER_LAYOUT_SEMANTIC_BONE_INDICES: has_bone_indices = true; break;
                case SF_FLVER_LAYOUT_SEMANTIC_BONE_WEIGHTS: has_bone_weights = true; break;
                case SF_FLVER_LAYOUT_SEMANTIC_UV:
                    if (member->index + 1 > max_uv_count) max_uv_count = (uint8_t)(member->index + 1);
                    break;
                case SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR:
                    if (member->index + 1 > max_color_count) max_color_count = (uint8_t)(member->index + 1);
                    break;
                default: break;
            }
        }
    }

    if (has_normal) {
        out->normals = sf_xalloc(a, vertex_count * sizeof(sf_vec3_t));
        if (!out->normals) goto error_oom;
    }
    if (has_tangent) {
        out->tangents = sf_xalloc(a, vertex_count * sizeof(sf_vec4_t));
        if (!out->tangents) goto error_oom;
    }
    if (has_bitangent) {
        out->bitangents = sf_xalloc(a, vertex_count * sizeof(sf_vec3_t));
        if (!out->bitangents) goto error_oom;
    }
    if (has_bone_indices) {
        out->bone_indices = sf_xalloc(a, vertex_count * sizeof(sf_flver_vertex_bone_indices_t));
        if (!out->bone_indices) goto error_oom;
    }
    if (has_bone_weights) {
        out->bone_weights = sf_xalloc(a, vertex_count * sizeof(sf_flver_vertex_bone_weights_t));
        if (!out->bone_weights) goto error_oom;
    }
    for (uint8_t i = 0; i < max_uv_count && i < 8; ++i) {
        out->uvs[i] = sf_xalloc(a, vertex_count * sizeof(sf_vec2_t));
        if (!out->uvs[i]) goto error_oom;
    }
    for (uint8_t i = 0; i < max_color_count && i < 4; ++i) {
        out->colors[i] = sf_xalloc(a, vertex_count * sizeof(sf_flver_vertex_color_t));
        if (!out->colors[i]) goto error_oom;
    }

    sf_flver2_vertex_context_t ctx = {
        .uv_factor = (f->header.version >= 0x2001A) ? 1024.0f : 2048.0f,
        .is_ac6 = false,
        .header_version = f->header.version,
    };

    for (uint32_t v = 0; v < vertex_count; ++v) {
        sf_flver2_decoded_vertex_t merged_v;
        memset(&merged_v, 0, sizeof(merged_v));

        for (size_t i = 0; i < mesh->vertex_buffer_index_count; ++i) {
            int32_t vb_idx = mesh->vertex_buffer_indices[i];
            const sf_flver2_vertex_buffer_t *vb = &f->vertex_buffers[vb_idx];
            const sf_flver2_buffer_layout_t *layout = &f->buffer_layouts[vb->layout_index];

            const uint8_t *vertex_bytes = vb->vertex_bytes + (v * vb->vertex_size);
            
            sf_flver2_decoded_vertex_t decoded_v;
            sf_result_t res = sfi_flver2_vertex_decode_one(layout, vertex_bytes, &ctx, &decoded_v);
            if (res != SF_OK) {
                sf_flver2_decoded_mesh_free(out, a);
                return res;
            }

            for (size_t j = 0; j < layout->member_count; ++j) {
                const sf_flver2_layout_member_t *member = &layout->members[j];
                switch (member->semantic) {
                    case SF_FLVER_LAYOUT_SEMANTIC_POSITION:
                        merged_v.position = decoded_v.position;
                        break;
                    case SF_FLVER_LAYOUT_SEMANTIC_NORMAL:
                        merged_v.normal = decoded_v.normal;
                        break;
                    case SF_FLVER_LAYOUT_SEMANTIC_TANGENT:
                        merged_v.tangent = decoded_v.tangent;
                        break;
                    case SF_FLVER_LAYOUT_SEMANTIC_BITANGENT:
                        merged_v.bitangent = decoded_v.bitangent;
                        break;
                    case SF_FLVER_LAYOUT_SEMANTIC_BONE_INDICES:
                        merged_v.bone_indices = decoded_v.bone_indices;
                        break;
                    case SF_FLVER_LAYOUT_SEMANTIC_BONE_WEIGHTS:
                        merged_v.bone_weights = decoded_v.bone_weights;
                        break;
                    case SF_FLVER_LAYOUT_SEMANTIC_UV:
                        if (member->index < 8) {
                            merged_v.uvs[member->index] = decoded_v.uvs[member->index];
                        }
                        break;
                    case SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR:
                        if (member->index < 4) {
                            merged_v.colors[member->index] = decoded_v.colors[member->index];
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        out->positions[v] = merged_v.position;
        if (out->normals) out->normals[v] = merged_v.normal;
        if (out->tangents) out->tangents[v] = merged_v.tangent;
        if (out->bitangents) out->bitangents[v] = merged_v.bitangent;
        if (out->bone_indices) out->bone_indices[v] = merged_v.bone_indices;
        if (out->bone_weights) out->bone_weights[v] = merged_v.bone_weights;
        for (uint8_t i = 0; i < max_uv_count && i < 8; ++i) {
            out->uvs[i][v] = merged_v.uvs[i];
        }
        for (uint8_t i = 0; i < max_color_count && i < 4; ++i) {
            out->colors[i][v] = merged_v.colors[i];
        }
    }

    int32_t best_fs_idx = -1;
    for (size_t i = 0; i < mesh->face_set_index_count; ++i) {
        int32_t fs_idx = mesh->face_set_indices[i];
        if (fs_idx < 0 || (size_t)fs_idx >= (size_t)f->header.face_set_count) continue;
        const sf_flver2_face_set_t *fs = &f->face_sets[fs_idx];
        
        bool is_lod0 = !(fs->flags & (SF_FLVER2_FS_FLAGS_LOD_LEVEL_1 | 
                                      SF_FLVER2_FS_FLAGS_LOD_LEVEL_2 | 
                                      SF_FLVER2_FS_FLAGS_LOD_LEVEL_EX));
        if (is_lod0) {
            best_fs_idx = fs_idx;
            break;
        }
    }
    if (best_fs_idx == -1 && mesh->face_set_index_count > 0) {
        best_fs_idx = mesh->face_set_indices[0];
    }

    if (best_fs_idx != -1) {
        const sf_flver2_face_set_t *fs = &f->face_sets[best_fs_idx];
        size_t temp_index_count = 0;
        sf_result_t res = sfi_flver2_face_set_triangulate(fs, true, &out->indices, &temp_index_count, a);
        if (res != SF_OK) {
            sf_flver2_decoded_mesh_free(out, a);
            return res;
        }
        out->index_count = (uint32_t)temp_index_count;
    }

    return SF_OK;

error_oom:
    sf_flver2_decoded_mesh_free(out, a);
    return SF_ERR_OOM;

error_invalid:
    sf_flver2_decoded_mesh_free(out, a);
    return SF_ERR_OUT_OF_RANGE;
}

SF_API void sf_flver2_decoded_mesh_free(sf_flver2_decoded_mesh_t *m,
                                        const sf_allocator_t *a)
{
    if (!m) return;

    if (m->positions) sf_free(a, m->positions);
    if (m->normals) sf_free(a, m->normals);
    if (m->tangents) sf_free(a, m->tangents);
    if (m->bitangents) sf_free(a, m->bitangents);
    if (m->bone_indices) sf_free(a, m->bone_indices);
    if (m->bone_weights) sf_free(a, m->bone_weights);
    
    for (int i = 0; i < 8; ++i) {
        if (m->uvs[i]) sf_free(a, m->uvs[i]);
    }
    for (int i = 0; i < 4; ++i) {
        if (m->colors[i]) sf_free(a, m->colors[i]);
    }
    if (m->indices) sf_free(a, m->indices);

    memset(m, 0, sizeof(*m));
}
