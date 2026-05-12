/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T24 — MATBIN synthetic round-trip: one parameter of each of the
 * 8 ParamType variants + 3 samplers. Byte layout mirrors
 * src/geom/matbin.c::matbin_write_to_writer().
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_matbin.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define MATBIN_KEY      0x12345678u
#define MATBIN_PARAMS   8u
#define MATBIN_SAMPLERS 3u

typedef struct param_spec {
    const char            *name;
    sf_matbin_param_type_t type;
    uint32_t               key;
    bool      b;
    int32_t   i;
    int32_t   i2[2];
    float     f;
    float     f2[2];
    float     f3[3];
    float     f4[4];
    float     f5[5];
} param_spec_t;

typedef struct sampler_spec {
    const char *type;
    const char *path;
    uint32_t    key;
    float       unk14_x;
    float       unk14_y;
} sampler_spec_t;

static const param_spec_t k_params[MATBIN_PARAMS] = {
    { "p_bool",   SF_MATBIN_PARAM_TYPE_BOOL,   0x00000001u, .b = true },
    { "p_int",    SF_MATBIN_PARAM_TYPE_INT,    0x00000002u, .i = -42 },
    { "p_int2",   SF_MATBIN_PARAM_TYPE_INT2,   0x00000003u, .i2 = { 7, -7 } },
    { "p_float",  SF_MATBIN_PARAM_TYPE_FLOAT,  0x00000004u, .f = 1.5f },
    { "p_float2", SF_MATBIN_PARAM_TYPE_FLOAT2, 0x00000005u, .f2 = { 0.25f, -0.5f } },
    { "p_float3", SF_MATBIN_PARAM_TYPE_FLOAT3, 0x00000006u, .f3 = { 0.1f, 0.2f, 0.3f } },
    { "p_float4", SF_MATBIN_PARAM_TYPE_FLOAT4, 0x00000007u,
                  .f4 = { 0.4f, 0.5f, 0.6f, 0.7f } },
    { "p_float5", SF_MATBIN_PARAM_TYPE_FLOAT5, 0x00000008u,
                  .f5 = { -1.f, -2.f, -3.f, -4.f, -5.f } },
};

static const sampler_spec_t k_samplers[MATBIN_SAMPLERS] = {
    { "g_DiffuseTexture",  "tex/diffuse.tga",  0xAAAA0001u, 0.0f,  0.0f },
    { "g_NormalTexture",   "tex/normal.tga",   0xAAAA0002u, 1.0f, -1.0f },
    { "g_SpecularTexture", "tex/specular.tga", 0xAAAA0003u, 0.5f,  0.5f },
};

static sf_result_t write_param_value(sf_binary_writer_t *bw,
                                     const param_spec_t *p) {
    switch (p->type) {
        case SF_MATBIN_PARAM_TYPE_BOOL:
            return sf_binary_writer_write_bool(bw, p->b);
        case SF_MATBIN_PARAM_TYPE_INT:
            return sf_binary_writer_write_i32(bw, p->i);
        case SF_MATBIN_PARAM_TYPE_INT2:
            return sf_binary_writer_write_i32s(bw, 2, p->i2);
        case SF_MATBIN_PARAM_TYPE_FLOAT:
            return sf_binary_writer_write_f32(bw, p->f);
        case SF_MATBIN_PARAM_TYPE_FLOAT2:
            return sf_binary_writer_write_f32s(bw, 2, p->f2);
        case SF_MATBIN_PARAM_TYPE_FLOAT3: {
            /* Float3 writes 5 floats on disk with trailing (1, 1) per upstream. */
            float five[5] = { p->f3[0], p->f3[1], p->f3[2], 1.0f, 1.0f };
            return sf_binary_writer_write_f32s(bw, 5, five);
        }
        case SF_MATBIN_PARAM_TYPE_FLOAT4:
            return sf_binary_writer_write_f32s(bw, 4, p->f4);
        case SF_MATBIN_PARAM_TYPE_FLOAT5:
            return sf_binary_writer_write_f32s(bw, 5, p->f5);
    }
    return SF_ERR_INVALID_ARG;
}

typedef struct slot_table {
    size_t shader_offset_pos;
    size_t source_offset_pos;
    size_t param_name_offset_pos[MATBIN_PARAMS];
    size_t param_value_offset_pos[MATBIN_PARAMS];
    size_t sampler_type_offset_pos[MATBIN_SAMPLERS];
    size_t sampler_path_offset_pos[MATBIN_SAMPLERS];
    size_t param_name_blob_pos[MATBIN_PARAMS];
    size_t param_value_blob_pos[MATBIN_PARAMS];
    size_t sampler_type_blob_pos[MATBIN_SAMPLERS];
    size_t sampler_path_blob_pos[MATBIN_SAMPLERS];
    size_t shader_blob_pos;
    size_t source_blob_pos;
} slot_table_t;

static sf_result_t build_matbin_pass1(uint8_t **out_bytes, size_t *out_size,
                                      slot_table_t *slots) {
    memset(slots, 0, sizeof(*slots));

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, NULL);
    if (r != SF_OK) return r;
    r = sf_binary_writer_create(&bw, os, false, NULL);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    static const uint8_t magic[4] = { 'M', 'A', 'B', 0 };
    if (r == SF_OK) r = sf_binary_writer_write_bytes(bw, magic, 4);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 2);

    slots->shader_offset_pos = (size_t)sf_binary_writer_position(bw);
    if (r == SF_OK) r = sf_binary_writer_write_i64(bw, 0);
    slots->source_offset_pos = (size_t)sf_binary_writer_position(bw);
    if (r == SF_OK) r = sf_binary_writer_write_i64(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_u32(bw, MATBIN_KEY);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)MATBIN_PARAMS);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)MATBIN_SAMPLERS);
    if (r == SF_OK) r = sf_binary_writer_write_pattern(bw, 0x14, 0x00);

    for (size_t i = 0; i < MATBIN_PARAMS && r == SF_OK; i++) {
        slots->param_name_offset_pos[i] = (size_t)sf_binary_writer_position(bw);
        r = sf_binary_writer_write_i64(bw, 0);
        if (r == SF_OK) slots->param_value_offset_pos[i] = (size_t)sf_binary_writer_position(bw);
        if (r == SF_OK) r = sf_binary_writer_write_i64(bw, 0);
        if (r == SF_OK) r = sf_binary_writer_write_u32(bw, k_params[i].key);
        if (r == SF_OK) r = sf_binary_writer_write_u32(bw, (uint32_t)k_params[i].type);
        if (r == SF_OK) r = sf_binary_writer_write_pattern(bw, 0x10, 0x00);
    }

    for (size_t i = 0; i < MATBIN_SAMPLERS && r == SF_OK; i++) {
        slots->sampler_type_offset_pos[i] = (size_t)sf_binary_writer_position(bw);
        r = sf_binary_writer_write_i64(bw, 0);
        if (r == SF_OK) slots->sampler_path_offset_pos[i] = (size_t)sf_binary_writer_position(bw);
        if (r == SF_OK) r = sf_binary_writer_write_i64(bw, 0);
        if (r == SF_OK) r = sf_binary_writer_write_u32(bw, k_samplers[i].key);
        sf_vec2_t unk14 = { k_samplers[i].unk14_x, k_samplers[i].unk14_y };
        if (r == SF_OK) r = sf_binary_writer_write_vec2(bw, unk14);
        if (r == SF_OK) r = sf_binary_writer_write_pattern(bw, 0x14, 0x00);
    }

    for (size_t i = 0; i < MATBIN_PARAMS && r == SF_OK; i++) {
        slots->param_name_blob_pos[i] = (size_t)sf_binary_writer_position(bw);
        if (r == SF_OK) r = sf_binary_writer_write_utf16(bw, k_params[i].name, true);
        if (r == SF_OK) slots->param_value_blob_pos[i] = (size_t)sf_binary_writer_position(bw);
        if (r == SF_OK) r = write_param_value(bw, &k_params[i]);
    }

    for (size_t i = 0; i < MATBIN_SAMPLERS && r == SF_OK; i++) {
        slots->sampler_type_blob_pos[i] = (size_t)sf_binary_writer_position(bw);
        if (r == SF_OK) r = sf_binary_writer_write_utf16(bw, k_samplers[i].type, true);
        if (r == SF_OK) slots->sampler_path_blob_pos[i] = (size_t)sf_binary_writer_position(bw);
        if (r == SF_OK) r = sf_binary_writer_write_utf16(bw, k_samplers[i].path, true);
    }

    slots->shader_blob_pos = (size_t)sf_binary_writer_position(bw);
    if (r == SF_OK) r = sf_binary_writer_write_utf16(bw, "shaders/test.spx", true);
    slots->source_blob_pos = (size_t)sf_binary_writer_position(bw);
    if (r == SF_OK) r = sf_binary_writer_write_utf16(bw, "src/test.matxml", true);

    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out_bytes, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

static void patch_i64_le(uint8_t *bytes, size_t pos, int64_t value) {
    uint64_t v = (uint64_t)value;
    for (size_t b = 0; b < 8; b++) {
        bytes[pos + b] = (uint8_t)((v >> (b * 8)) & 0xFFu);
    }
}

static sf_result_t build_canonical_matbin_bytes(uint8_t **out_bytes, size_t *out_size) {
    slot_table_t slots;
    sf_result_t r = build_matbin_pass1(out_bytes, out_size, &slots);
    if (r != SF_OK) return r;

    uint8_t *b = *out_bytes;
    patch_i64_le(b, slots.shader_offset_pos, (int64_t)slots.shader_blob_pos);
    patch_i64_le(b, slots.source_offset_pos, (int64_t)slots.source_blob_pos);
    for (size_t i = 0; i < MATBIN_PARAMS; i++) {
        patch_i64_le(b, slots.param_name_offset_pos[i],  (int64_t)slots.param_name_blob_pos[i]);
        patch_i64_le(b, slots.param_value_offset_pos[i], (int64_t)slots.param_value_blob_pos[i]);
    }
    for (size_t i = 0; i < MATBIN_SAMPLERS; i++) {
        patch_i64_le(b, slots.sampler_type_offset_pos[i],
                     (int64_t)slots.sampler_type_blob_pos[i]);
        patch_i64_le(b, slots.sampler_path_offset_pos[i],
                     (int64_t)slots.sampler_path_blob_pos[i]);
    }
    return SF_OK;
}

static void verify_matbin_fields(const sf_matbin_t *m) {
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_STRING("shaders/test.spx", sf_matbin_shader_path(m));
    TEST_ASSERT_EQUAL_STRING("src/test.matxml",  sf_matbin_source_path(m));
    TEST_ASSERT_EQUAL_HEX32(MATBIN_KEY, sf_matbin_key(m));

    TEST_ASSERT_EQUAL_size_t((size_t)MATBIN_PARAMS,  sf_matbin_param_count(m));
    TEST_ASSERT_EQUAL_size_t((size_t)MATBIN_SAMPLERS, sf_matbin_sampler_count(m));

    for (size_t i = 0; i < MATBIN_PARAMS; i++) {
        const sf_matbin_param_t *p = sf_matbin_param(m, i);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_STRING(k_params[i].name, sf_matbin_param_name(p));
        TEST_ASSERT_EQUAL_UINT32((uint32_t)k_params[i].type,
                                 (uint32_t)sf_matbin_param_type(p));
        TEST_ASSERT_EQUAL_HEX32(k_params[i].key, sf_matbin_param_key(p));

        switch (k_params[i].type) {
            case SF_MATBIN_PARAM_TYPE_BOOL: {
                bool v = false;
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_bool(p, &v));
                TEST_ASSERT_EQUAL(k_params[i].b, v);
                break;
            }
            case SF_MATBIN_PARAM_TYPE_INT: {
                int32_t v = 0;
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_int(p, &v));
                TEST_ASSERT_EQUAL_INT32(k_params[i].i, v);
                break;
            }
            case SF_MATBIN_PARAM_TYPE_INT2: {
                int32_t v[2] = { 0 };
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_int2(p, v));
                TEST_ASSERT_EQUAL_INT32_ARRAY(k_params[i].i2, v, 2);
                break;
            }
            case SF_MATBIN_PARAM_TYPE_FLOAT: {
                float v = 0.f;
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_float(p, &v));
                TEST_ASSERT_EQUAL_FLOAT(k_params[i].f, v);
                break;
            }
            case SF_MATBIN_PARAM_TYPE_FLOAT2: {
                float v[2] = { 0.f };
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_float2(p, v));
                TEST_ASSERT_EQUAL_FLOAT_ARRAY(k_params[i].f2, v, 2);
                break;
            }
            case SF_MATBIN_PARAM_TYPE_FLOAT3: {
                float v[3] = { 0.f };
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_float3(p, v));
                TEST_ASSERT_EQUAL_FLOAT_ARRAY(k_params[i].f3, v, 3);
                break;
            }
            case SF_MATBIN_PARAM_TYPE_FLOAT4: {
                float v[4] = { 0.f };
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_float4(p, v));
                TEST_ASSERT_EQUAL_FLOAT_ARRAY(k_params[i].f4, v, 4);
                break;
            }
            case SF_MATBIN_PARAM_TYPE_FLOAT5: {
                float v[5] = { 0.f };
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_param_value_float5(p, v));
                TEST_ASSERT_EQUAL_FLOAT_ARRAY(k_params[i].f5, v, 5);
                break;
            }
        }
    }

    for (size_t i = 0; i < MATBIN_SAMPLERS; i++) {
        const sf_matbin_sampler_t *s = sf_matbin_sampler(m, i);
        TEST_ASSERT_NOT_NULL(s);
        TEST_ASSERT_EQUAL_STRING(k_samplers[i].type, sf_matbin_sampler_type(s));
        TEST_ASSERT_EQUAL_STRING(k_samplers[i].path, sf_matbin_sampler_path(s));
        TEST_ASSERT_EQUAL_HEX32(k_samplers[i].key,  sf_matbin_sampler_key(s));
        sf_vec2_t unk14 = { 0.f, 0.f };
        TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_sampler_unk14(s, &unk14));
        TEST_ASSERT_EQUAL_FLOAT(k_samplers[i].unk14_x, unk14.x);
        TEST_ASSERT_EQUAL_FLOAT(k_samplers[i].unk14_y, unk14.y);
    }
}

static void test_matbin_all_param_types_round_trip(void) {
    uint8_t *in_bytes = NULL;
    size_t   in_size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, build_canonical_matbin_bytes(&in_bytes, &in_size));
    TEST_ASSERT_NOT_NULL(in_bytes);
    TEST_ASSERT_TRUE(in_size > 56u);
    TEST_ASSERT_EQUAL_MEMORY("MAB\0", in_bytes, 4);

    sf_matbin_t *m = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_read_from_memory(&m, in_bytes, in_size, NULL));
    verify_matbin_fields(m);

    void  *out_bytes = NULL;
    size_t out_size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_matbin_write_to_memory(m, &out_bytes, &out_size, NULL));
    TEST_ASSERT_NOT_NULL(out_bytes);
    TEST_ASSERT_EQUAL_size_t(in_size, out_size);
    TEST_ASSERT_EQUAL_MEMORY(in_bytes, out_bytes, in_size);

    sf_free(NULL, out_bytes);
    sf_matbin_destroy(m);
    sf_free(NULL, in_bytes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_matbin_all_param_types_round_trip);
    return UNITY_END();
}
