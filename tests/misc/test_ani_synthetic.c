/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_ani.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_math.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_ani_create_destroy(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_size_t(0, sf_ani_node_count(a));
    TEST_ASSERT_EQUAL_size_t(0, sf_ani_translation_count(a));
    TEST_ASSERT_EQUAL_size_t(0, sf_ani_rotation_count(a));
    sf_ani_destroy(a);
}

static void test_ani_is_function(void) {
    /* Magic 0x20051014 big-endian = bytes 20 05 10 14, then 60 zeros to reach 64. */
    uint8_t valid[64];
    memset(valid, 0, sizeof(valid));
    valid[0] = 0x20; valid[1] = 0x05; valid[2] = 0x10; valid[3] = 0x14;
    TEST_ASSERT_TRUE(sf_ani_is(valid, sizeof(valid)));

    uint8_t bad_magic[64];
    memset(bad_magic, 0, sizeof(bad_magic));
    bad_magic[0] = 0x14; bad_magic[1] = 0x10; bad_magic[2] = 0x05; bad_magic[3] = 0x20;
    TEST_ASSERT_FALSE(sf_ani_is(bad_magic, sizeof(bad_magic)));

    TEST_ASSERT_FALSE(sf_ani_is(valid, 32)); /* below min length */
    TEST_ASSERT_FALSE(sf_ani_is(NULL, 0));
}

static void test_ani_node_defaults(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));

    sf_ani_node_t *n = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_node(a, &n));
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_EQUAL_INT(SF_ANI_NODE_TYPE_GEOM, sf_ani_node_type(n));
    TEST_ASSERT_EQUAL_INT16(-1, sf_ani_node_geom_index(n));
    TEST_ASSERT_EQUAL_INT16(-1, sf_ani_node_parent_index(n));
    TEST_ASSERT_EQUAL_INT16(-1, sf_ani_node_first_child_index(n));
    TEST_ASSERT_EQUAL_INT16(-1, sf_ani_node_next_sibling_index(n));
    TEST_ASSERT_EQUAL_INT16(-1, sf_ani_node_unk_index_12(n));
    sf_vec3_t s = sf_ani_node_scale(n);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, s.x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, s.y);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, s.z);
    TEST_ASSERT_EQUAL_STRING("", sf_ani_node_name(n));
    TEST_ASSERT_NULL(sf_ani_node_animation(n));

    sf_ani_destroy(a);
}

static void test_ani_round_trip_minimal_no_nodes(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size >= 120);
    TEST_ASSERT_TRUE(sf_ani_is(bytes, size));

    sf_ani_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_size_t(0, sf_ani_node_count(b));
    TEST_ASSERT_EQUAL_size_t(0, sf_ani_translation_count(b));
    TEST_ASSERT_EQUAL_size_t(0, sf_ani_rotation_count(b));

    sf_free(NULL, bytes);
    sf_ani_destroy(b);
    sf_ani_destroy(a);
}

static void test_ani_round_trip_single_node_no_animation(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));

    sf_ani_node_t *n = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_node(a, &n));
    sf_ani_node_set_type(n, SF_ANI_NODE_TYPE_DUMMY);
    sf_ani_node_set_geom_index(n, 7);
    sf_ani_node_set_parent_index(n, -1);
    sf_ani_node_set_first_child_index(n, -1);
    sf_ani_node_set_next_sibling_index(n, -1);
    sf_ani_node_set_unk_index_12(n, -1);
    sf_vec3_t t = { 1.0f, 2.0f, 3.0f };
    sf_vec3_t r = { 0.25f, 0.5f, 0.75f };
    sf_vec3_t s = { 1.0f, 1.0f, 1.0f };
    sf_ani_node_set_translation(n, t);
    sf_ani_node_set_rotation(n, r);
    sf_ani_node_set_scale(n, s);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_node_set_name(n, "root"));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_TRUE(sf_ani_is(bytes, size));

    sf_ani_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_size_t(1, sf_ani_node_count(b));
    sf_ani_node_t *m = sf_ani_node_at(b, 0);
    TEST_ASSERT_EQUAL_INT(SF_ANI_NODE_TYPE_DUMMY, sf_ani_node_type(m));
    TEST_ASSERT_EQUAL_INT16(7, sf_ani_node_geom_index(m));
    TEST_ASSERT_EQUAL_INT16(-1, sf_ani_node_parent_index(m));
    TEST_ASSERT_EQUAL_STRING("root", sf_ani_node_name(m));
    sf_vec3_t tr = sf_ani_node_translation(m);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, tr.x);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, tr.y);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, tr.z);
    sf_vec3_t rt = sf_ani_node_rotation(m);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, rt.x);
    TEST_ASSERT_EQUAL_FLOAT(0.5f,  rt.y);
    TEST_ASSERT_EQUAL_FLOAT(0.75f, rt.z);
    sf_vec3_t sc = sf_ani_node_scale(m);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sc.x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sc.y);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sc.z);
    TEST_ASSERT_NULL(sf_ani_node_animation(m));

    sf_free(NULL, bytes);
    sf_ani_destroy(b);
    sf_ani_destroy(a);
}

static void test_ani_round_trip_with_translation_rotation_buffers(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));

    sf_vec3_t t0 = { 10.0f, 20.0f, 30.0f };
    sf_vec3_t t1 = { -1.0f, -2.0f, -3.0f };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_translation(a, t0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_translation(a, t1));

    /* Rotations: quantised to int16 * 1000 — pick clean values that won't
     * lose precision when round-tripped through that quantisation. */
    sf_vec3_t r0 = { 0.001f, 0.500f, -0.250f };
    sf_vec3_t r1 = { 1.234f, -2.000f, 0.123f };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_rotation(a, r0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_rotation(a, r1));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_write_to_memory(a, &bytes, &size, NULL));

    sf_ani_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_size_t(2, sf_ani_translation_count(b));
    TEST_ASSERT_EQUAL_size_t(2, sf_ani_rotation_count(b));

    sf_vec3_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_get_translation(b, 0, &got));
    TEST_ASSERT_EQUAL_FLOAT(10.0f, got.x);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, got.y);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, got.z);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_get_translation(b, 1, &got));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, got.x);
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, got.y);
    TEST_ASSERT_EQUAL_FLOAT(-3.0f, got.z);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_get_rotation(b, 0, &got));
    TEST_ASSERT_EQUAL_FLOAT(0.001f, got.x);
    TEST_ASSERT_EQUAL_FLOAT(0.500f, got.y);
    TEST_ASSERT_EQUAL_FLOAT(-0.250f, got.z);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_get_rotation(b, 1, &got));
    TEST_ASSERT_EQUAL_FLOAT(1.234f, got.x);
    TEST_ASSERT_EQUAL_FLOAT(-2.000f, got.y);
    TEST_ASSERT_EQUAL_FLOAT(0.123f, got.z);

    sf_free(NULL, bytes);
    sf_ani_destroy(b);
    sf_ani_destroy(a);
}

static void test_ani_round_trip_pos_rot_shorts_frames(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));

    /* Provide reachable indices so we can validate the frame contents. */
    sf_vec3_t tv = { 0.5f, 1.5f, 2.5f };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_translation(a, tv));
    sf_vec3_t rv = { 0.100f, -0.200f, 0.300f };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_rotation(a, rv));

    sf_ani_node_t *n = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_node(a, &n));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_node_set_name(n, "bone_0"));

    sf_ani_node_animation_t *anim = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_node_create_animation(n, &anim));
    sf_ani_animation_set_format(anim, SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS);
    sf_vec3_t u10 = { 0.1f, 0.2f, 0.3f };
    sf_vec3_t u20 = { 0.4f, 0.5f, 0.6f };
    sf_ani_animation_set_unk10(anim, u10);
    sf_ani_animation_set_unk20(anim, u20);

    sf_ani_frame_t f0 = { 0, 0, 0, 0, 0, 0, 0, 1 };
    sf_ani_frame_t f1 = { 30, 0, 0, 0, 0, 0, 0, 1 };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_add_frame(anim, f0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_add_frame(anim, f1));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_TRUE(sf_ani_is(bytes, size));

    sf_ani_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_read_from_memory(&b, bytes, size, NULL));

    TEST_ASSERT_EQUAL_size_t(1, sf_ani_node_count(b));
    sf_ani_node_t *m = sf_ani_node_at(b, 0);
    TEST_ASSERT_EQUAL_STRING("bone_0", sf_ani_node_name(m));
    sf_ani_node_animation_t *anim2 = sf_ani_node_animation(m);
    TEST_ASSERT_NOT_NULL(anim2);
    TEST_ASSERT_EQUAL_INT(SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS, sf_ani_animation_format(anim2));
    TEST_ASSERT_EQUAL_size_t(2, sf_ani_animation_frame_count(anim2));

    sf_ani_frame_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_get_frame(anim2, 0, &got));
    TEST_ASSERT_EQUAL_INT16(0,  got.key_frame);
    TEST_ASSERT_EQUAL_INT16(1,  got.unk_index);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_get_frame(anim2, 1, &got));
    TEST_ASSERT_EQUAL_INT16(30, got.key_frame);
    TEST_ASSERT_EQUAL_INT16(1,  got.unk_index);

    sf_free(NULL, bytes);
    sf_ani_destroy(b);
    sf_ani_destroy(a);
}

static void test_ani_round_trip_pos_rot_bytes_frames(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));

    sf_ani_node_t *n = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_node(a, &n));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_node_set_name(n, "byte_bone"));

    sf_ani_node_animation_t *anim = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_node_create_animation(n, &anim));
    sf_ani_animation_set_format(anim, SF_ANI_FRAME_FORMAT_POS_ROT_BYTES);

    sf_ani_frame_t f = {
        .key_frame                       = 12,
        .translation_index               = 5,
        .translation_in_tangent_index    = 6,
        .translation_out_tangent_index   = 7,
        .rotation_index                  = 8,
        .rotation_in_tangent_index       = 9,
        .rotation_out_tangent_index      = 10,
        .unk_index                       = 1,
    };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_add_frame(anim, f));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_write_to_memory(a, &bytes, &size, NULL));

    sf_ani_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_read_from_memory(&b, bytes, size, NULL));

    sf_ani_node_t *m = sf_ani_node_at(b, 0);
    sf_ani_node_animation_t *anim2 = sf_ani_node_animation(m);
    TEST_ASSERT_NOT_NULL(anim2);
    TEST_ASSERT_EQUAL_INT(SF_ANI_FRAME_FORMAT_POS_ROT_BYTES, sf_ani_animation_format(anim2));
    TEST_ASSERT_EQUAL_size_t(1, sf_ani_animation_frame_count(anim2));

    sf_ani_frame_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_get_frame(anim2, 0, &got));
    TEST_ASSERT_EQUAL_INT16(12, got.key_frame);
    TEST_ASSERT_EQUAL_INT16(5,  got.translation_index);
    TEST_ASSERT_EQUAL_INT16(6,  got.translation_in_tangent_index);
    TEST_ASSERT_EQUAL_INT16(7,  got.translation_out_tangent_index);
    TEST_ASSERT_EQUAL_INT16(8,  got.rotation_index);
    TEST_ASSERT_EQUAL_INT16(9,  got.rotation_in_tangent_index);
    TEST_ASSERT_EQUAL_INT16(10, got.rotation_out_tangent_index);
    TEST_ASSERT_EQUAL_INT16(1,  got.unk_index); /* upstream forces this to 1 for PosRotBytes */

    sf_free(NULL, bytes);
    sf_ani_destroy(b);
    sf_ani_destroy(a);
}

static void test_ani_round_trip_rot_shorts_frames(void) {
    sf_ani_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_create(&a, NULL));

    sf_ani_node_t *n = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_add_node(a, &n));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_node_set_name(n, "rot_only"));

    sf_ani_node_animation_t *anim = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_node_create_animation(n, &anim));
    sf_ani_animation_set_format(anim, SF_ANI_FRAME_FORMAT_ROT_SHORTS);

    sf_ani_frame_t f = {
        .key_frame                  = 42,
        .rotation_index             = 100,
        .rotation_in_tangent_index  = 101,
        .rotation_out_tangent_index = 102,
    };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_add_frame(anim, f));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_write_to_memory(a, &bytes, &size, NULL));

    sf_ani_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_read_from_memory(&b, bytes, size, NULL));

    sf_ani_node_animation_t *anim2 = sf_ani_node_animation(sf_ani_node_at(b, 0));
    TEST_ASSERT_NOT_NULL(anim2);
    TEST_ASSERT_EQUAL_INT(SF_ANI_FRAME_FORMAT_ROT_SHORTS, sf_ani_animation_format(anim2));

    sf_ani_frame_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ani_animation_get_frame(anim2, 0, &got));
    TEST_ASSERT_EQUAL_INT16(42,   got.key_frame);
    TEST_ASSERT_EQUAL_INT16(-1,   got.translation_index); /* defaulted by reader */
    TEST_ASSERT_EQUAL_INT16(100,  got.rotation_index);
    TEST_ASSERT_EQUAL_INT16(101,  got.rotation_in_tangent_index);
    TEST_ASSERT_EQUAL_INT16(102,  got.rotation_out_tangent_index);
    TEST_ASSERT_EQUAL_INT16(1,    got.unk_index);

    sf_free(NULL, bytes);
    sf_ani_destroy(b);
    sf_ani_destroy(a);
}

static void test_ani_read_bad_magic(void) {
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    /* Wrong magic. */
    buf[0] = 0xDE; buf[1] = 0xAD; buf[2] = 0xBE; buf[3] = 0xEF;

    sf_ani_t *a = NULL;
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_ani_read_from_memory(&a, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ani_create_destroy);
    RUN_TEST(test_ani_is_function);
    RUN_TEST(test_ani_node_defaults);
    RUN_TEST(test_ani_round_trip_minimal_no_nodes);
    RUN_TEST(test_ani_round_trip_single_node_no_animation);
    RUN_TEST(test_ani_round_trip_with_translation_rotation_buffers);
    RUN_TEST(test_ani_round_trip_pos_rot_shorts_frames);
    RUN_TEST(test_ani_round_trip_pos_rot_bytes_frames);
    RUN_TEST(test_ani_round_trip_rot_shorts_frames);
    RUN_TEST(test_ani_read_bad_magic);
    return UNITY_END();
}
