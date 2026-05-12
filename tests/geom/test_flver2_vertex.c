#include "unity.h"
#include "internal/flver2_internal.h"
#include "souls_formats/sf_flver.h"
#include "souls_formats/sf_math.h"

#include <string.h>

void test_flver2_vertex_roundtrip(void);
void test_flver2_vertex_edge_compressed(void);
void test_flver2_vertex_unknown_type(void);
void setUp(void) {}
void tearDown(void) {}

void test_flver2_vertex_roundtrip(void) {
    sf_flver2_layout_member_t members[] = {
        {0, 0, SF_FLVER_LAYOUT_TYPE_FLOAT3, SF_FLVER_LAYOUT_SEMANTIC_POSITION, 0, 0},
        {0, 12, SF_FLVER_LAYOUT_TYPE_BYTE4, SF_FLVER_LAYOUT_SEMANTIC_NORMAL, 0, 0}
    };
    sf_flver2_buffer_layout_t layout = {members, 2};

    sf_flver2_vertex_context_t ctx = {1024.0f, false, 0x2001A};

    /* 16 bytes per vertex */
    uint8_t original_bytes[16] = {
        0x00, 0x00, 0x80, 0x3F, /* x = 1.0f */
        0x00, 0x00, 0x00, 0x00, /* y = 0.0f */
        0x00, 0x00, 0x80, 0xBF, /* z = -1.0f */
        0x01, /* normal_w = 1 */
        0x7F, /* normal.z = 1.0f */
        0x00, /* normal.y = 0.0f */
        0x81  /* normal.x = -1.0f */
    };

    sf_flver2_decoded_vertex_t decoded;
    sf_result_t res = sfi_flver2_vertex_decode_one(&layout, original_bytes, &ctx, &decoded);
    TEST_ASSERT_EQUAL(SF_OK, res);

    TEST_ASSERT_EQUAL_FLOAT(1.0f, decoded.position.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, decoded.position.y);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, decoded.position.z);

    TEST_ASSERT_EQUAL_FLOAT(-1.0f, decoded.normal.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, decoded.normal.y);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, decoded.normal.z);
    TEST_ASSERT_EQUAL(1, decoded.normal_w);

    uint8_t encoded_bytes[16] = {0};
    res = sfi_flver2_vertex_encode_one(&layout, &decoded, &ctx, encoded_bytes);
    TEST_ASSERT_EQUAL(SF_OK, res);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(original_bytes, encoded_bytes, 16);
}

void test_flver2_vertex_edge_compressed(void) {
    sf_flver2_layout_member_t members[] = {
        {0, 0, SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED, SF_FLVER_LAYOUT_SEMANTIC_POSITION, 0, 0}
    };
    sf_flver2_buffer_layout_t layout = {members, 1};
    sf_flver2_vertex_context_t ctx = {1024.0f, false, 0x2001A};
    uint8_t bytes[16] = {0};
    sf_flver2_decoded_vertex_t decoded;

    sf_result_t res = sfi_flver2_vertex_decode_one(&layout, bytes, &ctx, &decoded);
    TEST_ASSERT_EQUAL(SF_ERR_UNSUPPORTED_VERSION, res);
}

void test_flver2_vertex_unknown_type(void) {
    sf_flver2_layout_member_t members[] = {
        {0, 0, (sf_flver_layout_type_t)0xFF, SF_FLVER_LAYOUT_SEMANTIC_POSITION, 0, 0}
    };
    sf_flver2_buffer_layout_t layout = {members, 1};
    sf_flver2_vertex_context_t ctx = {1024.0f, false, 0x2001A};
    uint8_t bytes[16] = {0};
    sf_flver2_decoded_vertex_t decoded;

    sf_result_t res = sfi_flver2_vertex_decode_one(&layout, bytes, &ctx, &decoded);
    TEST_ASSERT_EQUAL(SF_ERR_UNSUPPORTED_VERSION, res);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flver2_vertex_roundtrip);
    RUN_TEST(test_flver2_vertex_edge_compressed);
    RUN_TEST(test_flver2_vertex_unknown_type);
    return UNITY_END();
}
