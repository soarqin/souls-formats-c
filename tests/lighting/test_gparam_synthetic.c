/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Synthetic round-trip tests for GPARAM V5 (Sekiro/ER/NR) and V6 (AC6).
 *
 * Strategy: build the binary wire format manually in a heap buffer, then:
 *   1. sf_gparam_read_from_memory  → parse
 *   2. Verify field values via accessors
 *   3. sf_gparam_write_to_buffer   → serialize (buf1)
 *   4. sf_gparam_read_from_memory  → parse again
 *   5. sf_gparam_write_to_buffer   → serialize (buf2)
 *   6. TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, sz)
 *
 * All 16 FieldType variants (SBYTE..STRING) are exercised across the two
 * fixtures so the central switch in gparam.c is fully covered.
 */

#include "souls_formats/sf_gparam.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * Minimal growable byte buffer — used to build wire blobs without knowing
 * sizes up front.
 * ========================================================================= */

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} buf_t;

static void buf_init(buf_t *b) { b->data = NULL; b->len = 0; b->cap = 0; }

static void buf_free(buf_t *b) { free(b->data); buf_init(b); }

static void buf_grow(buf_t *b, size_t need) {
    if (b->len + need <= b->cap) return;
    size_t nc = b->cap ? b->cap * 2u : 256u;
    while (nc < b->len + need) nc *= 2u;
    b->data = (uint8_t *)realloc(b->data, nc);
    TEST_ASSERT_NOT_NULL(b->data);
    b->cap = nc;
}

static void buf_u8(buf_t *b, uint8_t v) {
    buf_grow(b, 1);
    b->data[b->len++] = v;
}

static void buf_i8(buf_t *b, int8_t v) { buf_u8(b, (uint8_t)v); }

static void buf_u16le(buf_t *b, uint16_t v) {
    buf_u8(b, (uint8_t)(v));
    buf_u8(b, (uint8_t)(v >> 8));
}

static void buf_i16le(buf_t *b, int16_t v) { buf_u16le(b, (uint16_t)v); }

static void buf_u32le(buf_t *b, uint32_t v) {
    buf_u8(b, (uint8_t)(v));
    buf_u8(b, (uint8_t)(v >> 8));
    buf_u8(b, (uint8_t)(v >> 16));
    buf_u8(b, (uint8_t)(v >> 24));
}

static void buf_i32le(buf_t *b, int32_t v) { buf_u32le(b, (uint32_t)v); }

static void buf_u64le(buf_t *b, uint64_t v) {
    buf_u32le(b, (uint32_t)(v));
    buf_u32le(b, (uint32_t)(v >> 32));
}

static void buf_i64le(buf_t *b, int64_t v) { buf_u64le(b, (uint64_t)v); }

static void buf_f32le(buf_t *b, float v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    buf_u32le(b, u);
}

static void buf_f64le(buf_t *b, double v) {
    uint64_t u;
    memcpy(&u, &v, 8);
    buf_u64le(b, u);
}

/* Write a NUL-terminated UTF-16LE string (including the NUL codepoint). */
static void buf_utf16le(buf_t *b, const char *s) {
    for (; *s; s++) {
        buf_u8(b, (uint8_t)*s);
        buf_u8(b, 0);
    }
    buf_u8(b, 0); buf_u8(b, 0); /* NUL terminator */
}

/* Pad to alignment (relative to buffer start). */
static void buf_pad(buf_t *b, size_t align) {
    while (b->len % align) buf_u8(b, 0);
}

/* Patch a little-endian i32 at a previously-written position. */
static void buf_patch_i32le(buf_t *b, size_t pos, int32_t v) {
    uint32_t u = (uint32_t)v;
    b->data[pos + 0] = (uint8_t)(u);
    b->data[pos + 1] = (uint8_t)(u >> 8);
    b->data[pos + 2] = (uint8_t)(u >> 16);
    b->data[pos + 3] = (uint8_t)(u >> 24);
}

/* =========================================================================
 * GPARAM wire-format builder
 *
 * Layout (matches upstream GPARAM.cs:Write() order):
 *   Header (magic + version + header fields + base-offset slots)
 *   ParamOffsets section
 *   Params section
 *   FieldOffsets section
 *   Fields section
 *   Values section
 *   ValueIds section
 *   Data30 (empty) + pad(4)
 *   ParamExtras section
 *   ParamExtraIds section
 *   ParamCommentsOffsets section
 *   CommentOffsets section
 *   Comments section
 * ========================================================================= */

/*
 * Fixture description:
 *
 * V5 fixture — 3 params, 16 fields total (all FieldType variants), 2 values
 * each, 1 comment on param 0, 1 UnkParamExtra with 2 ids.
 *
 * Param 0 "DofParam" (key "Dof"):
 *   Field 0: SBYTE  key="sbyte_key"  name="SbyteField"   values: [-1, 2]
 *   Field 1: SHORT  key="short_key"  name="ShortField"   values: [-100, 200]
 *   Field 2: INT    key="int_key"    name="IntField"      values: [-1000, 2000]
 *   Field 3: LONG   key="long_key"  name="LongField"     values: [-1000000, 2000000]
 *   Field 4: BYTE   key="byte_key"  name="ByteField"     values: [255, 0]
 *   Field 5: USHORT key="ushort_key" name="UshortField"  values: [65535, 1]
 *   Comment: "DofComment"
 *
 * Param 1 "FogParam" (key "Fog"):
 *   Field 0: UINT   key="uint_key"  name="UintField"     values: [4294967295, 1]
 *   Field 1: ULONG  key="ulong_key" name="UlongField"    values: [18446744073709551615, 1]
 *   Field 2: FLOAT  key="float_key" name="FloatField"    values: [1.5, -2.5]
 *   Field 3: DOUBLE key="double_key" name="DoubleField"  values: [3.14, -2.71]
 *   Field 4: BOOL   key="bool_key"  name="BoolField"     values: [true, false]
 *   Field 5: VEC2   key="vec2_key"  name="Vec2Field"     values: [(1,2), (3,4)]
 *   No comments
 *
 * Param 2 "SkyParam" (key "Sky"):
 *   Field 0: VEC3   key="vec3_key"  name="Vec3Field"     values: [(1,2,3), (4,5,6)]
 *   Field 1: VEC4   key="vec4_key"  name="Vec4Field"     values: [(1,2,3,4), (5,6,7,8)]
 *   Field 2: COLOR  key="color_key" name="ColorField"    values: [(255,0,128,255), (0,255,0,128)]
 *   Field 3: STRING key="str_key"   name="StringField"   values: ["hello", "world"]
 *   No comments
 *
 * UnkParamExtra[0]: unk00=42, ids=[10, 20], unk0c=99
 */

/* Write a single value payload for the Values section. */
static void buf_write_value(buf_t *b, int type, int val_idx) {
    /* Values are chosen to be distinct and verifiable. */
    switch (type) {
        case 1: /* SBYTE */
            buf_i8(b, val_idx == 0 ? -1 : 2);
            break;
        case 2: /* SHORT */
            buf_i16le(b, val_idx == 0 ? -100 : 200);
            break;
        case 3: /* INT */
            buf_i32le(b, val_idx == 0 ? -1000 : 2000);
            break;
        case 4: /* LONG */
            buf_i64le(b, val_idx == 0 ? -1000000LL : 2000000LL);
            break;
        case 5: /* BYTE */
            buf_u8(b, val_idx == 0 ? 255 : 0);
            break;
        case 6: /* USHORT */
            buf_u16le(b, val_idx == 0 ? 65535u : 1u);
            break;
        case 7: /* UINT */
            buf_u32le(b, val_idx == 0 ? 0xFFFFFFFFu : 1u);
            break;
        case 8: /* ULONG */
            buf_u64le(b, val_idx == 0 ? 0xFFFFFFFFFFFFFFFFull : 1ull);
            break;
        case 9: /* FLOAT */
            buf_f32le(b, val_idx == 0 ? 1.5f : -2.5f);
            break;
        case 10: /* DOUBLE */
            buf_f64le(b, val_idx == 0 ? 3.14 : -2.71);
            break;
        case 11: /* BOOL */
            buf_u8(b, val_idx == 0 ? 1 : 0);
            break;
        case 12: /* VEC2 (8 bytes + 8 pad) */
            buf_f32le(b, val_idx == 0 ? 1.0f : 3.0f);
            buf_f32le(b, val_idx == 0 ? 2.0f : 4.0f);
            buf_u64le(b, 0); /* 8-byte padding */
            break;
        case 13: /* VEC3 (12 bytes + 4 pad) */
            buf_f32le(b, val_idx == 0 ? 1.0f : 4.0f);
            buf_f32le(b, val_idx == 0 ? 2.0f : 5.0f);
            buf_f32le(b, val_idx == 0 ? 3.0f : 6.0f);
            buf_i32le(b, 0); /* 4-byte padding */
            break;
        case 14: /* VEC4 */
            buf_f32le(b, val_idx == 0 ? 1.0f : 5.0f);
            buf_f32le(b, val_idx == 0 ? 2.0f : 6.0f);
            buf_f32le(b, val_idx == 0 ? 3.0f : 7.0f);
            buf_f32le(b, val_idx == 0 ? 4.0f : 8.0f);
            break;
        case 15: /* COLOR (RGBA) */
            buf_u8(b, val_idx == 0 ? 255 : 0);
            buf_u8(b, val_idx == 0 ? 0   : 255);
            buf_u8(b, val_idx == 0 ? 128 : 0);
            buf_u8(b, val_idx == 0 ? 255 : 128);
            break;
        case 16: /* STRING (UTF-16LE NUL-terminated) */
            buf_utf16le(b, val_idx == 0 ? "hello" : "world");
            break;
        default:
            TEST_FAIL_MESSAGE("unknown field type in buf_write_value");
    }
}

/* Field type assignments: 16 fields across 3 params.
 * Param 0: types 1..6  (6 fields)
 * Param 1: types 7..12 (6 fields)
 * Param 2: types 13..16 (4 fields)
 */
static const int k_param0_types[6] = { 1, 2, 3, 4, 5, 6 };
static const int k_param1_types[6] = { 7, 8, 9, 10, 11, 12 };
static const int k_param2_types[4] = { 13, 14, 15, 16 };

static const char *k_param0_keys[6]  = { "sbyte_key", "short_key", "int_key", "long_key", "byte_key", "ushort_key" };
static const char *k_param0_names[6] = { "SbyteField", "ShortField", "IntField", "LongField", "ByteField", "UshortField" };
static const char *k_param1_keys[6]  = { "uint_key", "ulong_key", "float_key", "double_key", "bool_key", "vec2_key" };
static const char *k_param1_names[6] = { "UintField", "UlongField", "FloatField", "DoubleField", "BoolField", "Vec2Field" };
static const char *k_param2_keys[4]  = { "vec3_key", "vec4_key", "color_key", "str_key" };
static const char *k_param2_names[4] = { "Vec3Field", "Vec4Field", "ColorField", "StringField" };

#define NUM_PARAMS 3
#define P0_FIELDS  6
#define P1_FIELDS  6
#define P2_FIELDS  4
#define NUM_VALUES 2  /* per field */

static const int k_field_counts[NUM_PARAMS] = { P0_FIELDS, P1_FIELDS, P2_FIELDS };
static const int *k_field_types[NUM_PARAMS] = { k_param0_types, k_param1_types, k_param2_types };
static const char **k_field_keys[NUM_PARAMS]  = { k_param0_keys,  k_param1_keys,  k_param2_keys  };
static const char **k_field_names[NUM_PARAMS] = { k_param0_names, k_param1_names, k_param2_names };

static const char *k_param_keys[NUM_PARAMS]  = { "Dof", "Fog", "Sky" };
static const char *k_param_names[NUM_PARAMS] = { "DofParam", "FogParam", "SkyParam" };

/* Param 0 has 1 comment; others have 0. */
static const char *k_param0_comment = "DofComment";

/* UnkParamExtra: 1 entry, ids=[10,20], unk00=42, unk0c=99 */
#define NUM_EXTRAS 1
static const int32_t k_extra_ids[2] = { 10, 20 };

/*
 * Build a complete GPARAM V5 or V6 wire blob.
 * Returns a heap-allocated buffer; caller must free().
 * Sets *out_size to the buffer length.
 */
static uint8_t *make_gparam(uint32_t version, size_t *out_size) {
    buf_t b;
    buf_init(&b);

    /* ---- Header ---- */
    /* Magic: UTF-16LE "filt" */
    buf_u8(&b, 0x66); buf_u8(&b, 0x00);
    buf_u8(&b, 0x69); buf_u8(&b, 0x00);
    buf_u8(&b, 0x6C); buf_u8(&b, 0x00);
    buf_u8(&b, 0x74); buf_u8(&b, 0x00);

    buf_u32le(&b, version);   /* version */
    buf_u8(&b, 0);            /* assert 0 */
    buf_u8(&b, 1);            /* unk0d = true */
    buf_i16le(&b, 0);         /* assert 0 */
    buf_i32le(&b, NUM_PARAMS);/* param_count */
    buf_i32le(&b, 7);         /* count14 */

    /* Reserve slots for 12 base offsets (i32 each) + extra_capacity + unk40 */
    /* We'll patch these after computing actual positions. */
    size_t slot_param_offsets_base      = b.len; buf_i32le(&b, 0);
    size_t slot_params_base             = b.len; buf_i32le(&b, 0);
    size_t slot_field_offsets_base      = b.len; buf_i32le(&b, 0);
    size_t slot_fields_base             = b.len; buf_i32le(&b, 0);
    size_t slot_values_base             = b.len; buf_i32le(&b, 0);
    size_t slot_value_ids_base          = b.len; buf_i32le(&b, 0);
    size_t slot_unk30_base              = b.len; buf_i32le(&b, 0);
    buf_i32le(&b, NUM_EXTRAS);                   /* extra_capacity */
    size_t slot_param_extras_base       = b.len; buf_i32le(&b, 0);
    size_t slot_param_extra_ids_base    = b.len; buf_i32le(&b, 0);
    buf_f32le(&b, 1.25f);                        /* unk40 */
    size_t slot_param_comments_offsets_base = b.len; buf_i32le(&b, 0);
    size_t slot_comment_offsets_base    = b.len; buf_i32le(&b, 0);
    size_t slot_comments_base           = b.len; buf_i32le(&b, 0);

    /* V>=V5: unk50 */
    if (version >= 5) {
        buf_f32le(&b, 2.5f); /* unk50 */
    }

    /* ---- ParamOffsets section ---- */
    int32_t param_offsets_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_param_offsets_base, param_offsets_base);

    /* Reserve NUM_PARAMS slots for param offsets (relative to params_base) */
    size_t param_offset_slots[NUM_PARAMS];
    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        param_offset_slots[pi] = b.len;
        buf_i32le(&b, 0);
    }

    /* ---- Params section ---- */
    int32_t params_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_params_base, params_base);

    /* Reserve slots for each param's field_offsets_offset (relative to field_offsets_base) */
    size_t param_field_offsets_offset_slots[NUM_PARAMS];

    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        /* Fill param offset (relative to params_base) */
        buf_patch_i32le(&b, param_offset_slots[pi], (int32_t)(b.len - (size_t)params_base));

        buf_i32le(&b, k_field_counts[pi]);  /* field_count */
        param_field_offsets_offset_slots[pi] = b.len;
        buf_i32le(&b, 0);                   /* field_offsets_offset (patched later) */
        buf_utf16le(&b, k_param_keys[pi]);
        buf_utf16le(&b, k_param_names[pi]);
        buf_pad(&b, 4);
    }

    /* ---- FieldOffsets section ---- */
    int32_t field_offsets_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_field_offsets_base, field_offsets_base);

    /* Per-param sub-tables of field offsets (relative to fields_base) */
    size_t field_offset_slots[NUM_PARAMS][P0_FIELDS]; /* P0_FIELDS >= all */
    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        /* Fill param's field_offsets_offset (relative to field_offsets_base) */
        buf_patch_i32le(&b, param_field_offsets_offset_slots[pi],
                        (int32_t)(b.len - (size_t)field_offsets_base));
        for (int fi = 0; fi < k_field_counts[pi]; fi++) {
            field_offset_slots[pi][fi] = b.len;
            buf_i32le(&b, 0);
        }
    }

    /* ---- Fields section ---- */
    int32_t fields_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_fields_base, fields_base);

    /* Reserve slots for values_offset and value_ids_offset per field */
    size_t field_values_offset_slots[NUM_PARAMS][P0_FIELDS];
    size_t field_value_ids_offset_slots[NUM_PARAMS][P0_FIELDS];

    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        for (int fi = 0; fi < k_field_counts[pi]; fi++) {
            /* Fill field offset (relative to fields_base) */
            buf_patch_i32le(&b, field_offset_slots[pi][fi],
                            (int32_t)(b.len - (size_t)fields_base));

            field_values_offset_slots[pi][fi] = b.len;
            buf_i32le(&b, 0);   /* values_offset (patched later) */
            field_value_ids_offset_slots[pi][fi] = b.len;
            buf_i32le(&b, 0);   /* value_ids_offset (patched later) */

            int ftype = k_field_types[pi][fi];
            if (version < 6) {
                /* V<V6: [type byte][capacity sbyte][unk i16=0] */
                buf_u8(&b, (uint8_t)ftype);
                buf_i8(&b, (int8_t)NUM_VALUES);
                buf_i16le(&b, 0);
            } else {
                /* V>=V6: [capacity i16][type byte][unk byte=0] */
                buf_i16le(&b, (int16_t)NUM_VALUES);
                buf_u8(&b, (uint8_t)ftype);
                buf_u8(&b, 0);
            }

            buf_utf16le(&b, k_field_keys[pi][fi]);
            buf_utf16le(&b, k_field_names[pi][fi]);
            buf_pad(&b, 4);
        }
    }

    /* ---- Values section ---- */
    int32_t values_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_values_base, values_base);

    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        for (int fi = 0; fi < k_field_counts[pi]; fi++) {
            /* Fill values_offset (relative to values_base) */
            buf_patch_i32le(&b, field_values_offset_slots[pi][fi],
                            (int32_t)(b.len - (size_t)values_base));

            int ftype = k_field_types[pi][fi];
            for (int vi = 0; vi < NUM_VALUES; vi++) {
                buf_write_value(&b, ftype, vi);
            }
            buf_pad(&b, 4);
        }
    }

    /* ---- ValueIds section ---- */
    int32_t value_ids_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_value_ids_base, value_ids_base);

    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        for (int fi = 0; fi < k_field_counts[pi]; fi++) {
            /* Fill value_ids_offset (relative to value_ids_base) */
            buf_patch_i32le(&b, field_value_ids_offset_slots[pi][fi],
                            (int32_t)(b.len - (size_t)value_ids_base));

            for (int vi = 0; vi < NUM_VALUES; vi++) {
                /* id = pi*100 + fi*10 + vi */
                buf_i32le(&b, pi * 100 + fi * 10 + vi);
                if (version >= 5) {
                    /* unk04 float */
                    buf_f32le(&b, (float)(pi * 10 + fi + vi));
                }
            }
        }
    }

    /* ---- Data30 (empty) + pad(4) ---- */
    int32_t unk30_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_unk30_base, unk30_base);
    /* No data30 bytes — just pad to 4-byte alignment */
    buf_pad(&b, 4);

    /* ---- ParamExtras section ---- */
    int32_t param_extras_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_param_extras_base, param_extras_base);

    /* Reserve slot for extra[0]'s ids_offset */
    size_t extra_ids_offset_slot = b.len + 8; /* unk00(4) + id_count(4) = 8 bytes ahead */
    buf_i32le(&b, 42);          /* unk00 */
    buf_i32le(&b, 2);           /* id_count */
    size_t extra_ids_offset_slot_actual = b.len;
    buf_i32le(&b, 0);           /* ids_offset (patched later) */
    if (version >= 5) {
        buf_i32le(&b, 99);      /* unk0c */
    }
    (void)extra_ids_offset_slot; /* suppress unused warning */

    /* ---- ParamExtraIds section ---- */
    int32_t param_extra_ids_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_param_extra_ids_base, param_extra_ids_base);

    /* Fill extra[0]'s ids_offset (relative to param_extra_ids_base) */
    buf_patch_i32le(&b, extra_ids_offset_slot_actual,
                    (int32_t)(b.len - (size_t)param_extra_ids_base));
    buf_i32le(&b, k_extra_ids[0]);
    buf_i32le(&b, k_extra_ids[1]);

    /* ---- ParamCommentsOffsets section ---- */
    int32_t param_comments_offsets_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_param_comments_offsets_base, param_comments_offsets_base);

    /* Reserve per-param comment_offsets_offset slots (relative to comment_offsets_base) */
    size_t param_comment_offsets_offset_slots[NUM_PARAMS];
    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        param_comment_offsets_offset_slots[pi] = b.len;
        buf_i32le(&b, 0);
    }

    /* ---- CommentOffsets section ---- */
    int32_t comment_offsets_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_comment_offsets_base, comment_offsets_base);

    /* Param 0 has 1 comment; params 1 and 2 have 0. */
    /* Fill param_comment_offsets_offset for each param */
    size_t comment_offset_slot = 0; /* slot for param0's comment[0] offset */
    for (int pi = 0; pi < NUM_PARAMS; pi++) {
        buf_patch_i32le(&b, param_comment_offsets_offset_slots[pi],
                        (int32_t)(b.len - (size_t)comment_offsets_base));
        if (pi == 0) {
            /* 1 comment: reserve its offset slot */
            comment_offset_slot = b.len;
            buf_i32le(&b, 0);
        }
        /* params 1 and 2: 0 comments, no entries */
    }

    /* ---- Comments section ---- */
    int32_t comments_base = (int32_t)b.len;
    buf_patch_i32le(&b, slot_comments_base, comments_base);

    /* Fill param0's comment[0] offset (relative to comments_base) */
    buf_patch_i32le(&b, comment_offset_slot,
                    (int32_t)(b.len - (size_t)comments_base));
    buf_utf16le(&b, k_param0_comment);
    buf_pad(&b, 4);

    *out_size = b.len;
    return b.data; /* caller owns */
}

/* =========================================================================
 * Round-trip helper
 * ========================================================================= */

static void do_roundtrip(uint32_t version) {
    size_t   raw_size = 0;
    uint8_t *raw      = make_gparam(version, &raw_size);
    TEST_ASSERT_NOT_NULL(raw);

    /* ---- First read ---- */
    sf_gparam_t *g1 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_gparam_read_from_memory(&g1, raw, raw_size, NULL));
    TEST_ASSERT_NOT_NULL(g1);

    /* Verify top-level header */
    TEST_ASSERT_EQUAL_INT((int)version, (int)sf_gparam_get_version(g1));
    TEST_ASSERT_TRUE(sf_gparam_get_unk0d(g1));
    TEST_ASSERT_EQUAL_INT32(7, sf_gparam_get_count14(g1));
    TEST_ASSERT_EQUAL_FLOAT(1.25f, sf_gparam_get_unk40(g1));
    if (version >= 5) {
        TEST_ASSERT_EQUAL_FLOAT(2.5f, sf_gparam_get_unk50(g1));
    }

    /* Verify param count */
    TEST_ASSERT_EQUAL_size_t(NUM_PARAMS, sf_gparam_param_count(g1));

    /* Verify param 0 */
    const sf_gparam_param_t *p0 = sf_gparam_get_param(g1, 0);
    TEST_ASSERT_NOT_NULL(p0);
    TEST_ASSERT_EQUAL_STRING("Dof",      sf_gparam_param_get_key(p0));
    TEST_ASSERT_EQUAL_STRING("DofParam", sf_gparam_param_get_name(p0));
    TEST_ASSERT_EQUAL_size_t(P0_FIELDS,  sf_gparam_param_field_count(p0));
    TEST_ASSERT_EQUAL_size_t(1,          sf_gparam_param_comment_count(p0));
    TEST_ASSERT_EQUAL_STRING(k_param0_comment, sf_gparam_param_get_comment(p0, 0));

    /* Verify param 1 */
    const sf_gparam_param_t *p1 = sf_gparam_get_param(g1, 1);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_EQUAL_STRING("Fog",      sf_gparam_param_get_key(p1));
    TEST_ASSERT_EQUAL_STRING("FogParam", sf_gparam_param_get_name(p1));
    TEST_ASSERT_EQUAL_size_t(P1_FIELDS,  sf_gparam_param_field_count(p1));
    TEST_ASSERT_EQUAL_size_t(0,          sf_gparam_param_comment_count(p1));

    /* Verify param 2 */
    const sf_gparam_param_t *p2 = sf_gparam_get_param(g1, 2);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_STRING("Sky",      sf_gparam_param_get_key(p2));
    TEST_ASSERT_EQUAL_STRING("SkyParam", sf_gparam_param_get_name(p2));
    TEST_ASSERT_EQUAL_size_t(P2_FIELDS,  sf_gparam_param_field_count(p2));

    /* Verify all 16 FieldType variants */
    /* Param 0: SBYTE, SHORT, INT, LONG, BYTE, USHORT */
    {
        const sf_gparam_field_t *f;

        f = sf_gparam_param_get_field(p0, 0);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_SBYTE, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_size_t(NUM_VALUES, sf_gparam_field_value_count(f));
        TEST_ASSERT_EQUAL_INT8(-1, sf_gparam_field_get_value(f, 0).v.as_sbyte);
        TEST_ASSERT_EQUAL_INT8(2,  sf_gparam_field_get_value(f, 1).v.as_sbyte);

        f = sf_gparam_param_get_field(p0, 1);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_SHORT, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_INT16(-100, sf_gparam_field_get_value(f, 0).v.as_short);
        TEST_ASSERT_EQUAL_INT16(200,  sf_gparam_field_get_value(f, 1).v.as_short);

        f = sf_gparam_param_get_field(p0, 2);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_INT, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_INT32(-1000, sf_gparam_field_get_value(f, 0).v.as_int);
        TEST_ASSERT_EQUAL_INT32(2000,  sf_gparam_field_get_value(f, 1).v.as_int);

        f = sf_gparam_param_get_field(p0, 3);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_LONG, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_INT64(-1000000LL, sf_gparam_field_get_value(f, 0).v.as_long);
        TEST_ASSERT_EQUAL_INT64(2000000LL,  sf_gparam_field_get_value(f, 1).v.as_long);

        f = sf_gparam_param_get_field(p0, 4);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_BYTE, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_UINT8(255, sf_gparam_field_get_value(f, 0).v.as_byte);
        TEST_ASSERT_EQUAL_UINT8(0,   sf_gparam_field_get_value(f, 1).v.as_byte);

        f = sf_gparam_param_get_field(p0, 5);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_USHORT, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_UINT16(65535, sf_gparam_field_get_value(f, 0).v.as_ushort);
        TEST_ASSERT_EQUAL_UINT16(1,     sf_gparam_field_get_value(f, 1).v.as_ushort);
    }

    /* Param 1: UINT, ULONG, FLOAT, DOUBLE, BOOL, VEC2 */
    {
        const sf_gparam_field_t *f;

        f = sf_gparam_param_get_field(p1, 0);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_UINT, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, sf_gparam_field_get_value(f, 0).v.as_uint);
        TEST_ASSERT_EQUAL_UINT32(1u,          sf_gparam_field_get_value(f, 1).v.as_uint);

        f = sf_gparam_param_get_field(p1, 1);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_ULONG, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFull, sf_gparam_field_get_value(f, 0).v.as_ulong);
        TEST_ASSERT_EQUAL_UINT64(1ull,                  sf_gparam_field_get_value(f, 1).v.as_ulong);

        f = sf_gparam_param_get_field(p1, 2);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_FLOAT, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_FLOAT(1.5f,  sf_gparam_field_get_value(f, 0).v.as_float);
        TEST_ASSERT_EQUAL_FLOAT(-2.5f, sf_gparam_field_get_value(f, 1).v.as_float);

        f = sf_gparam_param_get_field(p1, 3);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_DOUBLE, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_DOUBLE(3.14,  sf_gparam_field_get_value(f, 0).v.as_double);
        TEST_ASSERT_EQUAL_DOUBLE(-2.71, sf_gparam_field_get_value(f, 1).v.as_double);

        f = sf_gparam_param_get_field(p1, 4);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_BOOL, sf_gparam_field_get_type(f));
        TEST_ASSERT_NOT_EQUAL_INT(0, sf_gparam_field_get_value(f, 0).v.as_bool);
        TEST_ASSERT_EQUAL_INT(0,    sf_gparam_field_get_value(f, 1).v.as_bool);

        f = sf_gparam_param_get_field(p1, 5);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_VEC2, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_gparam_field_get_value(f, 0).v.as_vec2.x);
        TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_gparam_field_get_value(f, 0).v.as_vec2.y);
        TEST_ASSERT_EQUAL_FLOAT(3.0f, sf_gparam_field_get_value(f, 1).v.as_vec2.x);
        TEST_ASSERT_EQUAL_FLOAT(4.0f, sf_gparam_field_get_value(f, 1).v.as_vec2.y);
    }

    /* Param 2: VEC3, VEC4, COLOR, STRING */
    {
        const sf_gparam_field_t *f;

        f = sf_gparam_param_get_field(p2, 0);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_VEC3, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_gparam_field_get_value(f, 0).v.as_vec3.x);
        TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_gparam_field_get_value(f, 0).v.as_vec3.y);
        TEST_ASSERT_EQUAL_FLOAT(3.0f, sf_gparam_field_get_value(f, 0).v.as_vec3.z);
        TEST_ASSERT_EQUAL_FLOAT(4.0f, sf_gparam_field_get_value(f, 1).v.as_vec3.x);

        f = sf_gparam_param_get_field(p2, 1);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_VEC4, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_gparam_field_get_value(f, 0).v.as_vec4.x);
        TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_gparam_field_get_value(f, 0).v.as_vec4.y);
        TEST_ASSERT_EQUAL_FLOAT(3.0f, sf_gparam_field_get_value(f, 0).v.as_vec4.z);
        TEST_ASSERT_EQUAL_FLOAT(4.0f, sf_gparam_field_get_value(f, 0).v.as_vec4.w);
        TEST_ASSERT_EQUAL_FLOAT(5.0f, sf_gparam_field_get_value(f, 1).v.as_vec4.x);

        f = sf_gparam_param_get_field(p2, 2);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_COLOR, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_UINT8(255, sf_gparam_field_get_value(f, 0).v.as_color.r);
        TEST_ASSERT_EQUAL_UINT8(0,   sf_gparam_field_get_value(f, 0).v.as_color.g);
        TEST_ASSERT_EQUAL_UINT8(128, sf_gparam_field_get_value(f, 0).v.as_color.b);
        TEST_ASSERT_EQUAL_UINT8(255, sf_gparam_field_get_value(f, 0).v.as_color.a);

        f = sf_gparam_param_get_field(p2, 3);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_GPARAM_FIELD_TYPE_STRING, sf_gparam_field_get_type(f));
        TEST_ASSERT_EQUAL_STRING("hello", sf_gparam_field_get_value(f, 0).v.as_string);
        TEST_ASSERT_EQUAL_STRING("world", sf_gparam_field_get_value(f, 1).v.as_string);
    }

    /* Verify UnkParamExtra */
    TEST_ASSERT_EQUAL_size_t(NUM_EXTRAS, sf_gparam_unk_param_extra_count(g1));

    /* ---- First write ---- */
    void  *buf1 = NULL;
    size_t sz1  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_gparam_write_to_buffer(g1, &buf1, &sz1, NULL));
    TEST_ASSERT_NOT_NULL(buf1);
    TEST_ASSERT_GREATER_THAN(0u, sz1);

    /* ---- Second read (from first write) ---- */
    sf_gparam_t *g2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_gparam_read_from_memory(&g2, buf1, sz1, NULL));
    TEST_ASSERT_NOT_NULL(g2);

    /* ---- Second write ---- */
    void  *buf2 = NULL;
    size_t sz2  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_gparam_write_to_buffer(g2, &buf2, &sz2, NULL));
    TEST_ASSERT_NOT_NULL(buf2);
    TEST_ASSERT_EQUAL_size_t(sz1, sz2);

    /* ---- Byte-compare both writes (round-trip identity) ---- */
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, sz1);

    sf_free(NULL, buf1);
    sf_free(NULL, buf2);
    sf_gparam_destroy(g1);
    sf_gparam_destroy(g2);
    free(raw);
}

/* =========================================================================
 * Test cases
 * ========================================================================= */

static void test_gparam_v5_roundtrip(void) {
    do_roundtrip(5);
}

static void test_gparam_v6_roundtrip(void) {
    do_roundtrip(6);
}

static void test_gparam_v2_rejected(void) {
    /* Build a minimal V2 blob (just magic + version) and verify rejection. */
    buf_t b;
    buf_init(&b);
    /* Magic */
    buf_u8(&b, 0x66); buf_u8(&b, 0x00);
    buf_u8(&b, 0x69); buf_u8(&b, 0x00);
    buf_u8(&b, 0x6C); buf_u8(&b, 0x00);
    buf_u8(&b, 0x74); buf_u8(&b, 0x00);
    buf_u32le(&b, 2); /* version = 2 */
    /* Pad to 64 bytes so the reader doesn't trip on a short read before
     * it gets to the version check. */
    while (b.len < 64) buf_u8(&b, 0);

    sf_gparam_t *g = NULL;
    sf_result_t  r = sf_gparam_read_from_memory(&g, b.data, b.len, NULL);
    TEST_ASSERT_NOT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NULL(g);

    buf_free(&b);
}

static void test_gparam_empty_params(void) {
    /* Build a V5 GPARAM with 0 params, 0 extras, 0 comments. */
    buf_t b;
    buf_init(&b);

    /* Magic */
    buf_u8(&b, 0x66); buf_u8(&b, 0x00);
    buf_u8(&b, 0x69); buf_u8(&b, 0x00);
    buf_u8(&b, 0x6C); buf_u8(&b, 0x00);
    buf_u8(&b, 0x74); buf_u8(&b, 0x00);

    buf_u32le(&b, 5);  /* version */
    buf_u8(&b, 0);     /* assert 0 */
    buf_u8(&b, 0);     /* unk0d = false */
    buf_i16le(&b, 0);  /* assert 0 */
    buf_i32le(&b, 0);  /* param_count = 0 */
    buf_i32le(&b, 0);  /* count14 */

    /* All base offsets point to the same location (right after header) */
    /* Header ends at 0x54 (with unk50) */
    /* With 0 params, all sections are empty, so all bases = 0x54 */
    size_t slot_po  = b.len; buf_i32le(&b, 0); /* param_offsets_base */
    size_t slot_p   = b.len; buf_i32le(&b, 0); /* params_base */
    size_t slot_fo  = b.len; buf_i32le(&b, 0); /* field_offsets_base */
    size_t slot_f   = b.len; buf_i32le(&b, 0); /* fields_base */
    size_t slot_v   = b.len; buf_i32le(&b, 0); /* values_base */
    size_t slot_vi  = b.len; buf_i32le(&b, 0); /* value_ids_base */
    size_t slot_u30 = b.len; buf_i32le(&b, 0); /* unk30_base */
    buf_i32le(&b, 0);                           /* extra_capacity = 0 */
    size_t slot_pe  = b.len; buf_i32le(&b, 0); /* param_extras_base */
    size_t slot_pei = b.len; buf_i32le(&b, 0); /* param_extra_ids_base */
    buf_f32le(&b, 0.0f);                        /* unk40 */
    size_t slot_pco = b.len; buf_i32le(&b, 0); /* param_comments_offsets_base */
    size_t slot_co  = b.len; buf_i32le(&b, 0); /* comment_offsets_base */
    size_t slot_c   = b.len; buf_i32le(&b, 0); /* comments_base */
    buf_f32le(&b, 0.0f);                        /* unk50 (V5) */

    /* All sections are empty — all bases point here */
    int32_t here = (int32_t)b.len;
    buf_patch_i32le(&b, slot_po,  here);
    buf_patch_i32le(&b, slot_p,   here);
    buf_patch_i32le(&b, slot_fo,  here);
    buf_patch_i32le(&b, slot_f,   here);
    buf_patch_i32le(&b, slot_v,   here);
    buf_patch_i32le(&b, slot_vi,  here);
    buf_patch_i32le(&b, slot_u30, here);
    buf_patch_i32le(&b, slot_pe,  here);
    buf_patch_i32le(&b, slot_pei, here);
    buf_patch_i32le(&b, slot_pco, here);
    buf_patch_i32le(&b, slot_co,  here);
    buf_patch_i32le(&b, slot_c,   here);

    /* Data30 pad(4) — already aligned */
    buf_pad(&b, 4);

    sf_gparam_t *g = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_gparam_read_from_memory(&g, b.data, b.len, NULL));
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_size_t(0, sf_gparam_param_count(g));
    TEST_ASSERT_EQUAL_size_t(0, sf_gparam_unk_param_extra_count(g));

    void  *out = NULL;
    size_t sz  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_gparam_write_to_buffer(g, &out, &sz, NULL));
    TEST_ASSERT_NOT_NULL(out);

    /* Read back and re-write — must be byte-equal */
    sf_gparam_t *g2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_gparam_read_from_memory(&g2, out, sz, NULL));
    void  *out2 = NULL;
    size_t sz2  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_gparam_write_to_buffer(g2, &out2, &sz2, NULL));
    TEST_ASSERT_EQUAL_size_t(sz, sz2);
    TEST_ASSERT_EQUAL_MEMORY(out, out2, sz);

    sf_free(NULL, out);
    sf_free(NULL, out2);
    sf_gparam_destroy(g);
    sf_gparam_destroy(g2);
    buf_free(&b);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gparam_v5_roundtrip);
    RUN_TEST(test_gparam_v6_roundtrip);
    RUN_TEST(test_gparam_v2_rejected);
    RUN_TEST(test_gparam_empty_params);
    return UNITY_END();
}
