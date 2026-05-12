/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_mdl.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void wr16(uint8_t *p, uint32_t off, uint16_t v) {
    p[off] = (uint8_t)v;
    p[off + 1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t off, uint32_t v) {
    p[off] = (uint8_t)v;
    p[off + 1] = (uint8_t)(v >> 8);
    p[off + 2] = (uint8_t)(v >> 16);
    p[off + 3] = (uint8_t)(v >> 24);
}

static void test_mdl_read_smoke(void) {
    uint8_t bytes[0x60 + 6 + 0x40 + 0x80 + 8];
    memset(bytes, 0, sizeof(bytes));
    wr32(bytes, 0x00, sizeof(bytes));
    memcpy(bytes + 0x04, "MDL ", 4);
    wr16(bytes, 0x08, 1);
    wr16(bytes, 0x0A, 1);
    wr32(bytes, 0x0C, 11);
    wr32(bytes, 0x10, 12);
    wr32(bytes, 0x14, 13);
    wr32(bytes, 0x18, 0); /* bones */
    wr32(bytes, 0x1C, 3); /* indices */
    wr32(bytes, 0x20, 1); /* vertex A */
    wr32(bytes, 0x24, 0);
    wr32(bytes, 0x28, 0);
    wr32(bytes, 0x2C, 0);
    wr32(bytes, 0x30, 0);
    wr32(bytes, 0x34, 1); /* materials */
    wr32(bytes, 0x38, 1); /* textures */
    uint32_t off = 0x60;
    wr32(bytes, 0x3C, off);      /* nodes */
    wr32(bytes, 0x40, off);      /* indices */
    wr16(bytes, off + 0, 0);
    wr16(bytes, off + 2, 1);
    wr16(bytes, off + 4, 2);
    off += 6;
    wr32(bytes, 0x44, off);      /* vertex A */
    off += 0x40;
    wr32(bytes, 0x48, off);      /* vertex B */
    wr32(bytes, 0x4C, off);      /* vertex C */
    wr32(bytes, 0x50, off);      /* vertex D */
    wr32(bytes, 0x54, off);      /* dummies */
    wr32(bytes, 0x58, off);      /* materials */
    off += 0x80;
    wr32(bytes, 0x5C, off);      /* textures */
    memcpy(bytes + off, "tex\0", 4);

    sf_mdl_t *mdl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mdl_read_from_memory(&mdl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_EQUAL_INT32(11, sf_mdl_unk0c(mdl));
    TEST_ASSERT_EQUAL_size_t(3u, sf_mdl_index_count(mdl));
    TEST_ASSERT_EQUAL_size_t(1u, sf_mdl_vertex_count_a(mdl));
    TEST_ASSERT_EQUAL_size_t(1u, sf_mdl_material_count(mdl));
    TEST_ASSERT_EQUAL_size_t(1u, sf_mdl_texture_count(mdl));
    sf_mdl_destroy(mdl);
}

static void test_mdl0_read_smoke(void) {
    uint8_t bytes[0x50];
    memset(bytes, 0, sizeof(bytes));
    wr32(bytes, 0x00, sizeof(bytes));
    wr32(bytes, 0x04, 21);
    wr32(bytes, 0x08, 22);
    wr32(bytes, 0x0C, 0);
    wr32(bytes, 0x30, 0x50);
    wr32(bytes, 0x34, 0x50);
    wr32(bytes, 0x38, 0x50);
    wr32(bytes, 0x3C, 0x50);
    wr32(bytes, 0x40, 0x50);
    wr32(bytes, 0x44, 0x50);
    wr32(bytes, 0x48, 0x50);
    wr32(bytes, 0x4C, 0x50);
    sf_mdl0_t *mdl0 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mdl0_read_from_memory(&mdl0, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_EQUAL_INT32(21, sf_mdl0_unk04(mdl0));
    TEST_ASSERT_EQUAL_INT32(22, sf_mdl0_unk08(mdl0));
    TEST_ASSERT_EQUAL_size_t(0u, sf_mdl0_node_count(mdl0));
    sf_mdl0_destroy(mdl0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mdl_read_smoke);
    RUN_TEST(test_mdl0_read_smoke);
    return UNITY_END();
}
