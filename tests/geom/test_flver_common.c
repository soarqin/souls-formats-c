/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T8 — FLVER common helpers: half-float, 11_11_10 packing,
 * LayoutType sizing, Dummy/Node/LayoutMember round-trip.
 */

#include "souls_formats/sf_flver.h"
#include "souls_formats/sf_io.h"

#include "internal/flver_common_internal.h"

#include "unity.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * Half-float — IEEE 754 binary16 golden vectors
 *===========================================================================*/

static void test_half_to_float_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf_half_to_float(0x0000));
}

static void test_half_to_float_neg_zero(void) {
    float v = sf_half_to_float(0x8000);
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    TEST_ASSERT_EQUAL_HEX32(0x80000000u, bits);
}

static void test_half_to_float_one(void) {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_half_to_float(0x3C00));
}

static void test_half_to_float_neg_one(void) {
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, sf_half_to_float(0xBC00));
}

static void test_half_to_float_two(void) {
    TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_half_to_float(0x4000));
}

static void test_half_to_float_half(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.5f, sf_half_to_float(0x3800));
}

static void test_half_to_float_max_normal(void) {
    /* 0x7BFF = 65504, largest binary16 normal */
    TEST_ASSERT_EQUAL_FLOAT(65504.0f, sf_half_to_float(0x7BFF));
}

static void test_half_to_float_min_normal(void) {
    /* 0x0400 = 2^-14 ≈ 6.1035e-5 */
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, ldexpf(1.0f, -14), sf_half_to_float(0x0400));
}

static void test_half_to_float_inf(void) {
    TEST_ASSERT_TRUE(isinf(sf_half_to_float(0x7C00)));
    TEST_ASSERT_TRUE(sf_half_to_float(0x7C00) > 0.0f);
}

static void test_half_to_float_neg_inf(void) {
    TEST_ASSERT_TRUE(isinf(sf_half_to_float(0xFC00)));
    TEST_ASSERT_TRUE(sf_half_to_float(0xFC00) < 0.0f);
}

static void test_half_to_float_nan(void) {
    TEST_ASSERT_TRUE(isnan(sf_half_to_float(0x7E00)));
}

static void test_half_to_float_subnormal(void) {
    /* 0x0001 = smallest positive subnormal: 2^-24 ≈ 5.96e-8 */
    float v = sf_half_to_float(0x0001);
    TEST_ASSERT_TRUE(v > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-12f, ldexpf(1.0f, -24), v);
}

static void test_half_to_float_neg_subnormal(void) {
    float v = sf_half_to_float(0x8001);
    TEST_ASSERT_TRUE(v < 0.0f);
}

static void test_half_to_float_largest_subnormal(void) {
    /* 0x03FF — largest subnormal: (1023/1024) * 2^-14 */
    float v = sf_half_to_float(0x03FF);
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, (1023.0f / 1024.0f) * ldexpf(1.0f, -14), v);
}

static void test_float_to_half_zero(void) {
    TEST_ASSERT_EQUAL_HEX16(0x0000, sf_float_to_half(0.0f));
    TEST_ASSERT_EQUAL_HEX16(0x8000, sf_float_to_half(-0.0f));
}

static void test_float_to_half_one(void) {
    TEST_ASSERT_EQUAL_HEX16(0x3C00, sf_float_to_half(1.0f));
    TEST_ASSERT_EQUAL_HEX16(0xBC00, sf_float_to_half(-1.0f));
}

static void test_float_to_half_roundtrip_simple(void) {
    /* Values exactly representable in binary16 */
    TEST_ASSERT_EQUAL_FLOAT(1.5f,  sf_half_to_float(sf_float_to_half(1.5f)));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, sf_half_to_float(sf_float_to_half(0.25f)));
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, sf_half_to_float(sf_float_to_half(-2.0f)));
}

static void test_float_to_half_inf(void) {
    TEST_ASSERT_EQUAL_HEX16(0x7C00, sf_float_to_half(INFINITY));
    TEST_ASSERT_EQUAL_HEX16(0xFC00, sf_float_to_half(-INFINITY));
}

static void test_float_to_half_nan(void) {
    uint16_t h = sf_float_to_half(NAN);
    /* exponent all ones, mantissa non-zero */
    TEST_ASSERT_EQUAL_HEX16(0x7C00, h & 0x7C00);
    TEST_ASSERT_NOT_EQUAL(0, h & 0x03FF);
}

static void test_float_to_half_overflow(void) {
    /* 70000 > 65504 → +inf */
    TEST_ASSERT_EQUAL_HEX16(0x7C00, sf_float_to_half(70000.0f));
}

/*===========================================================================
 * 11_11_10 packed normal — golden + round-trip
 *===========================================================================*/

static void test_11_11_10_zero(void) {
    float x = 9.0f, y = 9.0f, z = 9.0f;
    sf_unpack_11_11_10(0u, &x, &y, &z);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, y);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, z);
}

static void test_11_11_10_unit_x(void) {
    uint32_t p = sf_pack_11_11_10(1.0f, 0.0f, 0.0f);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sf_unpack_11_11_10(p, &x, &y, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, z);
}

static void test_11_11_10_neg_x(void) {
    uint32_t p = sf_pack_11_11_10(-1.0f, 0.0f, 0.0f);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sf_unpack_11_11_10(p, &x, &y, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  0.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  0.0f, z);
}

static void test_11_11_10_unit_y(void) {
    uint32_t p = sf_pack_11_11_10(0.0f, 1.0f, 0.0f);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sf_unpack_11_11_10(p, &x, &y, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, z);
}

static void test_11_11_10_unit_z(void) {
    uint32_t p = sf_pack_11_11_10(0.0f, 0.0f, 1.0f);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sf_unpack_11_11_10(p, &x, &y, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, z);
}

static void test_11_11_10_roundtrip_mixed(void) {
    uint32_t p = sf_pack_11_11_10(0.5f, -0.5f, 0.25f);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sf_unpack_11_11_10(p, &x, &y, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.002f,  0.5f,  x);
    TEST_ASSERT_FLOAT_WITHIN(0.002f, -0.5f,  y);
    TEST_ASSERT_FLOAT_WITHIN(0.002f,  0.25f, z);
}

static void test_11_11_10_diagonal(void) {
    uint32_t p = sf_pack_11_11_10(0.5f, 0.5f, 0.5f);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sf_unpack_11_11_10(p, &x, &y, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.5f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.5f, y);
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.5f, z);
}

static void test_11_11_10_clamp_extremes(void) {
    /* Pack beyond [-1, 1]; expect saturation. */
    uint32_t p = sf_pack_11_11_10(2.0f, -2.0f, 2.0f);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sf_unpack_11_11_10(p, &x, &y, &z);
    /* 1023/1023 = 1.0 (max +), -1024/1023 ≈ -1.0009 (min -), 511/511 = 1.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.01f,  1.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,  1.0f, z);
}

/*===========================================================================
 * LayoutType size lookup
 *===========================================================================*/

static void test_layout_type_size_basic(void) {
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_FLOAT1, 0));
    TEST_ASSERT_EQUAL_UINT32(8u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_FLOAT2, 0));
    TEST_ASSERT_EQUAL_UINT32(12u, sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_FLOAT3, 0));
    TEST_ASSERT_EQUAL_UINT32(16u, sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_FLOAT4, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_COLOR, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_UBYTE4, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_BYTE4, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_BYTE4_NORM, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_SHORT2, 0));
    TEST_ASSERT_EQUAL_UINT32(8u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_SHORT4, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_USHORT2, 0));
    TEST_ASSERT_EQUAL_UINT32(8u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_USHORT4, 0));
    TEST_ASSERT_EQUAL_UINT32(8u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_SHORT4_NORM, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_HALF2, 0));
    TEST_ASSERT_EQUAL_UINT32(8u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_HALF4, 0));
    TEST_ASSERT_EQUAL_UINT32(4u,  sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_BYTE4E, 0));
}

static void test_layout_type_size_speedtree_sentinel(void) {
    TEST_ASSERT_EQUAL_UINT32(0u, sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_FLOAT3, -32768));
    TEST_ASSERT_EQUAL_UINT32(0u, sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_HALF4,  -32768));
}

static void test_layout_type_size_edge_compressed_sentinel(void) {
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX,
        sf_flver_layout_type_size(SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED, 0));
}

/*===========================================================================
 * Dummy round-trip
 *===========================================================================*/

static void test_dummy_roundtrip(void) {
    sf_flver_dummy_t d_in = {
        .position           = { 1.0f, 2.0f, 3.0f },
        .color              = 0xAABBCCDDu,
        .forward            = { -1.0f, 0.0f, 0.0f },
        .reference_id       = 0x1234,
        .parent_bone_index  = 12,
        .upward             = { 0.0f, 1.0f, 0.0f },
        .attach_bone_index  = -1,
        .flag1              = true,
        .use_upward_vector  = false,
        .unk30              = 0x55667788,
        .unk34              = -0x12345678,
    };

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&os, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&bw, os, false, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_dummy_write(bw, &d_in));
    TEST_ASSERT_EQUAL_INT64(SFI_FLVER_DUMMY_SIZE, sf_binary_writer_position(bw));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(bw));
    sf_binary_writer_destroy(bw);

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(os, &bytes, &n));
    TEST_ASSERT_EQUAL_UINT64(SFI_FLVER_DUMMY_SIZE, n);

    sf_binary_reader_t *br = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create_from_memory(&br, false, bytes, n, NULL));
    sf_flver_dummy_t d_out;
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_dummy_read(br, &d_out));

    TEST_ASSERT_EQUAL_FLOAT(d_in.position.x, d_out.position.x);
    TEST_ASSERT_EQUAL_FLOAT(d_in.position.y, d_out.position.y);
    TEST_ASSERT_EQUAL_FLOAT(d_in.position.z, d_out.position.z);
    TEST_ASSERT_EQUAL_HEX32(d_in.color, d_out.color);
    TEST_ASSERT_EQUAL_FLOAT(d_in.forward.x, d_out.forward.x);
    TEST_ASSERT_EQUAL_FLOAT(d_in.forward.y, d_out.forward.y);
    TEST_ASSERT_EQUAL_FLOAT(d_in.forward.z, d_out.forward.z);
    TEST_ASSERT_EQUAL_INT16(d_in.reference_id,      d_out.reference_id);
    TEST_ASSERT_EQUAL_INT16(d_in.parent_bone_index, d_out.parent_bone_index);
    TEST_ASSERT_EQUAL_FLOAT(d_in.upward.x, d_out.upward.x);
    TEST_ASSERT_EQUAL_FLOAT(d_in.upward.y, d_out.upward.y);
    TEST_ASSERT_EQUAL_FLOAT(d_in.upward.z, d_out.upward.z);
    TEST_ASSERT_EQUAL_INT16(d_in.attach_bone_index, d_out.attach_bone_index);
    TEST_ASSERT_EQUAL(d_in.flag1,             d_out.flag1);
    TEST_ASSERT_EQUAL(d_in.use_upward_vector, d_out.use_upward_vector);
    TEST_ASSERT_EQUAL_INT32(d_in.unk30, d_out.unk30);
    TEST_ASSERT_EQUAL_INT32(d_in.unk34, d_out.unk34);

    sf_binary_reader_destroy(br);
    sf_ostream_close(os);
}

/*===========================================================================
 * Node round-trip
 *===========================================================================*/

static void test_node_roundtrip_shift_jis(void) {
    sf_flver_node_t n_in = {
        .name                   = NULL,
        .parent_index           = 3,
        .first_child_index      = 4,
        .next_sibling_index     = 5,
        .previous_sibling_index = 6,
        .translation            = { 1.0f, 2.0f, 3.0f },
        .rotation               = { 0.1f, 0.2f, 0.3f },
        .scale                  = { 1.0f, 1.0f, 1.0f },
        .bbox_min               = { -1.0f, -2.0f, -3.0f },
        .bbox_max               = {  1.0f,  2.0f,  3.0f },
        .flags                  = SF_FLVER_NODE_FLAG_BONE | SF_FLVER_NODE_FLAG_MESH,
    };

    /* Layout in test buffer:
     *   [0..128)   : Node record (name_offset = 128)
     *   [128..N)   : Shift-JIS name "Spine\0"
     */
    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&os, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&bw, os, false, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_node_write_with_offset(bw, &n_in, 128));
    TEST_ASSERT_EQUAL_INT64(SFI_FLVER_NODE_SIZE, sf_binary_writer_position(bw));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_shift_jis(bw, "Spine", true));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(bw));
    sf_binary_writer_destroy(bw);

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(os, &bytes, &n));
    TEST_ASSERT_TRUE(n >= SFI_FLVER_NODE_SIZE);

    sf_binary_reader_t *br = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create_from_memory(&br, false, bytes, n, NULL));
    sf_flver_node_t n_out = {0};
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_node_read(br, false, &n_out, NULL));

    TEST_ASSERT_NOT_NULL(n_out.name);
    TEST_ASSERT_EQUAL_STRING("Spine", n_out.name);
    TEST_ASSERT_EQUAL_INT16(n_in.parent_index,           n_out.parent_index);
    TEST_ASSERT_EQUAL_INT16(n_in.first_child_index,      n_out.first_child_index);
    TEST_ASSERT_EQUAL_INT16(n_in.next_sibling_index,     n_out.next_sibling_index);
    TEST_ASSERT_EQUAL_INT16(n_in.previous_sibling_index, n_out.previous_sibling_index);
    TEST_ASSERT_EQUAL_FLOAT(n_in.translation.x, n_out.translation.x);
    TEST_ASSERT_EQUAL_FLOAT(n_in.translation.y, n_out.translation.y);
    TEST_ASSERT_EQUAL_FLOAT(n_in.translation.z, n_out.translation.z);
    TEST_ASSERT_EQUAL_FLOAT(n_in.rotation.x, n_out.rotation.x);
    TEST_ASSERT_EQUAL_FLOAT(n_in.rotation.y, n_out.rotation.y);
    TEST_ASSERT_EQUAL_FLOAT(n_in.rotation.z, n_out.rotation.z);
    TEST_ASSERT_EQUAL_FLOAT(n_in.scale.x, n_out.scale.x);
    TEST_ASSERT_EQUAL_FLOAT(n_in.scale.y, n_out.scale.y);
    TEST_ASSERT_EQUAL_FLOAT(n_in.scale.z, n_out.scale.z);
    TEST_ASSERT_EQUAL_FLOAT(n_in.bbox_min.x, n_out.bbox_min.x);
    TEST_ASSERT_EQUAL_FLOAT(n_in.bbox_min.y, n_out.bbox_min.y);
    TEST_ASSERT_EQUAL_FLOAT(n_in.bbox_min.z, n_out.bbox_min.z);
    TEST_ASSERT_EQUAL_FLOAT(n_in.bbox_max.x, n_out.bbox_max.x);
    TEST_ASSERT_EQUAL_FLOAT(n_in.bbox_max.y, n_out.bbox_max.y);
    TEST_ASSERT_EQUAL_FLOAT(n_in.bbox_max.z, n_out.bbox_max.z);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)n_in.flags, (uint32_t)n_out.flags);

    sfi_flver_node_destroy_inplace(&n_out, NULL);
    sf_binary_reader_destroy(br);
    sf_ostream_close(os);
}

static void test_node_roundtrip_unicode(void) {
    /* Verify UTF-16 path works too. */
    sf_flver_node_t n_in = {
        .name                   = NULL,
        .parent_index           = -1,
        .first_child_index      = -1,
        .next_sibling_index     = -1,
        .previous_sibling_index = -1,
        .translation            = { 0.0f, 0.0f, 0.0f },
        .rotation               = { 0.0f, 0.0f, 0.0f },
        .scale                  = { 1.0f, 1.0f, 1.0f },
        .bbox_min               = { 0.0f, 0.0f, 0.0f },
        .bbox_max               = { 0.0f, 0.0f, 0.0f },
        .flags                  = SF_FLVER_NODE_FLAG_BONE,
    };

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&os, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&bw, os, false, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_node_write_with_offset(bw, &n_in, 128));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_utf16(bw, "Hip", true));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(bw));
    sf_binary_writer_destroy(bw);

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(os, &bytes, &n));

    sf_binary_reader_t *br = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create_from_memory(&br, false, bytes, n, NULL));
    sf_flver_node_t n_out = {0};
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_node_read(br, true, &n_out, NULL));

    TEST_ASSERT_NOT_NULL(n_out.name);
    TEST_ASSERT_EQUAL_STRING("Hip", n_out.name);

    sfi_flver_node_destroy_inplace(&n_out, NULL);
    sf_binary_reader_destroy(br);
    sf_ostream_close(os);
}

/*===========================================================================
 * LayoutMember round-trip
 *===========================================================================*/

static void test_layout_member_roundtrip_normal(void) {
    sfi_flver_layout_member_t m_in = {
        .stream           = 0,
        .special_modifier = 0,
        .type             = SF_FLVER_LAYOUT_TYPE_FLOAT3,
        .semantic         = SF_FLVER_LAYOUT_SEMANTIC_POSITION,
        .index            = 0,
    };
    const int32_t struct_offset = 16;

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&os, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&bw, os, false, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_layout_member_write(bw, struct_offset, false, &m_in));
    TEST_ASSERT_EQUAL_INT64(SFI_FLVER_LAYOUT_MEMBER_SIZE, sf_binary_writer_position(bw));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(bw));
    sf_binary_writer_destroy(bw);

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(os, &bytes, &n));
    TEST_ASSERT_EQUAL_UINT64(SFI_FLVER_LAYOUT_MEMBER_SIZE, n);

    sf_binary_reader_t *br = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create_from_memory(&br, false, bytes, n, NULL));
    sfi_flver_layout_member_t m_out = {0};
    TEST_ASSERT_EQUAL(SF_OK,
        sfi_flver_layout_member_read(br, struct_offset, false, &m_out));

    TEST_ASSERT_EQUAL_INT32 (m_in.stream,           m_out.stream);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)m_in.type,     (uint32_t)m_out.type);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)m_in.semantic, (uint32_t)m_out.semantic);
    TEST_ASSERT_EQUAL_INT32 (m_in.index,            m_out.index);

    sf_binary_reader_destroy(br);
    sf_ostream_close(os);
}

static void test_layout_member_roundtrip_speedtree(void) {
    sfi_flver_layout_member_t m_in = {
        .stream           = 1,
        .special_modifier = -32768,
        .type             = SF_FLVER_LAYOUT_TYPE_HALF4,
        .semantic         = SF_FLVER_LAYOUT_SEMANTIC_UV,
        .index            = 2,
    };

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&os, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&bw, os, false, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_layout_member_write(bw, 0, true, &m_in));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(bw));
    sf_binary_writer_destroy(bw);

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(os, &bytes, &n));

    sf_binary_reader_t *br = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create_from_memory(&br, false, bytes, n, NULL));
    sfi_flver_layout_member_t m_out = {0};
    TEST_ASSERT_EQUAL(SF_OK, sfi_flver_layout_member_read(br, 0, true, &m_out));

    TEST_ASSERT_EQUAL_INT32 (m_in.stream,           m_out.stream);
    TEST_ASSERT_EQUAL_INT16 (m_in.special_modifier, m_out.special_modifier);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)m_in.type,     (uint32_t)m_out.type);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)m_in.semantic, (uint32_t)m_out.semantic);
    TEST_ASSERT_EQUAL_INT32 (m_in.index,            m_out.index);

    sf_binary_reader_destroy(br);
    sf_ostream_close(os);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    UNITY_BEGIN();
    RUN_TEST(test_half_to_float_zero);
    RUN_TEST(test_half_to_float_neg_zero);
    RUN_TEST(test_half_to_float_one);
    RUN_TEST(test_half_to_float_neg_one);
    RUN_TEST(test_half_to_float_two);
    RUN_TEST(test_half_to_float_half);
    RUN_TEST(test_half_to_float_max_normal);
    RUN_TEST(test_half_to_float_min_normal);
    RUN_TEST(test_half_to_float_inf);
    RUN_TEST(test_half_to_float_neg_inf);
    RUN_TEST(test_half_to_float_nan);
    RUN_TEST(test_half_to_float_subnormal);
    RUN_TEST(test_half_to_float_neg_subnormal);
    RUN_TEST(test_half_to_float_largest_subnormal);
    RUN_TEST(test_float_to_half_zero);
    RUN_TEST(test_float_to_half_one);
    RUN_TEST(test_float_to_half_roundtrip_simple);
    RUN_TEST(test_float_to_half_inf);
    RUN_TEST(test_float_to_half_nan);
    RUN_TEST(test_float_to_half_overflow);
    RUN_TEST(test_11_11_10_zero);
    RUN_TEST(test_11_11_10_unit_x);
    RUN_TEST(test_11_11_10_neg_x);
    RUN_TEST(test_11_11_10_unit_y);
    RUN_TEST(test_11_11_10_unit_z);
    RUN_TEST(test_11_11_10_roundtrip_mixed);
    RUN_TEST(test_11_11_10_diagonal);
    RUN_TEST(test_11_11_10_clamp_extremes);
    RUN_TEST(test_layout_type_size_basic);
    RUN_TEST(test_layout_type_size_speedtree_sentinel);
    RUN_TEST(test_layout_type_size_edge_compressed_sentinel);
    RUN_TEST(test_dummy_roundtrip);
    RUN_TEST(test_node_roundtrip_shift_jis);
    RUN_TEST(test_node_roundtrip_unicode);
    RUN_TEST(test_layout_member_roundtrip_normal);
    RUN_TEST(test_layout_member_roundtrip_speedtree);
    return UNITY_END();
}
