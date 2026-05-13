/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * EDGE synthetic round-trip test: build a small EDGE in memory, write to
 * bytes, read back, verify all fields match, write again, byte-level equal.
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_edge.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_edge_synthetic_roundtrip(void) {
    sf_edge_file_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edge_create_empty(&src, NULL));
    TEST_ASSERT_NOT_NULL(src);
    sf_edge_set_id(src, 42);

    sf_edge_t *e0 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edge_append(src, &e0));
    sf_vec3_t v1a = {1.0f, 2.0f, 3.0f};
    sf_vec3_t v2a = {4.0f, 5.0f, 6.0f};
    sf_vec3_t v3a = {7.0f, 8.0f, 9.0f};
    sf_edge_set_v1(e0, v1a);
    sf_edge_set_v2(e0, v2a);
    sf_edge_set_v3(e0, v3a);
    sf_edge_set_unk2c(e0, 0.5f);
    sf_edge_set_unk30(e0, 100);
    sf_edge_set_edge_type(e0, SF_EDGE_TYPE_GRAPPLE);
    sf_edge_set_variation_id(e0, 7);
    sf_edge_set_unk36(e0, 9);

    sf_edge_t *e1 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edge_append(src, &e1));
    sf_vec3_t v1b = {-1.0f, -2.0f, -3.0f};
    sf_vec3_t v2b = {-4.0f, -5.0f, -6.0f};
    sf_vec3_t v3b = {-7.0f, -8.0f, -9.0f};
    sf_edge_set_v1(e1, v1b);
    sf_edge_set_v2(e1, v2b);
    sf_edge_set_v3(e1, v3b);
    sf_edge_set_unk2c(e1, 2.0f);
    sf_edge_set_unk30(e1, -1);
    sf_edge_set_edge_type(e1, SF_EDGE_TYPE_HANG);
    sf_edge_set_variation_id(e1, 1);
    sf_edge_set_unk36(e1, 2);

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edge_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_size_t(16 + 2 * 64, size);

    sf_edge_file_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edge_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(parsed);

    TEST_ASSERT_EQUAL_INT(42, sf_edge_id(parsed));
    TEST_ASSERT_EQUAL_size_t(2, sf_edge_count(parsed));

    const sf_edge_t *p0 = sf_edge_get(parsed, 0);
    TEST_ASSERT_NOT_NULL(p0);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_edge_v1(p0).x);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, sf_edge_v1(p0).z);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, sf_edge_unk2c(p0));
    TEST_ASSERT_EQUAL_INT(100, sf_edge_unk30(p0));
    TEST_ASSERT_EQUAL_INT(SF_EDGE_TYPE_GRAPPLE, sf_edge_edge_type(p0));
    TEST_ASSERT_EQUAL_UINT8(7, sf_edge_variation_id(p0));
    TEST_ASSERT_EQUAL_UINT8(9, sf_edge_unk36(p0));

    const sf_edge_t *p1 = sf_edge_get(parsed, 1);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, sf_edge_v1(p1).x);
    TEST_ASSERT_EQUAL_FLOAT(-9.0f, sf_edge_v3(p1).z);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_edge_unk2c(p1));
    TEST_ASSERT_EQUAL_INT(-1, sf_edge_unk30(p1));
    TEST_ASSERT_EQUAL_INT(SF_EDGE_TYPE_HANG, sf_edge_edge_type(p1));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edge_write_to_memory(parsed, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_edge_destroy(parsed);
    sf_edge_destroy(src);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_edge_synthetic_roundtrip);
    return UNITY_END();
}
