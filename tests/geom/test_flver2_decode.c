/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T25 — FLVER2 decode_mesh synthetic-fixture verification.
 *
 * Builds the same unit-cube FLVER2 fixture as test_flver2_synthetic.c
 * (1 mesh, Float3 Position layout, 8 vertices, 12 indices = 4 triangles
 * spread across two cube faces), serialises it through the public writer,
 * reads it back, and verifies that sf_flver2_decode_mesh lays the vertex
 * stream out into typed `positions[]` / `indices[]` arrays matching the
 * source data exactly.
 *
 * Also exercises the documented error contract:
 *   - SF_ERR_INVALID_ARG on NULL inputs.
 *   - SF_ERR_OUT_OF_RANGE on an out-of-range `mesh_index`.
 *   - sf_flver2_decoded_mesh_free is NULL-safe and zeroes its struct.
 */

#include "internal/flver2_internal.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver2.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Synthetic unit-cube fixture (mirrors test_flver2_synthetic.c) ──────── */

static const float k_cube_vertices[24] = {
    0.f, 0.f, 0.f,
    1.f, 0.f, 0.f,
    1.f, 1.f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.f, 1.f,
    1.f, 0.f, 1.f,
    1.f, 1.f, 1.f,
    0.f, 1.f, 1.f,
};

static const uint32_t k_cube_indices[12] = {
    0, 1, 2,  0, 2, 3,
    4, 5, 6,  4, 6, 7,
};

static void build_synthetic_flver2(sf_flver2_t *flver,
                                   sf_flver2_material_t *materials,
                                   sf_flver2_mesh_t *meshes,
                                   sf_flver2_face_set_t *face_sets,
                                   sf_flver2_vertex_buffer_t *vertex_buffers,
                                   sf_flver2_buffer_layout_t *buffer_layouts,
                                   sf_flver2_layout_member_t *layout_members,
                                   int32_t *mesh_face_set_indices,
                                   int32_t *mesh_vertex_buffer_indices,
                                   uint32_t *face_set_indices,
                                   uint8_t  *vertex_bytes) {
    memset(flver, 0, sizeof(*flver));
    flver->alloc = NULL;
    flver->header.version             = 0x20014u;
    flver->header.material_count      = 1;
    flver->header.mesh_count          = 1;
    flver->header.face_set_count      = 1;
    flver->header.vertex_buffer_count = 1;
    flver->header.buffer_layout_count = 1;
    flver->header.face_count          = 4;
    flver->header.vertex_indices_size = 16;
    flver->header.unicode             = 1;

    materials[0].name = (char *)"test_mat";
    materials[0].mtd  = (char *)"test.mtd";
    materials[0].textures = NULL;
    materials[0].texture_count = 0;
    materials[0].gx_index = -1;
    materials[0].index = 0;
    materials[0].pretake_texture_index = -1;
    materials[0].pretake_texture_count = -1;
    flver->materials = materials;

    mesh_face_set_indices[0] = 0;
    mesh_vertex_buffer_indices[0] = 0;
    meshes[0].use_bone_weights = false;
    meshes[0].material_index = 0;
    meshes[0].node_index = 0;
    meshes[0].bone_indices = NULL;
    meshes[0].bone_index_count = 0;
    meshes[0].face_set_indices = mesh_face_set_indices;
    meshes[0].face_set_index_count = 1;
    meshes[0].vertex_buffer_indices = mesh_vertex_buffer_indices;
    meshes[0].vertex_buffer_index_count = 1;
    meshes[0].has_bounding_box = false;
    flver->meshes = meshes;

    memcpy(face_set_indices, k_cube_indices, sizeof(k_cube_indices));
    face_sets[0].flags = SF_FLVER2_FS_FLAGS_NONE;
    face_sets[0].triangle_strip = false;
    face_sets[0].cull_backfaces = true;
    face_sets[0].unk06 = 0;
    face_sets[0].unk07 = 0;
    face_sets[0].indices = face_set_indices;
    face_sets[0].index_count = 12;
    face_sets[0].index_size = 16;
    flver->face_sets = face_sets;

    memcpy(vertex_bytes, k_cube_vertices, sizeof(k_cube_vertices));
    vertex_buffers[0].buffer_index = 0;
    vertex_buffers[0].layout_index = 0;
    vertex_buffers[0].vertex_size = 12;
    vertex_buffers[0].vertex_count = 8;
    vertex_buffers[0].vertex_bytes = vertex_bytes;
    vertex_buffers[0].vertex_bytes_size = sizeof(k_cube_vertices);
    flver->vertex_buffers = vertex_buffers;

    layout_members[0].stream = 0;
    layout_members[0].struct_offset = 0;
    layout_members[0].type = SF_FLVER_LAYOUT_TYPE_FLOAT3;
    layout_members[0].semantic = SF_FLVER_LAYOUT_SEMANTIC_POSITION;
    layout_members[0].index = 0;
    layout_members[0].special_modifier = 0;
    buffer_layouts[0].members = layout_members;
    buffer_layouts[0].member_count = 1;
    flver->buffer_layouts = buffer_layouts;
}

/* Serialise the stack-built synthetic FLVER2 and parse it back through the
 * public reader so decode_mesh operates on heap-owned, allocator-bound
 * data — that is the contract decode_mesh + decoded_mesh_free must obey. */
static sf_flver2_t *build_and_round_trip(void)
{
    sf_flver2_t                 flver;
    sf_flver2_material_t        materials[1];
    sf_flver2_mesh_t            meshes[1];
    sf_flver2_face_set_t        face_sets[1];
    sf_flver2_vertex_buffer_t   vertex_buffers[1];
    sf_flver2_buffer_layout_t   buffer_layouts[1];
    sf_flver2_layout_member_t   layout_members[1];
    int32_t                     mesh_face_set_idx[1];
    int32_t                     mesh_vertex_buffer_idx[1];
    uint32_t                    face_set_indices[12];
    uint8_t                     vertex_bytes[96];

    memset(&materials, 0, sizeof(materials));
    memset(&meshes, 0, sizeof(meshes));
    memset(&face_sets, 0, sizeof(face_sets));
    memset(&vertex_buffers, 0, sizeof(vertex_buffers));
    memset(&buffer_layouts, 0, sizeof(buffer_layouts));
    memset(&layout_members, 0, sizeof(layout_members));

    build_synthetic_flver2(&flver, materials, meshes, face_sets, vertex_buffers,
                           buffer_layouts, layout_members,
                           mesh_face_set_idx, mesh_vertex_buffer_idx,
                           face_set_indices, vertex_bytes);

    void  *bytes = NULL;
    size_t size  = 0;
    if (sf_flver2_write_to_memory(&flver, &bytes, &size, NULL) != SF_OK) {
        return NULL;
    }

    sf_flver2_t *out = NULL;
    sf_result_t r = sf_flver2_read_from_memory(&out, bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        return NULL;
    }
    return out;
}

/* ── T1: positions array matches the cube fixture exactly ───────────────── */

static void test_decode_mesh_unit_cube_positions(void)
{
    sf_flver2_t *flver = build_and_round_trip();
    TEST_ASSERT_NOT_NULL(flver);

    sf_flver2_decoded_mesh_t decoded;
    memset(&decoded, 0, sizeof(decoded));

    sf_result_t r = sf_flver2_decode_mesh(flver, 0, &decoded, sf_default_allocator());
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    TEST_ASSERT_EQUAL_UINT32(8u, decoded.vertex_count);
    TEST_ASSERT_NOT_NULL(decoded.positions);

    for (size_t v = 0; v < 8; ++v) {
        TEST_ASSERT_EQUAL_FLOAT(k_cube_vertices[v * 3 + 0], decoded.positions[v].x);
        TEST_ASSERT_EQUAL_FLOAT(k_cube_vertices[v * 3 + 1], decoded.positions[v].y);
        TEST_ASSERT_EQUAL_FLOAT(k_cube_vertices[v * 3 + 2], decoded.positions[v].z);
    }

    /* Position-only layout: every other typed array must be NULL. */
    TEST_ASSERT_NULL(decoded.normals);
    TEST_ASSERT_NULL(decoded.tangents);
    TEST_ASSERT_NULL(decoded.bitangents);
    TEST_ASSERT_NULL(decoded.bone_indices);
    TEST_ASSERT_NULL(decoded.bone_weights);
    for (size_t i = 0; i < 8; ++i) {
        TEST_ASSERT_NULL(decoded.uvs[i]);
    }
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_NULL(decoded.colors[i]);
    }

    sf_flver2_decoded_mesh_free(&decoded, sf_default_allocator());
    sf_flver2_destroy(flver);
}

/* ── T2: index buffer matches the cube fixture (triangle list, LOD0) ────── */

static void test_decode_mesh_unit_cube_indices(void)
{
    sf_flver2_t *flver = build_and_round_trip();
    TEST_ASSERT_NOT_NULL(flver);

    sf_flver2_decoded_mesh_t decoded;
    memset(&decoded, 0, sizeof(decoded));

    sf_result_t r = sf_flver2_decode_mesh(flver, 0, &decoded, sf_default_allocator());
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    /* Face set has triangle_strip=false, so triangulation is identity. */
    TEST_ASSERT_EQUAL_UINT32(12u, decoded.index_count);
    TEST_ASSERT_NOT_NULL(decoded.indices);
    for (size_t i = 0; i < 12; ++i) {
        TEST_ASSERT_EQUAL_UINT32(k_cube_indices[i], decoded.indices[i]);
    }

    sf_flver2_decoded_mesh_free(&decoded, sf_default_allocator());
    sf_flver2_destroy(flver);
}

/* ── T3: decoded_mesh_free zeroes the struct, NULL-safe ─────────────────── */

static void test_decoded_mesh_free_zeros_struct(void)
{
    sf_flver2_t *flver = build_and_round_trip();
    TEST_ASSERT_NOT_NULL(flver);

    sf_flver2_decoded_mesh_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_flver2_decode_mesh(flver, 0, &decoded, sf_default_allocator()));
    TEST_ASSERT_GREATER_THAN(0, (int)decoded.vertex_count);

    sf_flver2_decoded_mesh_free(&decoded, sf_default_allocator());
    /* Free must zero every field so a subsequent call (or aliased usage)
     * cannot reach freed memory. */
    TEST_ASSERT_EQUAL_UINT32(0u, decoded.vertex_count);
    TEST_ASSERT_EQUAL_UINT32(0u, decoded.index_count);
    TEST_ASSERT_NULL(decoded.positions);
    TEST_ASSERT_NULL(decoded.indices);
    for (size_t i = 0; i < 8; ++i) {
        TEST_ASSERT_NULL(decoded.uvs[i]);
    }

    /* NULL-safe per the public contract. */
    sf_flver2_decoded_mesh_free(NULL, sf_default_allocator());

    sf_flver2_destroy(flver);
}

/* ── T4: invalid arguments are rejected ─────────────────────────────────── */

static void test_decode_mesh_invalid_args(void)
{
    sf_flver2_t *flver = build_and_round_trip();
    TEST_ASSERT_NOT_NULL(flver);

    sf_flver2_decoded_mesh_t decoded;
    memset(&decoded, 0, sizeof(decoded));

    /* NULL FLVER */
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_flver2_decode_mesh(NULL, 0, &decoded, sf_default_allocator()));

    /* NULL out */
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_flver2_decode_mesh(flver, 0, NULL, sf_default_allocator()));

    /* NULL allocator (decode_mesh contractually requires a non-NULL allocator
     * because it must own the heap arrays it produces). */
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_flver2_decode_mesh(flver, 0, &decoded, NULL));

    sf_flver2_destroy(flver);
}

/* ── T5: out-of-range mesh_index is rejected ────────────────────────────── */

static void test_decode_mesh_out_of_range(void)
{
    sf_flver2_t *flver = build_and_round_trip();
    TEST_ASSERT_NOT_NULL(flver);

    sf_flver2_decoded_mesh_t decoded;
    memset(&decoded, 0, sizeof(decoded));

    /* Only one mesh exists; index 1 and 99 must fail with OUT_OF_RANGE. */
    TEST_ASSERT_EQUAL_INT(SF_ERR_OUT_OF_RANGE,
        sf_flver2_decode_mesh(flver, 1, &decoded, sf_default_allocator()));
    TEST_ASSERT_EQUAL_INT(SF_ERR_OUT_OF_RANGE,
        sf_flver2_decode_mesh(flver, 99, &decoded, sf_default_allocator()));

    sf_flver2_destroy(flver);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_decode_mesh_unit_cube_positions);
    RUN_TEST(test_decode_mesh_unit_cube_indices);
    RUN_TEST(test_decoded_mesh_free_zeros_struct);
    RUN_TEST(test_decode_mesh_invalid_args);
    RUN_TEST(test_decode_mesh_out_of_range);
    return UNITY_END();
}
