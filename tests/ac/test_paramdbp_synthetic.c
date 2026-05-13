/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_paramdbp.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_paramdbp_create_destroy(void) {
    sf_paramdbp_t *d = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_create(&d, true, NULL));
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_TRUE(sf_paramdbp_is_big_endian(d));
    TEST_ASSERT_EQUAL_size_t(0, sf_paramdbp_field_count(d));
    TEST_ASSERT_EQUAL_INT(0, sf_paramdbp_calculate_param_size(d));
    sf_paramdbp_destroy(d);
}

static void test_paramdbp_add_field(void) {
    sf_paramdbp_t *d = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_create(&d, true, NULL));

    sf_paramdbp_field_t f = {0};
    f.type = SF_DBP_TYPE_S32;
    f.display_name = "TestField";
    f.display_format = "%d";
    f.default_val.s32 = 0;
    f.increment.s32 = 1;
    f.minimum.s32 = -100;
    f.maximum.s32 = 100;

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_add_field(d, &f));
    TEST_ASSERT_EQUAL_size_t(1, sf_paramdbp_field_count(d));
    TEST_ASSERT_EQUAL_INT(4, sf_paramdbp_calculate_param_size(d));

    const sf_paramdbp_field_t *got = sf_paramdbp_get_field(d, 0);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT(SF_DBP_TYPE_S32, got->type);
    TEST_ASSERT_EQUAL_STRING("TestField", got->display_name);
    TEST_ASSERT_EQUAL_INT32(-100, got->minimum.s32);
    TEST_ASSERT_EQUAL_INT32(100, got->maximum.s32);

    sf_paramdbp_destroy(d);
}

static void test_paramdbp_round_trip(void) {
    sf_paramdbp_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_create(&a, true, NULL));

    sf_paramdbp_field_t f1 = {0};
    f1.type = SF_DBP_TYPE_U8;
    f1.display_name = "ByteField";
    f1.display_format = "%d";
    f1.default_val.u8 = 5;
    f1.increment.u8 = 1;
    f1.minimum.u8 = 0;
    f1.maximum.u8 = 255;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_add_field(a, &f1));

    sf_paramdbp_field_t f2 = {0};
    f2.type = SF_DBP_TYPE_F32;
    f2.display_name = "FloatField";
    f2.display_format = "%f";
    f2.default_val.f32 = 1.0f;
    f2.increment.f32 = 0.01f;
    f2.minimum.f32 = 0.0f;
    f2.maximum.f32 = 100.0f;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_add_field(a, &f2));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);

    sf_paramdbp_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_size_t(2, sf_paramdbp_field_count(b));
    TEST_ASSERT_EQUAL_INT(5, sf_paramdbp_calculate_param_size(b));

    const sf_paramdbp_field_t *g1 = sf_paramdbp_get_field(b, 0);
    TEST_ASSERT_EQUAL_INT(SF_DBP_TYPE_U8, g1->type);
    TEST_ASSERT_EQUAL_STRING("ByteField", g1->display_name);
    TEST_ASSERT_EQUAL_UINT8(5, g1->default_val.u8);

    const sf_paramdbp_field_t *g2 = sf_paramdbp_get_field(b, 1);
    TEST_ASSERT_EQUAL_INT(SF_DBP_TYPE_F32, g2->type);
    TEST_ASSERT_EQUAL_STRING("FloatField", g2->display_name);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, g2->default_val.f32);

    sf_free(NULL, bytes);
    sf_paramdbp_destroy(b);
    sf_paramdbp_destroy(a);
}

static void test_dbpparam_apply_and_read(void) {
    sf_paramdbp_t *dbp = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_create(&dbp, true, NULL));

    sf_paramdbp_field_t f1 = {0};
    f1.type = SF_DBP_TYPE_S16;
    f1.display_name = "HP";
    f1.display_format = "%d";
    f1.default_val.s16 = 0;
    f1.increment.s16 = 1;
    f1.minimum.s16 = -32768;
    f1.maximum.s16 = 32767;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_add_field(dbp, &f1));

    sf_paramdbp_field_t f2 = {0};
    f2.type = SF_DBP_TYPE_U8;
    f2.display_name = "Level";
    f2.display_format = "%d";
    f2.default_val.u8 = 1;
    f2.increment.u8 = 1;
    f2.minimum.u8 = 0;
    f2.maximum.u8 = 255;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_paramdbp_add_field(dbp, &f2));

    uint8_t raw[3];
    int16_t hp = 500;
    memcpy(raw, &hp, 2);
    raw[2] = 42;

    sf_dbpparam_t *param = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dbpparam_read_from_memory(&param, raw, sizeof(raw), NULL));
    TEST_ASSERT_FALSE(sf_dbpparam_is_applied(param));

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dbpparam_apply_paramdbp(param, dbp));
    TEST_ASSERT_TRUE(sf_dbpparam_is_applied(param));
    TEST_ASSERT_EQUAL_size_t(2, sf_dbpparam_cell_count(param));

    const sf_dbpparam_cell_t *c1 = sf_dbpparam_get_cell(param, 0);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_EQUAL_INT16(500, c1->value.s16);

    const sf_dbpparam_cell_t *c2 = sf_dbpparam_get_cell(param, 1);
    TEST_ASSERT_NOT_NULL(c2);
    TEST_ASSERT_EQUAL_UINT8(42, c2->value.u8);

    void *out_bytes = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dbpparam_write_to_memory(param, &out_bytes, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(3, out_size);
    TEST_ASSERT_EQUAL_MEMORY(raw, out_bytes, 3);

    sf_free(NULL, out_bytes);
    sf_dbpparam_destroy(param);
    sf_paramdbp_destroy(dbp);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_paramdbp_create_destroy);
    RUN_TEST(test_paramdbp_add_field);
    RUN_TEST(test_paramdbp_round_trip);
    RUN_TEST(test_dbpparam_apply_and_read);
    return UNITY_END();
}
