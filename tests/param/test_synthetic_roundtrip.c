/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 4 T4.2 — Integrated synthetic fixture round-trip tests for the four
 * text+param formats (PARAM, PARAMDEF, PARAMTDF, FMG).
 *
 * This is an *integration* test: it does not exercise new functionality;
 * it verifies that the read+write pipeline already covered by T2.x / T3.x
 * remains self-consistent end-to-end. The pattern matches the Phase 3
 * BND4 round-trip in tests/archive/test_bnd4_synthetic.c:
 *
 *   build fixture → read → write (canonical) → read → write
 *   then assert the two writes are byte-identical.
 *
 * Strict memcmp(input, output) byte-equality with the hand-built fixture
 * is not possible for the binary formats: the writer emits a canonical
 * layout that may legitimately differ from the human-friendly fixture
 * shape. Self-consistency of two consecutive canonical writes is the
 * strongest guarantee available and the one upstream tests in C# also
 * rely on (Wave 0 evidence, PARAM.Write/PARAMDEF.Write/FMG.Write).
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_fmg.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"
#include "souls_formats/sf_paramtdf.h"

#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * Little-endian fixture writer helpers
 *===========================================================================*/

typedef struct fixture {
    uint8_t data[1024];
    size_t size;
} fixture_t;

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
    p[off + 0] = (uint8_t)(v & 0xFFu);
    p[off + 1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, size_t off, uint32_t v) {
    p[off + 0] = (uint8_t)(v & 0xFFu);
    p[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
    p[off + 2] = (uint8_t)((v >> 16) & 0xFFu);
    p[off + 3] = (uint8_t)(v >> 24);
}

static void put_cstr(uint8_t *p, size_t off, const char *s) {
    memcpy(&p[off], s, strlen(s) + 1);
}

/*===========================================================================
 * Test 1: PARAM — 3 rows (IDs 100/200/300) × 5 fields
 *
 * Row data layout (27 bytes, padded to 32):
 *   offset  0  u8    field0
 *   offset  1  u16   field1
 *   offset  3  u32   field2
 *   offset  7  f32   field3
 *   offset 11  16-byte fixstr16 field4
 *   offset 27..31 zero padding
 *===========================================================================*/

static fixture_t make_param_fixture(void) {
    fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    const uint16_t row_count       = 3;
    const uint16_t rows_start      = 0x30;
    const uint16_t row_header_size = 12;
    const uint16_t row_data_size   = 32; /* u8+u16+u32+f32+fixstr16 = 27 → pad 32 */
    const uint16_t data_start      = (uint16_t)(rows_start + row_count * row_header_size);
    const uint16_t strings_offset  = (uint16_t)(data_start + row_count * row_data_size);

    /* Common 0x30-byte header */
    put_u32(fx.data, 0x00, strings_offset);
    put_u16(fx.data, 0x04, data_start);
    put_u16(fx.data, 0x06, 0);
    put_u16(fx.data, 0x08, 123);
    put_u16(fx.data, 0x0A, row_count);
    memcpy(&fx.data[0x0C], "SYN_PARAM", 9);
    fx.data[0x2C] = 0;
    fx.data[0x2D] = SF_PARAM_FORMAT_FLAGS1_NONE;
    fx.data[0x2E] = SF_PARAM_FORMAT_FLAGS2_NONE;
    fx.data[0x2F] = 0x6A;

    /* String table: leading 2-byte NUL pad, then row names. */
    size_t name_off = strings_offset;
    put_u16(fx.data, name_off, 0);
    name_off += 2;

    static const int32_t row_ids[3] = { 100, 200, 300 };
    for (uint16_t i = 0; i < row_count; i++) {
        size_t row_off  = rows_start + (size_t)i * row_header_size;
        size_t data_off = data_start + (size_t)i * row_data_size;

        put_u32(fx.data, row_off + 0, (uint32_t)row_ids[i]);
        put_u16(fx.data, row_off + 4, (uint16_t)data_off);
        put_u16(fx.data, row_off + 6, 0);
        put_u32(fx.data, row_off + 8, (uint32_t)name_off);

        /* Synthetic 5-field row data. Cells stay raw bytes (no ApplyParamdef). */
        fx.data[data_off + 0] = (uint8_t)(0xA0 + i);
        put_u16(fx.data, data_off + 1, (uint16_t)(0xB000u + i));
        put_u32(fx.data, data_off + 3, (uint32_t)(0xC0000000u + i));
        union { float f; uint32_t u; } cv;
        cv.f = 1.5f * (float)(i + 1);
        put_u32(fx.data, data_off + 7, cv.u);
        char fixstr16[16];
        snprintf(fixstr16, sizeof(fixstr16), "field4_%u", (unsigned)i);
        memcpy(&fx.data[data_off + 11], fixstr16, 16);

        char name[16];
        snprintf(name, sizeof(name), "row_%u", (unsigned)i);
        put_cstr(fx.data, name_off, name);
        name_off += strlen(name) + 1;
    }
    /* Trailing 2-byte NUL pad to anchor the writer's string-table footer. */
    put_u16(fx.data, name_off, 0);
    fx.size = name_off + 2;
    return fx;
}

static void test_param_synthetic_roundtrip(void) {
    fixture_t fx = make_param_fixture();

    sf_param_t *param = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_param_read_from_memory(&param, fx.data, fx.size, NULL));
    TEST_ASSERT_NOT_NULL(param);

    uint8_t *write1 = NULL;
    size_t size1 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(param, &write1, &size1, NULL));
    TEST_ASSERT_NOT_NULL(write1);
    TEST_ASSERT_GREATER_THAN_size_t(0, size1);

    sf_param_t *rebound = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_param_read_from_memory(&rebound, write1, size1, NULL));
    TEST_ASSERT_NOT_NULL(rebound);
    TEST_ASSERT_EQUAL_size_t(3, sf_param_get_row_count(rebound));
    TEST_ASSERT_EQUAL_STRING("SYN_PARAM", sf_param_get_param_type(rebound));

    static const int32_t expected_ids[3] = { 100, 200, 300 };
    for (size_t i = 0; i < 3; i++) {
        const sf_param_row_t *row = sf_param_get_row(rebound, i);
        TEST_ASSERT_NOT_NULL(row);
        TEST_ASSERT_EQUAL_INT32(expected_ids[i], sf_param_row_get_id(row));
    }

    uint8_t *write2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(rebound, &write2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size1, size2);
    TEST_ASSERT_EQUAL_MEMORY(write1, write2, size1);

    sf_free(NULL, write2);
    sf_param_destroy(rebound);
    sf_free(NULL, write1);
    sf_param_destroy(param);
}

/*===========================================================================
 * Test 2: PARAMDEF v104 — 3 fields (s32 / f32 / u8)
 *
 * v104 uses 0xB0-byte fields with fixstr names. We build the header by
 * hand and append three 0xB0-byte field records back-to-back.
 *===========================================================================*/

static void put_i16_at(uint8_t *p, size_t off, int16_t v) {
    p[off + 0] = (uint8_t)((uint16_t)v & 0xFFu);
    p[off + 1] = (uint8_t)(((uint16_t)v >> 8) & 0xFFu);
}

static void put_fixstr(uint8_t *p, size_t off, const char *s, size_t width) {
    size_t n = strlen(s);
    for (size_t i = 0; i < width; i++) {
        p[off + i] = i < n ? (uint8_t)s[i] : 0;
    }
}

/*  Writes a single v104 paramdef field at *cursor and advances. Layout:
 *    +0x00  display_name fixstr  0x40
 *    +0x40  display_type fixstr  0x08  ("s32"/"f32"/"u8")
 *    +0x48  display_format fixstr 0x08 ("%d"/"%f")
 *    +0x50  default/min/max/incr 4 × f32 (zeroed)
 *    +0x60  edit_flags i32       (0)
 *    +0x64  byte_count i32       (matches type)
 *    +0x68  description_offset i32 (0 = no description)
 *    +0x6C  internal_type fixstr 0x20
 *    +0x8C  internal_name fixstr 0x20
 *    +0xAC  sort_id i32          (123)
 *    Total: 0xB0 bytes.
 */
static void put_v104_field(uint8_t *p, size_t *cursor, const char *display_name,
                           const char *type, const char *fmt, int32_t byte_count,
                           const char *internal_name) {
    size_t base = *cursor;
    memset(&p[base], 0, 0xB0);
    put_fixstr(p, base + 0x00, display_name, 0x40);
    put_fixstr(p, base + 0x40, type, 0x08);
    put_fixstr(p, base + 0x48, fmt, 0x08);
    /* 0x50..0x5F: default/min/max/incr f32 — already zero. */
    put_u32(p, base + 0x60, 0);
    put_u32(p, base + 0x64, (uint32_t)byte_count);
    put_u32(p, base + 0x68, 0);
    put_fixstr(p, base + 0x6C, type, 0x20);
    put_fixstr(p, base + 0x8C, internal_name, 0x20);
    put_u32(p, base + 0xAC, 123);
    *cursor = base + 0xB0;
}

static fixture_t make_paramdef_fixture(void) {
    fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    /* v104 header layout (matches src/param/paramdef.c reader): */
    size_t cursor = 0;
    put_u32(fx.data, cursor, 0);  cursor += 4;  /* file_size — patched later */
    put_i16_at(fx.data, cursor, (int16_t)0x30);  cursor += 2;  /* header_size */
    put_i16_at(fx.data, cursor, 7);              cursor += 2;  /* data_version */
    put_i16_at(fx.data, cursor, 3);              cursor += 2;  /* field_count */
    put_i16_at(fx.data, cursor, 0xB0);           cursor += 2;  /* field_size */
    put_fixstr(fx.data, cursor, "SYN_PARAMDEF", 0x20); cursor += 0x20;
    fx.data[cursor++] = 0;                       /* big_endian = false */
    fx.data[cursor++] = 0;                       /* unicode    = false */
    put_i16_at(fx.data, cursor, 104);            cursor += 2;  /* format_version */
    /* Header end at 0x30. */

    /* 3 fields: s32 / f32 / u8 */
    put_v104_field(fx.data, &cursor, "DisplayA", "s32", "%d", 4,  "field_s32");
    put_v104_field(fx.data, &cursor, "DisplayB", "f32", "%f", 4,  "field_f32");
    put_v104_field(fx.data, &cursor, "DisplayC", "u8",  "%d", 1,  "field_u8");

    put_u32(fx.data, 0, (uint32_t)cursor);  /* patch file_size */
    fx.size = cursor;
    return fx;
}

static void test_paramdef_synthetic_roundtrip(void) {
    fixture_t fx = make_paramdef_fixture();

    sf_paramdef_t *def = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_paramdef_read_from_memory(&def, fx.data, fx.size, NULL));
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT_EQUAL_INT16(104, sf_paramdef_get_format_version(def));
    TEST_ASSERT_EQUAL_size_t(3, sf_paramdef_get_field_count(def));
    TEST_ASSERT_EQUAL_STRING("SYN_PARAMDEF", sf_paramdef_get_param_type(def));

    uint8_t *write1 = NULL;
    size_t size1 = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_paramdef_write_to_memory(def, &write1, &size1, NULL));
    TEST_ASSERT_NOT_NULL(write1);
    TEST_ASSERT_GREATER_THAN_size_t(0, size1);

    sf_paramdef_t *rebound = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_paramdef_read_from_memory(&rebound, write1, size1, NULL));
    TEST_ASSERT_NOT_NULL(rebound);
    TEST_ASSERT_EQUAL_INT16(104, sf_paramdef_get_format_version(rebound));
    TEST_ASSERT_EQUAL_size_t(3, sf_paramdef_get_field_count(rebound));

    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_S32,
        sf_paramdef_field_get_display_type(sf_paramdef_get_field(rebound, 0)));
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_F32,
        sf_paramdef_field_get_display_type(sf_paramdef_get_field(rebound, 1)));
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_U8,
        sf_paramdef_field_get_display_type(sf_paramdef_get_field(rebound, 2)));

    uint8_t *write2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_paramdef_write_to_memory(rebound, &write2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size1, size2);
    TEST_ASSERT_EQUAL_MEMORY(write1, write2, size1);

    sf_free(NULL, write2);
    sf_paramdef_destroy(rebound);
    sf_free(NULL, write1);
    sf_paramdef_destroy(def);
}

/*===========================================================================
 * Test 3: PARAMTDF — 3-entry u32 round-trip via text
 *
 * Text format may legitimately differ between input (no trailing newline)
 * and canonical output (trailing CRLF). We therefore compare two
 * consecutive canonical writes (both produced by the writer) with strcmp.
 *===========================================================================*/

static void test_paramtdf_synthetic_roundtrip(void) {
    static const char input[] =
        "\"SynTdf\"\r\n"
        "\"u32\"\r\n"
        "\"None\",\"0\"\r\n"
        "\"On\",\"1\"\r\n"
        "\"Off\",\"2\"";

    sf_paramtdf_t *tdf = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_paramtdf_read_from_text(input, strlen(input), &tdf, NULL));
    TEST_ASSERT_NOT_NULL(tdf);
    TEST_ASSERT_EQUAL_STRING("SynTdf", sf_paramtdf_get_name(tdf));
    TEST_ASSERT_EQUAL_INT(SF_PARAMTDF_TYPE_U32, sf_paramtdf_get_type(tdf));
    TEST_ASSERT_EQUAL_size_t(3, sf_paramtdf_get_entry_count(tdf));

    char *write1 = NULL;
    size_t size1 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_paramtdf_write_to_text(tdf, &write1, &size1, NULL));
    TEST_ASSERT_NOT_NULL(write1);
    TEST_ASSERT_GREATER_THAN_size_t(0, size1);

    sf_paramtdf_t *rebound = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_paramtdf_read_from_text(write1, size1, &rebound, NULL));
    TEST_ASSERT_NOT_NULL(rebound);
    TEST_ASSERT_EQUAL_size_t(3, sf_paramtdf_get_entry_count(rebound));

    static const char *const expected_names[3]  = { "None", "On", "Off" };
    static const int64_t expected_values[3]     = { 0, 1, 2 };
    for (size_t i = 0; i < 3; i++) {
        const sf_paramtdf_entry_t *e = sf_paramtdf_get_entry(rebound, i);
        TEST_ASSERT_NOT_NULL(e);
        TEST_ASSERT_EQUAL_STRING(expected_names[i], sf_paramtdf_entry_get_name(e));
        TEST_ASSERT_EQUAL_INT64(expected_values[i], sf_paramtdf_entry_get_value(e));
    }

    char *write2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_paramtdf_write_to_text(rebound, &write2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size1, size2);
    TEST_ASSERT_EQUAL_STRING(write1, write2);

    sf_free(NULL, write2);
    sf_paramtdf_destroy(rebound, NULL);
    sf_free(NULL, write1);
    sf_paramtdf_destroy(tdf, NULL);
}

/*===========================================================================
 * Test 4: FMG — v1 (DarkSouls1), 5 entries including 1 deleted (NULL text)
 *===========================================================================*/

static void test_fmg_synthetic_roundtrip(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &fmg));
    TEST_ASSERT_NOT_NULL(fmg);

    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 100, "alpha", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 101, "beta",  NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 102, NULL,    NULL)); /* deleted */
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 103, "gamma", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 104, "delta", NULL));

    uint8_t *write1 = NULL;
    size_t size1 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &write1, &size1, NULL));
    TEST_ASSERT_NOT_NULL(write1);
    TEST_ASSERT_GREATER_THAN_size_t(0, size1);

    sf_fmg_t *rebound = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&rebound, write1, size1, NULL));
    TEST_ASSERT_NOT_NULL(rebound);
    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DARK_SOULS_1, sf_fmg_get_version(rebound));
    TEST_ASSERT_EQUAL_size_t(5, sf_fmg_get_entry_count(rebound));

    static const struct {
        int32_t id;
        const char *text;
    } expected[5] = {
        { 100, "alpha" },
        { 101, "beta"  },
        { 102, NULL    },
        { 103, "gamma" },
        { 104, "delta" },
    };
    for (size_t i = 0; i < 5; i++) {
        const sf_fmg_entry_t *e = sf_fmg_get_entry(rebound, i);
        TEST_ASSERT_NOT_NULL(e);
        TEST_ASSERT_EQUAL_INT32(expected[i].id, sf_fmg_entry_get_id(e));
        if (expected[i].text == NULL) {
            TEST_ASSERT_NULL(sf_fmg_entry_get_text(e));
        } else {
            TEST_ASSERT_NOT_NULL(sf_fmg_entry_get_text(e));
            TEST_ASSERT_EQUAL_STRING(expected[i].text, sf_fmg_entry_get_text(e));
        }
    }

    uint8_t *write2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(rebound, &write2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size1, size2);
    TEST_ASSERT_EQUAL_MEMORY(write1, write2, size1);

    sf_free(NULL, write2);
    sf_fmg_destroy(rebound);
    sf_free(NULL, write1);
    sf_fmg_destroy(fmg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_param_synthetic_roundtrip);
    RUN_TEST(test_paramdef_synthetic_roundtrip);
    RUN_TEST(test_paramtdf_synthetic_roundtrip);
    RUN_TEST(test_fmg_synthetic_roundtrip);
    return UNITY_END();
}
