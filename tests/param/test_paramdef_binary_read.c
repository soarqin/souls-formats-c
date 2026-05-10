/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_paramdef.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct fixture_buf {
    uint8_t data[1024];
    size_t size;
} fixture_buf_t;

static void put_u8(fixture_buf_t *b, uint8_t v) {
    TEST_ASSERT_LESS_THAN_size_t(sizeof(b->data), b->size);
    b->data[b->size++] = v;
}

static void put_i16(fixture_buf_t *b, int16_t v) {
    put_u8(b, (uint8_t)((uint16_t)v & 0xFFu));
    put_u8(b, (uint8_t)(((uint16_t)v >> 8) & 0xFFu));
}

static void put_i32(fixture_buf_t *b, int32_t v) {
    uint32_t u = (uint32_t)v;
    put_u8(b, (uint8_t)(u & 0xFFu));
    put_u8(b, (uint8_t)((u >> 8) & 0xFFu));
    put_u8(b, (uint8_t)((u >> 16) & 0xFFu));
    put_u8(b, (uint8_t)((u >> 24) & 0xFFu));
}

static void put_i64(fixture_buf_t *b, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; i++) put_u8(b, (uint8_t)((u >> (i * 8)) & 0xFFu));
}

static void patch_i32(fixture_buf_t *b, size_t off, int32_t v) {
    TEST_ASSERT_LESS_OR_EQUAL_size_t(b->size, off + 4);
    uint32_t u = (uint32_t)v;
    b->data[off + 0] = (uint8_t)(u & 0xFFu);
    b->data[off + 1] = (uint8_t)((u >> 8) & 0xFFu);
    b->data[off + 2] = (uint8_t)((u >> 16) & 0xFFu);
    b->data[off + 3] = (uint8_t)((u >> 24) & 0xFFu);
}

static void patch_i64(fixture_buf_t *b, size_t off, int64_t v) {
    TEST_ASSERT_LESS_OR_EQUAL_size_t(b->size, off + 8);
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; i++) b->data[off + (size_t)i] = (uint8_t)((u >> (i * 8)) & 0xFFu);
}

static void patch_varint(fixture_buf_t *b, size_t off, int16_t version, size_t target) {
    if (version >= 200) patch_i64(b, off, (int64_t)target);
    else patch_i32(b, off, (int32_t)target);
}

static void put_varint_zero(fixture_buf_t *b, int16_t version) {
    if (version >= 200) put_i64(b, 0);
    else put_i32(b, 0);
}

static void put_zeroes(fixture_buf_t *b, size_t count) {
    for (size_t i = 0; i < count; i++) put_u8(b, 0);
}

static void put_fixstr(fixture_buf_t *b, const char *s, size_t width) {
    size_t n = strlen(s);
    TEST_ASSERT_LESS_OR_EQUAL_size_t(width, n);
    for (size_t i = 0; i < width; i++) put_u8(b, i < n ? (uint8_t)s[i] : 0);
}

static void put_cstr(fixture_buf_t *b, const char *s) {
    while (*s) put_u8(b, (uint8_t)*s++);
    put_u8(b, 0);
}

static void put_utf16le_cstr(fixture_buf_t *b, const char *ascii) {
    while (*ascii) {
        put_u8(b, (uint8_t)*ascii++);
        put_u8(b, 0);
    }
    put_u8(b, 0);
    put_u8(b, 0);
}

static void put_f32_zero(fixture_buf_t *b) {
    put_i32(b, 0);
}

static int16_t field_size_for_version(int16_t version) {
    switch (version) {
    case 0: return 0x68;
    case 101: return 0x8C;
    case 102: return 0xAC;
    case 103: return 0x6C;
    case 104: return 0xB0;
    case 106: return 0x48;
    case 201: return 0xD0;
    case 202: return 0x68;
    case 203: return 0x88;
    default: return 0;
    }
}

static fixture_buf_t make_paramdef_fixture(int16_t version) {
    fixture_buf_t b;
    memset(&b, 0, sizeof b);

    size_t file_size_pos = b.size;
    put_i32(&b, 0);
    put_i16(&b, (int16_t)(version >= 200 ? 0xFF : 0x30));
    put_i16(&b, 7);
    put_i16(&b, 1);
    put_i16(&b, field_size_for_version(version));

    size_t param_type_off_pos = 0;
    if (version >= 202) {
        put_i32(&b, 0);
        param_type_off_pos = b.size;
        put_i64(&b, 0);
        put_i64(&b, 0);
        put_i64(&b, 0);
        put_i32(&b, 0);
    } else if (version >= 106 && version < 200) {
        param_type_off_pos = b.size;
        put_i32(&b, 0);
        put_i64(&b, 0);
        put_i64(&b, 0);
        put_i64(&b, 0);
        put_i32(&b, 0);
    } else {
        put_fixstr(&b, "TEST_PARAM", 0x20);
    }

    put_u8(&b, 0); /* little-endian */
    put_u8(&b, 0); /* Shift-JIS field strings */
    put_i16(&b, version);
    if (version >= 200) put_i64(&b, 0x38);

    size_t display_name_off_pos = 0;
    size_t internal_type_off_pos = 0;
    size_t internal_name_off_pos = 0;

    if (version >= 202 || (version >= 106 && version < 200)) {
        display_name_off_pos = b.size;
        put_varint_zero(&b, version);
    } else {
        put_fixstr(&b, "Display", 0x40);
    }

    put_fixstr(&b, "s32", 8);
    put_fixstr(&b, "%d", 8);

    if (version >= 203) {
        put_zeroes(&b, 0x10);
    } else {
        put_f32_zero(&b);
        put_f32_zero(&b);
        put_f32_zero(&b);
        put_f32_zero(&b);
    }

    put_i32(&b, 0);
    put_i32(&b, 4);

    if (version == 0) {
        /* BasicFields ends here. */
    } else {
        put_varint_zero(&b, version); /* description offset */

        if (version >= 202 || (version >= 106 && version < 200)) {
            internal_type_off_pos = b.size;
            put_varint_zero(&b, version);
        } else {
            put_fixstr(&b, "s32", 0x20);
        }

        if (version >= 102) {
            if (version >= 202 || (version >= 106 && version < 200)) {
                internal_name_off_pos = b.size;
                put_varint_zero(&b, version);
            } else {
                put_fixstr(&b, "field:5", 0x20);
            }
        }

        if (version >= 104) put_i32(&b, 123);

        if (version >= 200) {
            put_i32(&b, 0);
            put_i64(&b, 0);
            put_i64(&b, 0);
            put_i64(&b, 0);
        } else if (version >= 106) {
            put_i32(&b, 0);
            put_i32(&b, 0);
            put_i32(&b, 0);
        }
    }

    if (version >= 203) {
        for (int i = 0; i < 4; i++) {
            put_i32(&b, i + 1);
            put_i32(&b, 0);
        }
    }

    if (version >= 202 || (version >= 106 && version < 200)) {
        patch_varint(&b, param_type_off_pos, version, b.size);
        put_cstr(&b, "TEST_PARAM");

        patch_varint(&b, display_name_off_pos, version, b.size);
        put_utf16le_cstr(&b, "Display");

        patch_varint(&b, internal_type_off_pos, version, b.size);
        put_cstr(&b, "s32");

        patch_varint(&b, internal_name_off_pos, version, b.size);
        put_cstr(&b, "field:5");
    }

    patch_i32(&b, file_size_pos, (int32_t)b.size);
    return b;
}

static void assert_fixture_reads(int16_t version, const char *expected_internal_name,
                                 int32_t expected_bit_size) {
    fixture_buf_t fixture = make_paramdef_fixture(version);
    sf_paramdef_t *paramdef = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_paramdef_read_from_memory(&paramdef, fixture.data, fixture.size, NULL));
    TEST_ASSERT_NOT_NULL(paramdef);
    TEST_ASSERT_EQUAL_INT16(version, sf_paramdef_get_format_version(paramdef));
    TEST_ASSERT_EQUAL_size_t(1, sf_paramdef_get_field_count(paramdef));
    TEST_ASSERT_EQUAL_STRING("TEST_PARAM", sf_paramdef_get_param_type(paramdef));

    const sf_paramdef_field_t *field = sf_paramdef_get_field(paramdef, 0);
    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_S32, sf_paramdef_field_get_display_type(field));
    TEST_ASSERT_EQUAL_STRING(expected_internal_name, sf_paramdef_field_get_internal_name(field));
    TEST_ASSERT_EQUAL_INT32(expected_bit_size, sf_paramdef_field_get_bit_size(field));
    TEST_ASSERT_EQUAL_INT32(1, sf_paramdef_field_get_array_length(field));

    sf_paramdef_destroy(paramdef);
}

static void test_paramdef_reads_version_0(void) { assert_fixture_reads(0, "", -1); }
static void test_paramdef_reads_version_101(void) { assert_fixture_reads(101, "", -1); }
static void test_paramdef_reads_version_102(void) { assert_fixture_reads(102, "field", 5); }
static void test_paramdef_reads_version_103(void) { assert_fixture_reads(103, "field", 5); }
static void test_paramdef_reads_version_104(void) { assert_fixture_reads(104, "field", 5); }
static void test_paramdef_reads_version_106(void) { assert_fixture_reads(106, "field", 5); }
static void test_paramdef_reads_version_201(void) { assert_fixture_reads(201, "field", 5); }
static void test_paramdef_reads_version_202(void) { assert_fixture_reads(202, "field", 5); }
static void test_paramdef_reads_version_203(void) { assert_fixture_reads(203, "field", 5); }

static void test_paramdef_rejects_unknown_format_version(void) {
    fixture_buf_t fixture = make_paramdef_fixture(104);
    fixture.data[0x2E] = 0xE7;
    fixture.data[0x2F] = 0x03;
    sf_paramdef_t *paramdef = NULL;
    TEST_ASSERT_EQUAL(SF_ERR_UNSUPPORTED_VERSION,
                      sf_paramdef_read_from_memory(&paramdef, fixture.data, fixture.size, NULL));
    TEST_ASSERT_NULL(paramdef);
}

static void test_paramdef_rejects_truncated_input(void) {
    uint8_t tiny[8] = { 0 };
    sf_paramdef_t *paramdef = NULL;
    sf_result_t r = sf_paramdef_read_from_memory(&paramdef, tiny, sizeof tiny, NULL);
    TEST_ASSERT_TRUE(r == SF_ERR_TRUNCATED || r == SF_ERR_OUT_OF_RANGE);
    TEST_ASSERT_NULL(paramdef);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_paramdef_reads_version_0);
    RUN_TEST(test_paramdef_reads_version_101);
    RUN_TEST(test_paramdef_reads_version_102);
    RUN_TEST(test_paramdef_reads_version_103);
    RUN_TEST(test_paramdef_reads_version_104);
    RUN_TEST(test_paramdef_reads_version_106);
    RUN_TEST(test_paramdef_reads_version_201);
    RUN_TEST(test_paramdef_reads_version_202);
    RUN_TEST(test_paramdef_reads_version_203);
    RUN_TEST(test_paramdef_rejects_unknown_format_version);
    RUN_TEST(test_paramdef_rejects_truncated_input);
    return UNITY_END();
}
