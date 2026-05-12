/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FLVER common helpers — half-float, 11_11_10 packing, LayoutType sizing,
 * Dummy / Node / LayoutMember serializers.
 *
 * Strict mirror of upstream SoulsFormatsNEXT at the pinned commit:
 *   - SoulsFormats/Utilities/BinaryReaderEx.cs   (ReadHalf)
 *   - SoulsFormats/Utilities/BinaryWriterEx.cs   (WriteHalf)
 *   - SoulsFormats/Formats/FLVER/Dummy.cs        (Read / Write)
 *   - SoulsFormats/Formats/FLVER/Node.cs         (Read / Write / WriteStrings)
 *   - SoulsFormats/Formats/FLVER/LayoutMember.cs (Read / Write / Size)
 */

#include "souls_formats/sf_flver.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_common.h"

#include "internal/flver_common_internal.h"
#include "internal/sf_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*===========================================================================
 * IEEE 754 half-precision (16-bit) ↔ single-precision (32-bit)
 *
 * Layout of a binary16 value:
 *   sign  : bit 15
 *   exp   : bits 14..10  (5-bit, biased by 15)
 *   mant  : bits  9..0   (10-bit)
 *
 * Reference: IEEE 754-2008 §3.6.
 *
 * The algorithm covers ±0, subnormals (renormalized into binary32 normals),
 * normals (exponent rebias 15 → 127), and inf/NaN (preserves payload).
 *===========================================================================*/

SF_API float sf_half_to_float(uint16_t h) {
    uint32_t sign = (uint32_t)(h >> 15) << 31;
    uint32_t exp  = (uint32_t)((h >> 10) & 0x1Fu);
    uint32_t mant = (uint32_t)(h & 0x3FFu);
    uint32_t result;

    if (exp == 0u) {
        if (mant == 0u) {
            result = sign;
        } else {
            /* subnormal: shift mantissa until the implicit 1 bit appears */
            uint32_t e = 1u;
            while ((mant & 0x400u) == 0u) {
                mant <<= 1;
                e--;
            }
            mant &= 0x3FFu;
            result = sign | ((e + 112u) << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        /* inf or NaN — preserve mantissa payload */
        result = sign | 0x7F800000u | (mant << 13);
    } else {
        /* normal — rebias exponent: (exp - 15) + 127 = exp + 112 */
        result = sign | ((exp + 112u) << 23) | (mant << 13);
    }

    float f;
    memcpy(&f, &result, sizeof(f));
    return f;
}

SF_API uint16_t sf_float_to_half(float f) {
    uint32_t x;
    memcpy(&x, &f, sizeof(x));

    uint32_t sign = (x >> 16) & 0x8000u;
    int32_t  exp  = (int32_t)((x >> 23) & 0xFFu) - 127 + 15;
    uint32_t mant = x & 0x7FFFFFu;

    if (exp >= 31) {
        if ((x & 0x7FFFFFFFu) > 0x7F800000u) {
            /* NaN: preserve quiet bit + at least one mantissa bit */
            return (uint16_t)(sign | 0x7E00u);
        }
        /* inf or overflow → inf */
        return (uint16_t)(sign | 0x7C00u);
    }

    if (exp <= 0) {
        if (exp < -10) {
            /* underflow → signed zero */
            return (uint16_t)sign;
        }
        /* subnormal: re-add implicit 1 bit, then right-shift into place */
        mant |= 0x800000u;
        uint32_t shift = (uint32_t)(1 - exp) + 13u;
        return (uint16_t)(sign | (mant >> shift));
    }

    return (uint16_t)(sign | (uint32_t)(exp << 10) | (mant >> 13));
}

/*===========================================================================
 * 11_11_10 packed normal/tangent
 *
 * Encoding (mirrors upstream BinaryReaderEx.Read11_11_10Vector3):
 *   bits  0..10  : X — 11-bit signed two's complement, normalized by 1023
 *   bits 11..21  : Y — 11-bit signed two's complement, normalized by 1023
 *   bits 22..31  : Z — 10-bit signed two's complement, normalized by 511
 *===========================================================================*/

SF_API void sf_unpack_11_11_10(uint32_t packed, float *out_x, float *out_y, float *out_z) {
    if (!out_x || !out_y || !out_z) return;

    int32_t xi = (int32_t)(packed         & 0x7FFu);
    int32_t yi = (int32_t)((packed >> 11) & 0x7FFu);
    int32_t zi = (int32_t)((packed >> 22) & 0x3FFu);

    /* sign-extend 11-bit / 10-bit values */
    if (xi > 1023) xi -= 2048;
    if (yi > 1023) yi -= 2048;
    if (zi >  511) zi -= 1024;

    *out_x = (float)xi / 1023.0f;
    *out_y = (float)yi / 1023.0f;
    *out_z = (float)zi /  511.0f;
}

SF_API uint32_t sf_pack_11_11_10(float x, float y, float z) {
    /* sign-aware rounding to nearest integer */
    float fx = x * 1023.0f;
    float fy = y * 1023.0f;
    float fz = z *  511.0f;
    int32_t xi = (int32_t)(fx + (fx >= 0.0f ? 0.5f : -0.5f));
    int32_t yi = (int32_t)(fy + (fy >= 0.0f ? 0.5f : -0.5f));
    int32_t zi = (int32_t)(fz + (fz >= 0.0f ? 0.5f : -0.5f));

    /* clamp to representable range */
    if (xi < -1024) xi = -1024; else if (xi > 1023) xi = 1023;
    if (yi < -1024) yi = -1024; else if (yi > 1023) yi = 1023;
    if (zi <  -512) zi =  -512; else if (zi >  511) zi =  511;

    return ((uint32_t)xi & 0x7FFu)
         | (((uint32_t)yi & 0x7FFu) << 11)
         | (((uint32_t)zi & 0x3FFu) << 22);
}

/*===========================================================================
 * LayoutType → byte size
 *
 * Mirrors upstream LayoutMember.Size, with two C-style adaptations
 * documented at sf_flver.h:
 *   - special_modifier == -32768 (SpeedTree sentinel) → 0
 *   - LayoutType.EdgeCompressed → UINT32_MAX (unsupported in v1; callers
 *     interpret this as SF_ERR_UNSUPPORTED_VERSION territory)
 *===========================================================================*/

SF_API uint32_t sf_flver_layout_type_size(sf_flver_layout_type_t t, int32_t special_modifier) {
    if (special_modifier == -32768) return 0u;

    switch (t) {
        case SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED:
            return UINT32_MAX;

        case SF_FLVER_LAYOUT_TYPE_FLOAT1:
        case SF_FLVER_LAYOUT_TYPE_COLOR:
        case SF_FLVER_LAYOUT_TYPE_UBYTE4:
        case SF_FLVER_LAYOUT_TYPE_BYTE4:
        case SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM:
        case SF_FLVER_LAYOUT_TYPE_BYTE4_NORM:
        case SF_FLVER_LAYOUT_TYPE_SHORT2:
        case SF_FLVER_LAYOUT_TYPE_USHORT2:
        case SF_FLVER_LAYOUT_TYPE_BYTE4E:
        case SF_FLVER_LAYOUT_TYPE_HALF2:
            return 4u;

        case SF_FLVER_LAYOUT_TYPE_FLOAT2:
        case SF_FLVER_LAYOUT_TYPE_SHORT4:
        case SF_FLVER_LAYOUT_TYPE_USHORT4:
        case SF_FLVER_LAYOUT_TYPE_SHORT4_NORM:
        case SF_FLVER_LAYOUT_TYPE_HALF4:
            return 8u;

        case SF_FLVER_LAYOUT_TYPE_FLOAT3:
            return 12u;

        case SF_FLVER_LAYOUT_TYPE_FLOAT4:
            return 16u;
    }
    return UINT32_MAX;
}

/*===========================================================================
 * Dummy — 64-byte record
 *===========================================================================*/

sf_result_t sfi_flver_dummy_read(sf_binary_reader_t *br, sf_flver_dummy_t *out) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t e;
    uint8_t color_bytes[4];

    if ((e = sf_binary_reader_read_vec3 (br, &out->position))           != SF_OK) return e;
    if ((e = sf_binary_reader_read_bytes(br, color_bytes, 4))           != SF_OK) return e;
    /* on-disk ARGB byte order → ARGB packed u32 (A in high byte) */
    out->color = ((uint32_t)color_bytes[0] << 24)
               | ((uint32_t)color_bytes[1] << 16)
               | ((uint32_t)color_bytes[2] <<  8)
               |  (uint32_t)color_bytes[3];

    if ((e = sf_binary_reader_read_vec3(br, &out->forward))             != SF_OK) return e;
    if ((e = sf_binary_reader_read_i16 (br, &out->reference_id))        != SF_OK) return e;
    if ((e = sf_binary_reader_read_i16 (br, &out->parent_bone_index))   != SF_OK) return e;
    if ((e = sf_binary_reader_read_vec3(br, &out->upward))              != SF_OK) return e;
    if ((e = sf_binary_reader_read_i16 (br, &out->attach_bone_index))   != SF_OK) return e;
    if ((e = sf_binary_reader_read_bool(br, &out->flag1))               != SF_OK) return e;
    if ((e = sf_binary_reader_read_bool(br, &out->use_upward_vector))   != SF_OK) return e;
    if ((e = sf_binary_reader_read_i32 (br, &out->unk30))               != SF_OK) return e;
    if ((e = sf_binary_reader_read_i32 (br, &out->unk34))               != SF_OK) return e;
    if ((e = sf_binary_reader_assert_i32_one(br, 0))                    != SF_OK) return e;
    if ((e = sf_binary_reader_assert_i32_one(br, 0))                    != SF_OK) return e;

    return SF_OK;
}

sf_result_t sfi_flver_dummy_write(sf_binary_writer_t *bw, const sf_flver_dummy_t *d) {
    SF_CHECK_ARG(bw != NULL && d != NULL);

    sf_result_t e;
    uint8_t color_bytes[4] = {
        (uint8_t)((d->color >> 24) & 0xFFu),
        (uint8_t)((d->color >> 16) & 0xFFu),
        (uint8_t)((d->color >>  8) & 0xFFu),
        (uint8_t) (d->color        & 0xFFu),
    };

    if ((e = sf_binary_writer_write_vec3 (bw, d->position))             != SF_OK) return e;
    if ((e = sf_binary_writer_write_bytes(bw, color_bytes, 4))          != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3 (bw, d->forward))              != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16  (bw, d->reference_id))         != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16  (bw, d->parent_bone_index))    != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3 (bw, d->upward))               != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16  (bw, d->attach_bone_index))    != SF_OK) return e;
    if ((e = sf_binary_writer_write_bool (bw, d->flag1))                != SF_OK) return e;
    if ((e = sf_binary_writer_write_bool (bw, d->use_upward_vector))    != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32  (bw, d->unk30))                != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32  (bw, d->unk34))                != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32  (bw, 0))                       != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32  (bw, 0))                       != SF_OK) return e;

    return SF_OK;
}

/*===========================================================================
 * Node — 128-byte record + name pool entry
 *===========================================================================*/

sf_result_t sfi_flver_node_read(sf_binary_reader_t *br, bool unicode,
                                sf_flver_node_t *out, const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    memset(out, 0, sizeof(*out));
    /* Match upstream defaults set in Node() ctor for empty index links. */
    out->parent_index           = -1;
    out->first_child_index      = -1;
    out->next_sibling_index     = -1;
    out->previous_sibling_index = -1;

    sf_result_t e;
    int32_t name_offset = 0;
    int32_t flags_raw   = 0;

    if ((e = sf_binary_reader_read_vec3(br, &out->translation))             != SF_OK) return e;
    if ((e = sf_binary_reader_read_i32 (br, &name_offset))                  != SF_OK) return e;
    if ((e = sf_binary_reader_read_vec3(br, &out->rotation))                != SF_OK) return e;
    if ((e = sf_binary_reader_read_i16 (br, &out->parent_index))            != SF_OK) return e;
    if ((e = sf_binary_reader_read_i16 (br, &out->first_child_index))       != SF_OK) return e;
    if ((e = sf_binary_reader_read_vec3(br, &out->scale))                   != SF_OK) return e;
    if ((e = sf_binary_reader_read_i16 (br, &out->next_sibling_index))      != SF_OK) return e;
    if ((e = sf_binary_reader_read_i16 (br, &out->previous_sibling_index))  != SF_OK) return e;
    if ((e = sf_binary_reader_read_vec3(br, &out->bbox_min))                != SF_OK) return e;
    if ((e = sf_binary_reader_read_i32 (br, &flags_raw))                    != SF_OK) return e;
    if ((e = sf_binary_reader_read_vec3(br, &out->bbox_max))                != SF_OK) return e;
    if ((e = sf_binary_reader_assert_pattern(br, 0x34, 0x00))               != SF_OK) return e;

    out->flags = (sf_flver_node_flags_t)flags_raw;

    /* Resolve name via Get* (no cursor movement). */
    if (unicode) {
        e = sf_binary_reader_get_utf16(br, (int64_t)name_offset, &out->name, NULL);
    } else {
        e = sf_binary_reader_get_shift_jis(br, (int64_t)name_offset, &out->name, NULL);
    }
    if (e != SF_OK) {
        sf_xfree(a, out->name);
        out->name = NULL;
        return e;
    }
    return SF_OK;
}

/* Writes everything in the 128-byte node record EXCEPT the leading
 * translation (12B) and name_offset (4B); the caller has already emitted
 * those two fields via their preferred mechanism (reservation or literal). */
static sf_result_t flver_node_write_body(sf_binary_writer_t *bw,
                                         const sf_flver_node_t *n) {
    sf_result_t e;
    if ((e = sf_binary_writer_write_vec3(bw, n->rotation))                  != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16 (bw, n->parent_index))              != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16 (bw, n->first_child_index))         != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3(bw, n->scale))                     != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16 (bw, n->next_sibling_index))        != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16 (bw, n->previous_sibling_index))    != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3(bw, n->bbox_min))                  != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32 (bw, (int32_t)n->flags))            != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3(bw, n->bbox_max))                  != SF_OK) return e;
    if ((e = sf_binary_writer_write_pattern(bw, 0x34, 0x00))                != SF_OK) return e;
    return SF_OK;
}

sf_result_t sfi_flver_node_write(sf_binary_writer_t *bw, const sf_flver_node_t *n,
                                 size_t index) {
    SF_CHECK_ARG(bw != NULL && n != NULL);

    sf_result_t e;
    char name_buf[40];
    int written = snprintf(name_buf, sizeof(name_buf), "BoneNameOffset%u",
                           (unsigned int)index);
    if (written < 0 || (size_t)written >= sizeof(name_buf)) return SF_ERR_INTERNAL;

    if ((e = sf_binary_writer_write_vec3 (bw, n->translation))              != SF_OK) return e;
    if ((e = sf_binary_writer_reserve_i32(bw, name_buf))                    != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3 (bw, n->rotation))                 != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16  (bw, n->parent_index))             != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16  (bw, n->first_child_index))        != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3 (bw, n->scale))                    != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16  (bw, n->next_sibling_index))       != SF_OK) return e;
    if ((e = sf_binary_writer_write_i16  (bw, n->previous_sibling_index))   != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3 (bw, n->bbox_min))                 != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32  (bw, (int32_t)n->flags))           != SF_OK) return e;
    if ((e = sf_binary_writer_write_vec3 (bw, n->bbox_max))                 != SF_OK) return e;
    if ((e = sf_binary_writer_write_pattern(bw, 0x34, 0x00))                != SF_OK) return e;
    return SF_OK;
}

sf_result_t sfi_flver_node_write_with_offset(sf_binary_writer_t *bw,
                                              const sf_flver_node_t *n,
                                              int32_t name_offset) {
    SF_CHECK_ARG(bw != NULL && n != NULL);

    sf_result_t e;
    if ((e = sf_binary_writer_write_vec3(bw, n->translation))               != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32 (bw, name_offset))                  != SF_OK) return e;
    return flver_node_write_body(bw, n);
}

void sfi_flver_node_destroy_inplace(sf_flver_node_t *n, const sf_allocator_t *a) {
    if (!n) return;
    sf_xfree(a, n->name);
    n->name = NULL;
}

/*===========================================================================
 * LayoutMember — 20-byte record (non-SpeedTree) or 16-byte (SpeedTree)
 *===========================================================================*/

sf_result_t sfi_flver_layout_member_read(sf_binary_reader_t *br,
                                          int32_t struct_offset, bool is_speedtree,
                                          sfi_flver_layout_member_t *out) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t e;
    uint32_t type_raw     = 0;
    uint32_t semantic_raw = 0;

    if (is_speedtree) {
        int16_t stream_s16 = 0;
        int32_t local_offset = 0;
        if ((e = sf_binary_reader_read_i16(br, &stream_s16))                != SF_OK) return e;
        if ((e = sf_binary_reader_read_i16(br, &out->special_modifier))     != SF_OK) return e;
        if ((e = sf_binary_reader_read_i32(br, &local_offset))              != SF_OK) return e;
        out->stream = (int32_t)stream_s16;
        (void)local_offset;
    } else {
        if ((e = sf_binary_reader_read_i32 (br, &out->stream))              != SF_OK) return e;
        if ((e = sf_binary_reader_assert_i32_one(br, struct_offset))        != SF_OK) return e;
    }

    if ((e = sf_binary_reader_read_u32(br, &type_raw))                      != SF_OK) return e;
    if ((e = sf_binary_reader_read_u32(br, &semantic_raw))                  != SF_OK) return e;
    if ((e = sf_binary_reader_read_i32(br, &out->index))                    != SF_OK) return e;

    out->type     = (sf_flver_layout_type_t)type_raw;
    out->semantic = (sf_flver_layout_semantic_t)semantic_raw;
    return SF_OK;
}

sf_result_t sfi_flver_layout_member_write(sf_binary_writer_t *bw,
                                           int32_t struct_offset, bool is_speedtree,
                                           const sfi_flver_layout_member_t *m) {
    SF_CHECK_ARG(bw != NULL && m != NULL);

    sf_result_t e;
    if (is_speedtree) {
        if ((e = sf_binary_writer_write_i16(bw, (int16_t)m->stream))        != SF_OK) return e;
        if ((e = sf_binary_writer_write_i16(bw, m->special_modifier))       != SF_OK) return e;
    } else {
        if ((e = sf_binary_writer_write_i32(bw, m->stream))                 != SF_OK) return e;
    }
    if ((e = sf_binary_writer_write_i32(bw, struct_offset))                 != SF_OK) return e;
    if ((e = sf_binary_writer_write_u32(bw, (uint32_t)m->type))             != SF_OK) return e;
    if ((e = sf_binary_writer_write_u32(bw, (uint32_t)m->semantic))         != SF_OK) return e;
    if ((e = sf_binary_writer_write_i32(bw, m->index))                      != SF_OK) return e;
    return SF_OK;
}
