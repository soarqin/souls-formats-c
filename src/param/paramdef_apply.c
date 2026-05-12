/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMDEF application and PARAM cell materialization.
 *
 * Bitstream helpers — literal mirror of Row.cs:236-244.
 * DO NOT "beautify" the shift math. The (64 - bitSize - bitOffset) pattern
 * is intentional and must match upstream exactly for round-trip correctness.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/PARAM/PARAM/PARAM.cs (lines 309-356)
 *   SoulsFormats/Formats/PARAM/PARAM/Row.cs   (lines 118-281, 305-432)
 *   SoulsFormats/Formats/PARAM/PARAM/Cell.cs  (lines 24-56)
 *   SoulsFormats/Formats/PARAM/ParamUtil.cs   (lines 239-292)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_encoding.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"

_Static_assert(sizeof(uint64_t) == 8, "bitstream helpers require 64-bit uint64_t");

/*===========================================================================
 * Bitstream primitives — literal mirror of Row.cs:236-244.
 *
 * All helpers operate on a byte buffer with a *global* bit offset measured
 * from the start of the buffer. Internally they decompose the global offset
 * into a byte index (offset / 8) plus a *local* bit offset (offset % 8) —
 * the latter is what feeds into the upstream `(64 - bitSize - bitOffset)`
 * shift formula. Buffer access is unaligned-safe via memcpy.
 *===========================================================================*/

/*  Read `bit_size` bits starting at `bit_offset` (zero-indexed, little-endian
 *  bit order) from `buf` and return them as a zero-extended uint64_t.
 *
 *  Mirrors Row.cs:244 (unsigned branch):
 *      shifted = (long)(bitValue << leftShift >> rightShift);
 *  with leftShift = 64 - bitSize - bitOffset, rightShift = 64 - bitSize.
 *
 *  Preconditions (caller-enforced, matches upstream — no in-helper checks):
 *      - 1 <= bit_size <= 64 - (bit_offset % 8)   (i.e. bits fit in 64-bit window)
 *      - buf has at least 8 readable bytes from buf + (bit_offset / 8). */
static uint64_t extract_bits_unsigned(const uint8_t *buf, size_t bit_offset,
                                      size_t bit_size) {
    /*  Load the 8-byte little-endian window covering the requested bits. */
    uint64_t bit_value;
    memcpy(&bit_value, buf + bit_offset / 8, sizeof(uint64_t));
    /*  Row.cs:236-244:
     *      leftShift  = 64 - bitSize - bitOffset
     *      rightShift = 64 - bitSize
     *      result     = (bitValue << leftShift) >> rightShift          */
    size_t local_bit_offset = bit_offset % 8;
    return (bit_value << (64 - bit_size - local_bit_offset)) >> (64 - bit_size);
}

/*  Same as extract_bits_unsigned but sign-extends the high bit of the
 *  bit_size-wide field into the upper bits of the returned int64_t.
 *
 *  Equivalent to Row.cs:241 (signed branch):
 *      shifted = (long)bitValue << leftShift >> rightShift;
 *  but uses an explicit sign-extension mask to avoid relying on the
 *  implementation-defined behaviour of right-shifting negative signed
 *  integers in C.                                                          */
static int64_t extract_bits_signed(const uint8_t *buf, size_t bit_offset,
                                   size_t bit_size) {
    uint64_t u = extract_bits_unsigned(buf, bit_offset, bit_size);
    /*  Sign-extend: if the high bit of the bit_size-wide field is set,
     *  fill the upper (64 - bit_size) bits with 1s. */
    if (bit_size < 64 && ((u >> (bit_size - 1)) & 1u)) {
        u |= ~(uint64_t)0 << bit_size;
    }
    return (int64_t)u;
}

/*  Insert `bit_size` low bits of `value` at `bit_offset` into `buf`,
 *  preserving the surrounding bits via read-modify-write.
 *
 *  Mirrors the shift-and-OR pattern from Row.cs:396-397:
 *      shifted = shifted << (BIT_VALUE_SIZE - field.BitSize)
 *                       >> (BIT_VALUE_SIZE - field.BitSize - bitOffset);
 *      bitValue |= shifted;
 *  reformulated as a bounded mask + insert so callers can update individual
 *  fields while queuing full bit groups before flushing, exactly like
 *  upstream WriteCells. */
static void insert_bits(uint8_t *buf, size_t bit_offset, size_t bit_size,
                        uint64_t value) {
    uint64_t existing;
    memcpy(&existing, buf + bit_offset / 8, sizeof(uint64_t));
    size_t local_bit_offset = bit_offset % 8;
    uint64_t mask = (bit_size < 64) ? (((uint64_t)1 << bit_size) - 1u)
                                    : ~(uint64_t)0;
    mask <<= local_bit_offset;
    existing = (existing & ~mask) | ((value << local_bit_offset) & mask);
    memcpy(buf + bit_offset / 8, &existing, sizeof(uint64_t));
}

/*  Detect orphaned bits left over from a partially consumed bit window —
 *  literal mirror of Row.cs:136-140 checkOrphanedBits():
 *      if (bitOffset != -1 && (bitValue >> bitOffset) != 0)
 *          throw ...;
 *  Returns true when the high (window_bits - bit_offset) bits of `bit_value`
 *  are non-zero, i.e. when the upstream check would throw.
 *
 *  `bit_offset` here is the LOCAL offset within the current bit window
 *  (0..bit_limit). At bit_offset >= 64 the right-shift would be UB; upstream
 *  guards bit_value's width to 64 bits via the ulong cast, so we mirror that
 *  by returning false (no further bits to inspect).                         */
static bool detect_orphaned_bits(uint64_t bit_value, size_t bit_offset) {
    return (bit_offset < 64) ? ((bit_value >> bit_offset) != 0) : false;
}

#ifndef SF_PARAMDEF_APPLY_BITSTREAM_ONLY

#include "param/param_internal.h"
#include "param/paramdef_internal.h"

#include "internal/sf_internal.h"

enum { BIT_VALUE_SIZE = 64 };

static uint16_t load_u16(const uint8_t *p, bool big_endian) {
    if (big_endian) return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
    return (uint16_t)(((uint16_t)p[1] << 8) | p[0]);
}

static uint32_t load_u32(const uint8_t *p, bool big_endian) {
    if (big_endian) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | p[3];
    }
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) | p[0];
}

static uint64_t load_u64(const uint8_t *p, bool big_endian) {
    uint64_t v = 0;
    if (big_endian) {
        for (size_t i = 0; i < 8; i++) v = (v << 8) | p[i];
    } else {
        for (size_t i = 0; i < 8; i++) v |= (uint64_t)p[i] << (i * 8);
    }
    return v;
}

static void store_u16(uint8_t *p, uint16_t v, bool big_endian) {
    if (big_endian) {
        p[0] = (uint8_t)(v >> 8);
        p[1] = (uint8_t)v;
    } else {
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
    }
}

static void store_u32(uint8_t *p, uint32_t v, bool big_endian) {
    if (big_endian) {
        p[0] = (uint8_t)(v >> 24);
        p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);
        p[3] = (uint8_t)v;
    } else {
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
        p[2] = (uint8_t)(v >> 16);
        p[3] = (uint8_t)(v >> 24);
    }
}

static void store_u64(uint8_t *p, uint64_t v, bool big_endian) {
    if (big_endian) {
        for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)(v >> ((7 - i) * 8));
    } else {
        for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (i * 8));
    }
}

static uint64_t load_bit_window(const uint8_t *p, size_t byte_count, bool big_endian) {
    switch (byte_count) {
    case 1: return p[0];
    case 2: return load_u16(p, big_endian);
    case 4: return load_u32(p, big_endian);
    default: return 0;
    }
}

static void store_bit_window(uint8_t *p, size_t byte_count, uint64_t value,
                             bool big_endian) {
    switch (byte_count) {
    case 1:
        p[0] = (uint8_t)value;
        break;
    case 2:
        store_u16(p, (uint16_t)value, big_endian);
        break;
    case 4:
        store_u32(p, (uint32_t)value, big_endian);
        break;
    default:
        break;
    }
}

static void store_u64_le(uint8_t p[8], uint64_t v) {
    for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (i * 8));
}

static uint64_t load_u64_le(const uint8_t p[8]) {
    uint64_t v = 0;
    for (size_t i = 0; i < 8; i++) v |= (uint64_t)p[i] << (i * 8);
    return v;
}

static bool is_signed_bit_type(sf_paramdef_def_type_t type) {
    return type == SF_PARAMDEF_DEF_TYPE_S8 || type == SF_PARAMDEF_DEF_TYPE_S16 ||
           type == SF_PARAMDEF_DEF_TYPE_S32;
}

static sf_param_cell_kind_t cell_kind_for_field(sf_paramdef_def_type_t type,
                                                int32_t bit_size,
                                                int32_t array_length) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8: return SF_PARAM_CELL_KIND_S8;
    case SF_PARAMDEF_DEF_TYPE_U8:
        return (bit_size == -1 && array_length > 1) ? SF_PARAM_CELL_KIND_U8_ARRAY
                                                    : SF_PARAM_CELL_KIND_U8;
    case SF_PARAMDEF_DEF_TYPE_S16: return SF_PARAM_CELL_KIND_S16;
    case SF_PARAMDEF_DEF_TYPE_U16: return SF_PARAM_CELL_KIND_U16;
    case SF_PARAMDEF_DEF_TYPE_S32: return SF_PARAM_CELL_KIND_S32;
    case SF_PARAMDEF_DEF_TYPE_U32: return SF_PARAM_CELL_KIND_U32;
    case SF_PARAMDEF_DEF_TYPE_B32: return SF_PARAM_CELL_KIND_B32;
    case SF_PARAMDEF_DEF_TYPE_F32: return SF_PARAM_CELL_KIND_F32;
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: return SF_PARAM_CELL_KIND_ANGLE32;
    case SF_PARAMDEF_DEF_TYPE_F64: return SF_PARAM_CELL_KIND_F64;
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
        return bit_size == -1 ? SF_PARAM_CELL_KIND_DUMMY8_ARRAY
                              : SF_PARAM_CELL_KIND_DUMMY8_BIT;
    case SF_PARAMDEF_DEF_TYPE_FIXSTR: return SF_PARAM_CELL_KIND_FIXSTR;
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W: return SF_PARAM_CELL_KIND_FIXSTR_W;
    default: return SF_PARAM_CELL_KIND_U8;
    }
}

static size_t field_byte_count(sf_paramdef_def_type_t type, int32_t byte_count,
                               int32_t array_length) {
    if (byte_count > 0) return (size_t)byte_count;
    if (array_length <= 0) return 0;
    size_t value_size = sf_param_util_get_value_size(type);
    return value_size * (size_t)array_length;
}

static bool field_valid_for_unversioned_apply(const sf_paramdef_t *def,
                                              const sf_paramdef_field_t *field) {
    if (!sf_paramdef_is_version_aware(def)) return true;

    /* ApplyParamdef passes ulong.MaxValue to Row.ReadCells. Per
     * Field.IsValidForRegulationVersion, a removed field is therefore invalid
     * for this unversioned mode; first-version bounds cannot exceed MaxValue. */
    return sf_paramdef_field_get_removed_regulation_version(field) == 0;
}

static sf_result_t init_cell_from_layout(sf_param_cell_t *cell,
                                         const sf_paramdef_field_layout_entry_t *entry,
                                         const sf_allocator_t *alloc) {
    memset(cell, 0, sizeof(*cell));
    const char *name = entry->field->internal_name ? entry->field->internal_name : "";
    cell->internal_name = (char *)name;
    cell->owns_internal_name = false;

    cell->display_type = entry->display_type;
    cell->bit_size = entry->declared_bit_size;
    cell->array_length = entry->array_length;
    cell->byte_count = entry->declared_byte_count;
    cell->value.kind = entry->cell_kind;
    return SF_OK;
}

static sf_result_t set_cell_bytes(sf_param_cell_t *cell, const uint8_t *data,
                                  size_t size, const sf_allocator_t *alloc) {
    uint8_t *copy = NULL;
    if (size > 0) {
        copy = (uint8_t *)sf_xalloc(alloc, size);
        if (!copy) return SF_ERR_OOM;
        memcpy(copy, data, size);
    }
    cell->value.v.bytes.data = copy;
    cell->value.v.bytes.size = size;
    return SF_OK;
}

static sf_result_t set_cell_fixstr(sf_param_cell_t *cell, const uint8_t *data,
                                   size_t size, const sf_allocator_t *alloc) {
    size_t term = 0;
    while (term < size && data[term] != 0) term++;
    char *s = NULL;
    sf_result_t r = sf_shift_jis_to_utf8(data, term, &s, NULL, alloc);
    if (r != SF_OK) return r;
    cell->value.v.str_utf8 = s;
    return SF_OK;
}

static sf_result_t set_cell_fixstr_w(sf_param_cell_t *cell, const uint8_t *data,
                                     size_t size, bool big_endian,
                                     const sf_allocator_t *alloc) {
    size_t term = 0;
    while (term + 1 < size) {
        if (data[term] == 0 && data[term + 1] == 0) break;
        term += 2;
    }
    char *s = NULL;
    sf_result_t r = big_endian
        ? sf_utf16be_to_utf8(data, term, &s, NULL, alloc)
        : sf_utf16le_to_utf8(data, term, &s, NULL, alloc);
    if (r != SF_OK) return r;
    cell->value.v.str_utf8 = s;
    return SF_OK;
}

static sf_result_t assign_integral_cell(sf_param_cell_t *cell, int64_t signed_value,
                                        uint64_t unsigned_value) {
    switch (cell->display_type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
        cell->value.v.s8 = (int8_t)signed_value;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_U8:
        cell->value.v.u8 = (uint8_t)unsigned_value;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_S16:
        cell->value.v.s16 = (int16_t)signed_value;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_U16:
        cell->value.v.u16 = (uint16_t)unsigned_value;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_S32:
        cell->value.v.s32 = (int32_t)signed_value;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_U32:
        cell->value.v.u32 = (uint32_t)unsigned_value;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
        cell->value.v.u8 = (uint8_t)unsigned_value;
        return SF_OK;
    default:
        return SF_ERR_INTERNAL;
    }
}

static sf_result_t check_range(size_t offset, size_t need, size_t size) {
    if (need > size || offset > size - need) return SF_ERR_TRUNCATED;
    return SF_OK;
}

static sf_result_t layout_flush_bits(sf_paramdef_field_layout_entry_t *entries,
                                     size_t last_bit_index, bool *bits_active,
                                     size_t *offset, size_t bit_limit) {
    if (!*bits_active) return SF_OK;
    if (last_bit_index == SIZE_MAX) return SF_ERR_INTERNAL;
    entries[last_bit_index].check_orphaned_bits_after = true;
    size_t byte_count = bit_limit / 8;
    if (*offset > SIZE_MAX - byte_count) return SF_ERR_OUT_OF_RANGE;
    *offset += byte_count;
    *bits_active = false;
    return SF_OK;
}

static sf_result_t build_field_layout(const sf_paramdef_t *def,
                                      sf_paramdef_field_layout_t **out) {
    SF_CHECK_ARG(def != NULL && out != NULL);
    *out = NULL;

    const sf_allocator_t *alloc = def->alloc;
    size_t field_count = def->field_count;
    sf_paramdef_field_layout_t *layout =
        (sf_paramdef_field_layout_t *)sf_xalloc(alloc, sizeof(*layout));
    if (!layout) return SF_ERR_OOM;
    memset(layout, 0, sizeof(*layout));

    if (field_count > 0) {
        if (field_count > SIZE_MAX / sizeof(*layout->entries)) {
            sf_xfree(alloc, layout);
            return SF_ERR_OUT_OF_RANGE;
        }
        layout->entries = (sf_paramdef_field_layout_entry_t *)sf_xalloc(
            alloc, field_count * sizeof(*layout->entries));
        if (!layout->entries) {
            sf_xfree(alloc, layout);
            return SF_ERR_OOM;
        }
        memset(layout->entries, 0, field_count * sizeof(*layout->entries));
    }

    size_t offset = 0;
    bool bits_active = false;
    size_t bit_offset = 0;
    size_t bit_limit = 0;
    size_t last_bit_index = SIZE_MAX;
    sf_result_t fail_result = SF_OK;

    for (size_t i = 0; i < field_count; i++) {
        const sf_paramdef_field_t *field = &def->fields[i];
        if (!field_valid_for_unversioned_apply(def, field)) continue;

        sf_paramdef_def_type_t type = field->display_type;
        int32_t bit_size_i32 = field->bit_size;
        bool is_bit = sf_param_util_is_bit_type(type) && bit_size_i32 != -1;

        if (is_bit) {
            if (bit_size_i32 <= 0) goto bad_magic;
            size_t field_bit_size = (size_t)bit_size_i32;
            size_t field_bit_limit = (size_t)sf_param_util_get_bit_limit(type);
            if (field_bit_limit == 0 || field_bit_size > field_bit_limit) goto bad_magic;

            if (!bits_active || bit_limit != field_bit_limit ||
                bit_offset + field_bit_size > bit_limit) {
                sf_result_t r = layout_flush_bits(layout->entries, last_bit_index,
                                                  &bits_active, &offset, bit_limit);
                if (r != SF_OK) { fail_result = r; goto fail; }
                bits_active = true;
                bit_offset = 0;
                bit_limit = field_bit_limit;
                last_bit_index = SIZE_MAX;
            }

            sf_paramdef_field_layout_entry_t *entry =
                &layout->entries[layout->entry_count];
            entry->field = field;
            entry->byte_offset = offset;
            entry->bit_offset = bit_offset;
            entry->bit_size = field_bit_size;
            entry->bit_limit = bit_limit;
            entry->display_type = type;
            entry->cell_kind = cell_kind_for_field(type, bit_size_i32, field->array_length);
            entry->array_length = field->array_length;
            entry->declared_byte_count = field->byte_count;
            entry->declared_bit_size = bit_size_i32;
            entry->is_bit_field = true;
            last_bit_index = layout->entry_count;
            layout->entry_count++;
            bit_offset += field_bit_size;
        } else {
            sf_result_t r = layout_flush_bits(layout->entries, last_bit_index,
                                              &bits_active, &offset, bit_limit);
            if (r != SF_OK) { fail_result = r; goto fail; }

            size_t byte_count = field_byte_count(type, field->byte_count,
                                                 field->array_length);
            if (byte_count == 0) goto bad_magic;
            if (offset > SIZE_MAX - byte_count) goto out_of_range;

            sf_paramdef_field_layout_entry_t *entry =
                &layout->entries[layout->entry_count];
            entry->field = field;
            entry->byte_offset = offset;
            entry->byte_count = byte_count;
            entry->display_type = type;
            entry->cell_kind = cell_kind_for_field(type, bit_size_i32, field->array_length);
            entry->array_length = field->array_length;
            entry->declared_byte_count = field->byte_count;
            entry->declared_bit_size = bit_size_i32;
            layout->entry_count++;
            offset += byte_count;
        }
    }

    {
        sf_result_t r = layout_flush_bits(layout->entries, last_bit_index,
                                          &bits_active, &offset, bit_limit);
        if (r != SF_OK) { fail_result = r; goto fail; }
    }

    layout->row_data_size = def->row_size > 0 ? (size_t)def->row_size : offset;
    *out = layout;
    return SF_OK;

bad_magic:
    fail_result = SF_ERR_BAD_MAGIC;
    goto fail;
out_of_range:
    fail_result = SF_ERR_OUT_OF_RANGE;
fail:
    sf_xfree(alloc, layout->entries);
    sf_xfree(alloc, layout);
    return fail_result;
}

static sf_result_t get_field_layout(const sf_paramdef_t *def,
                                    const sf_paramdef_field_layout_t **out) {
    SF_CHECK_ARG(def != NULL && out != NULL);
    sf_paramdef_t *mutable_def = (sf_paramdef_t *)def;
    if (!mutable_def->layout_cache) {
        sf_paramdef_field_layout_t *layout = NULL;
        sf_result_t r = build_field_layout(def, &layout);
        if (r != SF_OK) return r;
        mutable_def->layout_cache = layout;
    }
    *out = mutable_def->layout_cache;
    return SF_OK;
}

typedef struct bit_read_state {
    bool active;
    size_t byte_offset;
    size_t bit_offset;
    size_t bit_limit;
    uint64_t bit_value;
    uint8_t window[8];
} bit_read_state_t;

static sf_result_t start_read_bits(bit_read_state_t *state, const uint8_t *data,
                                   size_t data_size, size_t offset,
                                   size_t bit_limit, bool big_endian) {
    size_t byte_count = bit_limit / 8;
    sf_result_t r = check_range(offset, byte_count, data_size);
    if (r != SF_OK) return r;

    memset(state->window, 0, sizeof(state->window));
    state->bit_value = load_bit_window(data + offset, byte_count, big_endian);
    store_u64_le(state->window, state->bit_value);
    state->byte_offset = offset;
    state->bit_offset = 0;
    state->bit_limit = bit_limit;
    state->active = true;
    return SF_OK;
}

static sf_result_t read_layout_bit_cell(sf_param_cell_t *cell,
                                        const sf_paramdef_field_layout_entry_t *entry,
                                        const uint8_t *data, size_t data_size,
                                        bool big_endian, bit_read_state_t *bits) {
    if (!bits->active || bits->byte_offset != entry->byte_offset ||
        bits->bit_limit != entry->bit_limit) {
        sf_result_t r = start_read_bits(bits, data, data_size, entry->byte_offset,
                                        entry->bit_limit, big_endian);
        if (r != SF_OK) return r;
    }

    uint64_t raw = 0;
    int64_t signed_raw = 0;
    if (is_signed_bit_type(entry->display_type)) {
        signed_raw = extract_bits_signed(bits->window, entry->bit_offset, entry->bit_size);
        raw = (uint64_t)signed_raw;
    } else {
        raw = extract_bits_unsigned(bits->window, entry->bit_offset, entry->bit_size);
        signed_raw = (int64_t)raw;
    }

    sf_result_t r = assign_integral_cell(cell, signed_raw, raw);
    if (r != SF_OK) return r;
    if (entry->check_orphaned_bits_after &&
        detect_orphaned_bits(bits->bit_value, entry->bit_offset + entry->bit_size)) {
        return SF_ERR_BAD_MAGIC;
    }
    return SF_OK;
}

static sf_result_t read_nonbit_cell(sf_param_cell_t *cell, const uint8_t *data,
                                    size_t data_size, size_t *offset,
                                    size_t count, bool big_endian,
                                    const sf_allocator_t *alloc) {
    if (count == 0) return SF_ERR_BAD_MAGIC;
    sf_result_t r = check_range(*offset, count, data_size);
    if (r != SF_OK) return r;
    const uint8_t *p = data + *offset;

    switch (cell->display_type) {
    case SF_PARAMDEF_DEF_TYPE_B32:
        cell->value.v.b32 = load_u32(p, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: {
        uint32_t raw = load_u32(p, big_endian);
        float f = 0.0f;
        memcpy(&f, &raw, sizeof(f));
        if (cell->display_type == SF_PARAMDEF_DEF_TYPE_F32) cell->value.v.f32 = f;
        else cell->value.v.angle32 = f;
        break;
    }
    case SF_PARAMDEF_DEF_TYPE_F64: {
        uint64_t raw = load_u64(p, big_endian);
        double d = 0.0;
        memcpy(&d, &raw, sizeof(d));
        cell->value.v.f64 = d;
        break;
    }
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
        r = set_cell_fixstr(cell, p, count, alloc);
        if (r != SF_OK) return r;
        break;
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        r = set_cell_fixstr_w(cell, p, count, big_endian, alloc);
        if (r != SF_OK) return r;
        break;
    case SF_PARAMDEF_DEF_TYPE_S8:
        cell->value.v.s8 = (int8_t)p[0];
        break;
    case SF_PARAMDEF_DEF_TYPE_U8:
        if (cell->array_length > 1) {
            r = set_cell_bytes(cell, p, count, alloc);
            if (r != SF_OK) return r;
        } else {
            cell->value.v.u8 = p[0];
        }
        break;
    case SF_PARAMDEF_DEF_TYPE_S16:
        cell->value.v.s16 = (int16_t)load_u16(p, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_U16:
        cell->value.v.u16 = load_u16(p, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_S32:
        cell->value.v.s32 = (int32_t)load_u32(p, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_U32:
        cell->value.v.u32 = load_u32(p, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
        r = set_cell_bytes(cell, p, count, alloc);
        if (r != SF_OK) return r;
        break;
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }

    *offset += count;
    return SF_OK;
}

static sf_result_t populate_row_cells(sf_param_row_t *row, const sf_param_t *param,
                                      const sf_paramdef_t *def) {
    if (row->data_offset == 0) return SF_OK;
    if (row->data_size > 0 && !row->data) return SF_ERR_INVALID_ARG;

    const sf_paramdef_field_layout_t *layout = NULL;
    sf_result_t r = get_field_layout(def, &layout);
    if (r != SF_OK) return r;

    sfi_param_row_clear_cells(row, param->alloc);

    size_t field_count = layout->entry_count;
    sf_param_cell_t *cells = NULL;
    if (field_count > 0) {
        cells = (sf_param_cell_t *)sf_xalloc(param->alloc, field_count * sizeof(*cells));
        if (!cells) return SF_ERR_OOM;
        memset(cells, 0, field_count * sizeof(*cells));
    }

    size_t cell_count = 0;
    bit_read_state_t bits;
    memset(&bits, 0, sizeof(bits));

    for (size_t i = 0; i < layout->entry_count; i++) {
        const sf_paramdef_field_layout_entry_t *entry = &layout->entries[i];
        sf_param_cell_t *cell = &cells[cell_count];
        r = init_cell_from_layout(cell, entry, param->alloc);
        if (r != SF_OK) goto fail;

        if (entry->is_bit_field) {
            r = read_layout_bit_cell(cell, entry, row->data, row->data_size,
                                     param->big_endian, &bits);
        } else {
            size_t offset = entry->byte_offset;
            r = read_nonbit_cell(cell, row->data, row->data_size, &offset,
                                 entry->byte_count, param->big_endian,
                                 param->alloc);
        }
        if (r != SF_OK) goto fail;
        cell_count++;
    }

    row->cells = cells;
    row->cell_count = cell_count;
    row->cell_data_size = layout->row_data_size;
    row->cells_applied = true;
    return SF_OK;

fail:
    if (cells) {
        row->cells = cells;
        row->cell_count = field_count;
        sfi_param_row_clear_cells(row, param->alloc);
    }
    return r;
}

static bool param_type_matches(const sf_param_t *param, const sf_paramdef_t *def) {
    return strcmp(sf_param_get_param_type(param), sf_paramdef_get_param_type(def)) == 0;
}

static bool param_type_matches_or_empty(const sf_param_t *param, const sf_paramdef_t *def) {
    const char *type = sf_param_get_param_type(param);
    return type[0] == '\0' || strcmp(type, sf_paramdef_get_param_type(def)) == 0;
}

static bool data_version_matches(const sf_param_t *param, const sf_paramdef_t *def) {
    return param->paramdef_data_version == sf_paramdef_get_data_version(def);
}

static bool data_version_matches_or_headerless(const sf_param_t *param,
                                               const sf_paramdef_t *def) {
    return param->headerless_rows || data_version_matches(param, def);
}

static sf_result_t populate_all_cells(sf_param_t *param, const sf_paramdef_t *def) {
    for (size_t i = 0; i < param->row_count; i++) {
        sf_result_t r = populate_row_cells(&param->rows[i], param, def);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

sf_result_t sf_param_apply_paramdef(sf_param_t *param, const sf_paramdef_t *paramdef,
                                    sf_param_apply_mode_t mode) {
    SF_CHECK_ARG(param != NULL && paramdef != NULL);

    switch (mode) {
    case SF_PARAM_APPLY_UNCONDITIONAL:
        break;
    case SF_PARAM_APPLY_SOMEWHAT_CAREFUL:
        if (!param_type_matches_or_empty(param, paramdef)) return SF_ERR_NOT_FOUND;
        if (!data_version_matches_or_headerless(param, paramdef)) return SF_ERR_NOT_FOUND;
        break;
    case SF_PARAM_APPLY_CAREFUL:
        if (!param_type_matches(param, paramdef)) return SF_ERR_NOT_FOUND;
        if (!data_version_matches(param, paramdef)) return SF_ERR_NOT_FOUND;
        if (param->detected_size != -1 &&
            param->detected_size != (int64_t)sf_paramdef_get_row_size(paramdef)) {
            return SF_ERR_NOT_FOUND;
        }
        break;
    default:
        return SF_ERR_INVALID_ARG;
    }

    return populate_all_cells(param, paramdef);
}

sf_result_t sf_param_apply_paramdef_multi(sf_param_t *param,
                                          const sf_paramdef_t *const *paramdefs,
                                          size_t paramdef_count,
                                          sf_param_apply_mode_t mode) {
    SF_CHECK_ARG(param != NULL && (paramdef_count == 0 || paramdefs != NULL));

    for (size_t i = 0; i < paramdef_count; i++) {
        if (!paramdefs[i]) return SF_ERR_INVALID_ARG;
        sf_result_t r = sf_param_apply_paramdef(param, paramdefs[i], mode);
        if (r == SF_OK) return SF_OK;
        if (r != SF_ERR_NOT_FOUND) return r;
    }
    return SF_ERR_NOT_FOUND;
}

typedef struct bit_write_state {
    bool active;
    size_t bit_offset;
    size_t bit_limit;
    uint8_t window[8];
} bit_write_state_t;

static sf_result_t flush_write_bits(bit_write_state_t *bits, uint8_t *out,
                                    size_t out_size, size_t *offset,
                                    bool big_endian) {
    if (!bits->active) return SF_OK;
    size_t byte_count = bits->bit_limit / 8;
    sf_result_t r = check_range(*offset, byte_count, out_size);
    if (r != SF_OK) return r;
    store_bit_window(out + *offset, byte_count, load_u64_le(bits->window), big_endian);
    *offset += byte_count;
    memset(bits, 0, sizeof(*bits));
    return SF_OK;
}

static uint64_t cell_bit_value(const sf_param_cell_t *cell) {
    switch (cell->display_type) {
    case SF_PARAMDEF_DEF_TYPE_S8: return (uint8_t)cell->value.v.s8;
    case SF_PARAMDEF_DEF_TYPE_U8: return cell->value.v.u8;
    case SF_PARAMDEF_DEF_TYPE_S16: return (uint16_t)cell->value.v.s16;
    case SF_PARAMDEF_DEF_TYPE_U16: return cell->value.v.u16;
    case SF_PARAMDEF_DEF_TYPE_S32: return (uint32_t)cell->value.v.s32;
    case SF_PARAMDEF_DEF_TYPE_U32: return cell->value.v.u32;
    case SF_PARAMDEF_DEF_TYPE_DUMMY8: return cell->value.v.u8;
    default: return 0;
    }
}

static bool next_requires_bit_flush(const sf_param_row_t *row, size_t index,
                                    const bit_write_state_t *bits) {
    if (index + 1 >= row->cell_count) return true;
    const sf_param_cell_t *next = &row->cells[index + 1];
    if (!sf_param_util_is_bit_type(next->display_type) || next->bit_size == -1) return true;
    int next_limit = sf_param_util_get_bit_limit(next->display_type);
    if (next_limit <= 0 || (size_t)next_limit != bits->bit_limit) return true;
    if (next->bit_size <= 0) return true;
    return bits->bit_offset + (size_t)next->bit_size > bits->bit_limit;
}

static sf_result_t write_bit_cell(const sf_param_row_t *row, size_t index,
                                  bit_write_state_t *bits, uint8_t *out,
                                  size_t out_size, size_t *offset,
                                  bool big_endian) {
    const sf_param_cell_t *cell = &row->cells[index];
    if (cell->bit_size <= 0) return SF_ERR_BAD_MAGIC;
    size_t bit_size = (size_t)cell->bit_size;
    size_t bit_limit = (size_t)sf_param_util_get_bit_limit(cell->display_type);
    if (bit_limit == 0 || bit_size > bit_limit) return SF_ERR_BAD_MAGIC;

    if (!bits->active) {
        bits->active = true;
        bits->bit_limit = bit_limit;
        bits->bit_offset = 0;
        memset(bits->window, 0, sizeof(bits->window));
    }
    if (bits->bit_limit != bit_limit || bits->bit_offset + bit_size > bits->bit_limit) {
        sf_result_t r = flush_write_bits(bits, out, out_size, offset, big_endian);
        if (r != SF_OK) return r;
        bits->active = true;
        bits->bit_limit = bit_limit;
        bits->bit_offset = 0;
        memset(bits->window, 0, sizeof(bits->window));
    }

    insert_bits(bits->window, bits->bit_offset, bit_size, cell_bit_value(cell));
    bits->bit_offset += bit_size;

    if (next_requires_bit_flush(row, index, bits)) {
        return flush_write_bits(bits, out, out_size, offset, big_endian);
    }
    return SF_OK;
}

static sf_result_t write_bytes_cell(const sf_param_cell_t *cell, uint8_t *out,
                                    size_t out_size, size_t *offset) {
    const uint8_t *data = cell->value.v.bytes.data;
    size_t size = cell->value.v.bytes.size;
    sf_result_t r = check_range(*offset, size, out_size);
    if (r != SF_OK) return r;
    if (size > 0 && data) memcpy(out + *offset, data, size);
    *offset += size;
    return SF_OK;
}

static sf_result_t write_fixstr_cell(const sf_param_cell_t *cell, uint8_t *out,
                                     size_t out_size, size_t *offset,
                                     const sf_allocator_t *alloc) {
    size_t size = field_byte_count(cell->display_type, cell->byte_count,
                                   cell->array_length);
    if (size == 0) return SF_ERR_BAD_MAGIC;
    sf_result_t r = check_range(*offset, size, out_size);
    if (r != SF_OK) return r;

    void *raw = NULL;
    size_t raw_size = 0;
    r = sf_utf8_to_shift_jis(cell->value.v.str_utf8 ? cell->value.v.str_utf8 : "",
                             true, &raw, &raw_size, alloc);
    if (r != SF_OK) return r;
    size_t to_copy = raw_size < size ? raw_size : size;
    if (to_copy > 0) memcpy(out + *offset, raw, to_copy);
    sf_xfree(alloc, raw);
    *offset += size;
    return SF_OK;
}

static sf_result_t write_fixstr_w_cell(const sf_param_cell_t *cell, uint8_t *out,
                                       size_t out_size, size_t *offset,
                                       bool big_endian,
                                       const sf_allocator_t *alloc) {
    size_t size = field_byte_count(cell->display_type, cell->byte_count,
                                   cell->array_length);
    if (size == 0) return SF_ERR_BAD_MAGIC;
    sf_result_t r = check_range(*offset, size, out_size);
    if (r != SF_OK) return r;

    void *raw = NULL;
    size_t raw_size = 0;
    r = big_endian
        ? sf_utf8_to_utf16be(cell->value.v.str_utf8 ? cell->value.v.str_utf8 : "",
                             true, &raw, &raw_size, alloc)
        : sf_utf8_to_utf16le(cell->value.v.str_utf8 ? cell->value.v.str_utf8 : "",
                             true, &raw, &raw_size, alloc);
    if (r != SF_OK) return r;
    size_t to_copy = raw_size < size ? raw_size : size;
    if (to_copy > 0) memcpy(out + *offset, raw, to_copy);
    sf_xfree(alloc, raw);
    *offset += size;
    return SF_OK;
}

static sf_result_t write_nonbit_cell(const sf_param_cell_t *cell, uint8_t *out,
                                     size_t out_size, size_t *offset,
                                     bool big_endian,
                                     const sf_allocator_t *alloc) {
    size_t size = field_byte_count(cell->display_type, cell->byte_count,
                                   cell->array_length);
    if (size == 0) return SF_ERR_BAD_MAGIC;
    sf_result_t r = check_range(*offset, size, out_size);
    if (r != SF_OK) return r;
    uint8_t *p = out + *offset;

    switch (cell->display_type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
        p[0] = (uint8_t)cell->value.v.s8;
        break;
    case SF_PARAMDEF_DEF_TYPE_U8:
        if (cell->value.kind == SF_PARAM_CELL_KIND_U8_ARRAY) return write_bytes_cell(cell, out, out_size, offset);
        p[0] = cell->value.v.u8;
        break;
    case SF_PARAMDEF_DEF_TYPE_S16:
        store_u16(p, (uint16_t)cell->value.v.s16, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_U16:
        store_u16(p, cell->value.v.u16, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_S32:
        store_u32(p, (uint32_t)cell->value.v.s32, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_U32:
        store_u32(p, cell->value.v.u32, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_B32:
        store_u32(p, cell->value.v.b32, big_endian);
        break;
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: {
        float f = cell->display_type == SF_PARAMDEF_DEF_TYPE_F32 ? cell->value.v.f32
                                                                 : cell->value.v.angle32;
        uint32_t raw = 0;
        memcpy(&raw, &f, sizeof(raw));
        store_u32(p, raw, big_endian);
        break;
    }
    case SF_PARAMDEF_DEF_TYPE_F64: {
        uint64_t raw = 0;
        memcpy(&raw, &cell->value.v.f64, sizeof(raw));
        store_u64(p, raw, big_endian);
        break;
    }
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
        return write_bytes_cell(cell, out, out_size, offset);
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
        return write_fixstr_cell(cell, out, out_size, offset, alloc);
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        return write_fixstr_w_cell(cell, out, out_size, offset, big_endian, alloc);
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }

    *offset += size;
    return SF_OK;
}

sf_result_t sfi_param_row_cells_to_bytes(const sf_param_t *param,
                                         const sf_param_row_t *row,
                                         uint8_t **out,
                                         size_t *out_size) {
    SF_CHECK_ARG(param != NULL && row != NULL && out != NULL && out_size != NULL);
    *out = NULL;
    *out_size = 0;

    size_t size = row->cell_data_size;
    if (size == 0) return SF_OK;
    uint8_t *bytes = (uint8_t *)sf_xalloc(param->alloc, size);
    if (!bytes) return SF_ERR_OOM;
    memset(bytes, 0, size);

    size_t offset = 0;
    bit_write_state_t bits;
    memset(&bits, 0, sizeof(bits));
    for (size_t i = 0; i < row->cell_count; i++) {
        const sf_param_cell_t *cell = &row->cells[i];
        sf_result_t r;
        if (sf_param_util_is_bit_type(cell->display_type) && cell->bit_size != -1) {
            r = write_bit_cell(row, i, &bits, bytes, size, &offset, param->big_endian);
        } else {
            r = flush_write_bits(&bits, bytes, size, &offset, param->big_endian);
            if (r == SF_OK) {
                r = write_nonbit_cell(cell, bytes, size, &offset, param->big_endian,
                                      param->alloc);
            }
        }
        if (r != SF_OK) {
            sf_xfree(param->alloc, bytes);
            return r;
        }
    }

    sf_result_t r = flush_write_bits(&bits, bytes, size, &offset, param->big_endian);
    if (r != SF_OK) {
        sf_xfree(param->alloc, bytes);
        return r;
    }

    *out = bytes;
    *out_size = size;
    return SF_OK;
}

#endif /* SF_PARAMDEF_APPLY_BITSTREAM_ONLY */
