/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAM / PARAMDEF public surface.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/PARAM/PARAM/PARAM.cs
 *   SoulsFormats/Formats/PARAM/PARAM/Row.cs
 *   SoulsFormats/Formats/PARAM/PARAM/Cell.cs
 */

#ifndef SF_PARAM_H
#define SF_PARAM_H

#include "sf_common.h"
#include "souls_formats/sf_io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_param sf_param_t;
typedef struct sf_param_row sf_param_row_t;
typedef struct sf_param_cell sf_param_cell_t;
typedef struct sf_paramdef sf_paramdef_t;

#if defined(__cplusplus)
#define SF_PARAM_STATIC_ASSERT static_assert
#else
#define SF_PARAM_STATIC_ASSERT _Static_assert
#endif

typedef uint8_t sf_param_format_flags1_t;
#define SF_PARAM_FORMAT_FLAGS1_NONE             ((sf_param_format_flags1_t)0x00)
#define SF_PARAM_FORMAT_FLAGS1_FLAG01           ((sf_param_format_flags1_t)0x01)
#define SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET  ((sf_param_format_flags1_t)0x02)
#define SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET ((sf_param_format_flags1_t)0x04)
#define SF_PARAM_FORMAT_FLAGS1_FLAG08           ((sf_param_format_flags1_t)0x08)
#define SF_PARAM_FORMAT_FLAGS1_FLAG10           ((sf_param_format_flags1_t)0x10)
#define SF_PARAM_FORMAT_FLAGS1_FLAG20           ((sf_param_format_flags1_t)0x20)
#define SF_PARAM_FORMAT_FLAGS1_FLAG40           ((sf_param_format_flags1_t)0x40)
#define SF_PARAM_FORMAT_FLAGS1_OFFSET_PARAM_TYPE ((sf_param_format_flags1_t)0x80)
SF_PARAM_STATIC_ASSERT(sizeof(sf_param_format_flags1_t) == 1, "FormatFlags1 must be 1 byte");

typedef uint8_t sf_param_format_flags2_t;
#define SF_PARAM_FORMAT_FLAGS2_NONE              ((sf_param_format_flags2_t)0x00)
#define SF_PARAM_FORMAT_FLAGS2_UNICODE_ROW_NAMES ((sf_param_format_flags2_t)0x01)
#define SF_PARAM_FORMAT_FLAGS2_FLAG02            ((sf_param_format_flags2_t)0x02)
#define SF_PARAM_FORMAT_FLAGS2_FLAG04            ((sf_param_format_flags2_t)0x04)
#define SF_PARAM_FORMAT_FLAGS2_FLAG08            ((sf_param_format_flags2_t)0x08)
#define SF_PARAM_FORMAT_FLAGS2_FLAG10            ((sf_param_format_flags2_t)0x10)
#define SF_PARAM_FORMAT_FLAGS2_FLAG20            ((sf_param_format_flags2_t)0x20)
#define SF_PARAM_FORMAT_FLAGS2_FLAG40            ((sf_param_format_flags2_t)0x40)
#define SF_PARAM_FORMAT_FLAGS2_FLAG80            ((sf_param_format_flags2_t)0x80)
SF_PARAM_STATIC_ASSERT(sizeof(sf_param_format_flags2_t) == 1, "FormatFlags2 must be 1 byte");

typedef enum sf_param_apply_mode {
    SF_PARAM_APPLY_UNCONDITIONAL = 0,
    SF_PARAM_APPLY_SOMEWHAT_CAREFUL = 1,
    SF_PARAM_APPLY_CAREFUL = 2,
} sf_param_apply_mode_t;
SF_PARAM_STATIC_ASSERT(SF_PARAM_APPLY_CAREFUL == 2, "apply mode constants must be stable");

typedef enum sf_param_cell_kind {
    SF_PARAM_CELL_KIND_U8 = 0,
    SF_PARAM_CELL_KIND_S8,
    SF_PARAM_CELL_KIND_U16,
    SF_PARAM_CELL_KIND_S16,
    SF_PARAM_CELL_KIND_U32,
    SF_PARAM_CELL_KIND_S32,
    SF_PARAM_CELL_KIND_U64,
    SF_PARAM_CELL_KIND_S64,
    SF_PARAM_CELL_KIND_B32,
    SF_PARAM_CELL_KIND_F32,
    SF_PARAM_CELL_KIND_ANGLE32,
    SF_PARAM_CELL_KIND_F64,
    SF_PARAM_CELL_KIND_DUMMY8_BIT,
    SF_PARAM_CELL_KIND_DUMMY8_ARRAY,
    SF_PARAM_CELL_KIND_U8_ARRAY,
    SF_PARAM_CELL_KIND_FIXSTR,
    SF_PARAM_CELL_KIND_FIXSTR_W,
} sf_param_cell_kind_t;
SF_PARAM_STATIC_ASSERT(SF_PARAM_CELL_KIND_FIXSTR_W + 1 == 17, "cell kind count must be 17");

typedef struct sf_param_cell_value {
    sf_param_cell_kind_t kind;
    union {
        uint8_t u8;
        int8_t s8;
        uint16_t u16;
        int16_t s16;
        uint32_t u32;
        int32_t s32;
        uint64_t u64;
        int64_t s64;
        uint32_t b32;
        float f32;
        float angle32;
        double f64;
        struct {
            const uint8_t *data;
            size_t size;
        } bytes;
        const char *str_utf8;
    } v;
} sf_param_cell_value_t;

SF_API sf_result_t sf_param_read_from_memory(sf_param_t **out, const uint8_t *data,
                                             size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_param_read_from_stream(sf_param_t **out, sf_istream_t *stream,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_param_read_from_path(sf_param_t **out, const wchar_t *path,
                                           const sf_allocator_t *alloc);

SF_API sf_result_t sf_param_write_to_memory(const sf_param_t *param, uint8_t **out,
                                            size_t *out_size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_param_write_to_stream(const sf_param_t *param, sf_ostream_t *stream,
                                            const sf_allocator_t *alloc);
SF_API sf_result_t sf_param_write_to_path(const sf_param_t *param, const wchar_t *path,
                                          const sf_allocator_t *alloc);

SF_API sf_result_t sf_param_apply_paramdef(sf_param_t *param, const sf_paramdef_t *paramdef,
                                           sf_param_apply_mode_t mode);
SF_API sf_result_t sf_param_apply_paramdef_multi(
    sf_param_t *param, const sf_paramdef_t *const *paramdefs, size_t paramdef_count,
    sf_param_apply_mode_t mode);

SF_API void sf_param_destroy(sf_param_t *param);

SF_API const char *sf_param_get_param_type(const sf_param_t *param);
SF_API bool sf_param_is_big_endian(const sf_param_t *param);
SF_API sf_param_format_flags1_t sf_param_get_format_flags1(const sf_param_t *param);
SF_API sf_param_format_flags2_t sf_param_get_format_flags2(const sf_param_t *param);
SF_API uint8_t sf_param_get_paramdef_format_version(const sf_param_t *param);
SF_API int16_t sf_param_get_paramdef_data_version(const sf_param_t *param);
SF_API size_t sf_param_get_row_count(const sf_param_t *param);
SF_API const sf_param_row_t *sf_param_get_row(const sf_param_t *param, size_t index);
SF_API const sf_param_row_t *sf_param_find_row_by_id(const sf_param_t *param, int32_t id);
/* Mutable accessor invalidation: mut accessors invalidate previously-returned
 * const accessors to the same row. Callers promise to serialize all mutations
 * on a single thread and not hold mutable pointers across other mutating calls
 * to the same row. */
SF_API sf_param_row_t *sf_param_get_row_mut(sf_param_t *param, size_t index);
SF_API sf_param_row_t *sf_param_find_row_by_id_mut(sf_param_t *param, int64_t id);

/**
 * Append a new row with the given id to the PARAM, and apply the previously-attached
 * paramdef to it so its cells become writable. Returns the new row pointer in *out_row.
 *
 * The new row is allocated using the PARAM's allocator. It is appended to the rows array
 * (at index row_count-1 after a row_count++). The row's data buffer is zeroed initially.
 * After this call, sf_param_row_find_cell_mut() works on the new row identically to
 * existing rows.
 *
 * Requires: sf_param_apply_paramdef() has been called on this PARAM. If not applied,
 * returns SF_ERR_INVALID_STATE.
 *
 * If row with same id already exists, the new row is still appended (PARAM tolerates
 * duplicates per upstream C# behavior).
 */
SF_API sf_result_t sf_param_add_row_by_id(sf_param_t *param, int32_t id,
                                          const char *name_optional,
                                          sf_param_row_t **out_row);
SF_API sf_result_t sf_param_sort_rows_by_id(sf_param_t *param);

SF_API int32_t sf_param_row_get_id(const sf_param_row_t *row);
SF_API const char *sf_param_row_get_name(const sf_param_row_t *row);
SF_API size_t sf_param_row_get_cell_count(const sf_param_row_t *row);
SF_API const sf_param_cell_t *sf_param_row_get_cell(const sf_param_row_t *row, size_t index);
SF_API const sf_param_cell_t *sf_param_row_find_cell(const sf_param_row_t *row,
                                                      const char *internal_name);
SF_API sf_param_cell_t *sf_param_row_get_cell_mut(sf_param_row_t *row, size_t index);
SF_API sf_param_cell_t *sf_param_row_find_cell_mut(sf_param_row_t *row,
                                                   const char *internal_name);
SF_API sf_result_t sf_param_row_copy(sf_param_row_t *dst, const sf_param_row_t *src);

SF_API sf_param_cell_value_t sf_param_cell_get_value(const sf_param_cell_t *cell);
SF_API uint8_t sf_param_cell_get_u8(const sf_param_cell_t *cell);
SF_API int8_t sf_param_cell_get_s8(const sf_param_cell_t *cell);
SF_API uint16_t sf_param_cell_get_u16(const sf_param_cell_t *cell);
SF_API int16_t sf_param_cell_get_s16(const sf_param_cell_t *cell);
SF_API uint32_t sf_param_cell_get_u32(const sf_param_cell_t *cell);
SF_API int32_t sf_param_cell_get_s32(const sf_param_cell_t *cell);
SF_API uint32_t sf_param_cell_get_b32(const sf_param_cell_t *cell);
SF_API float sf_param_cell_get_f32(const sf_param_cell_t *cell);
SF_API float sf_param_cell_get_angle32(const sf_param_cell_t *cell);
SF_API double sf_param_cell_get_f64(const sf_param_cell_t *cell);
SF_API sf_result_t sf_param_cell_get_bytes(const sf_param_cell_t *cell,
                                            const uint8_t **out_data, size_t *out_size);
SF_API const char *sf_param_cell_get_string(const sf_param_cell_t *cell);
SF_API bool sf_param_cell_get_bool(const sf_param_cell_t *cell);

SF_API sf_result_t sf_param_cell_set_s64(sf_param_cell_t *cell, int64_t  value);
SF_API sf_result_t sf_param_cell_set_u64(sf_param_cell_t *cell, uint64_t value);
SF_API sf_result_t sf_param_cell_set_f32(sf_param_cell_t *cell, float    value);
SF_API sf_result_t sf_param_cell_set_angle32(sf_param_cell_t *cell, float value);
SF_API sf_result_t sf_param_cell_set_f64(sf_param_cell_t *cell, double   value);
SF_API sf_result_t sf_param_cell_set_bool(sf_param_cell_t *cell, bool    value);
SF_API sf_result_t sf_param_cell_set_byte(sf_param_cell_t *cell, uint8_t value);
SF_API sf_result_t sf_param_cell_set_fixstr(sf_param_cell_t *cell, const char *value, size_t value_len);
SF_API sf_result_t sf_param_cell_set_fixstr_w(sf_param_cell_t *cell, const wchar_t *value, size_t value_len);
SF_API sf_result_t sf_param_cell_set_bytes(sf_param_cell_t *cell, const uint8_t *data, size_t size);

SF_API sf_result_t sf_param_cell_set_s8(sf_param_cell_t *cell, int8_t value);
SF_API sf_result_t sf_param_cell_set_u8(sf_param_cell_t *cell, uint8_t value);
SF_API sf_result_t sf_param_cell_set_s16(sf_param_cell_t *cell, int16_t value);
SF_API sf_result_t sf_param_cell_set_u16(sf_param_cell_t *cell, uint16_t value);
SF_API sf_result_t sf_param_cell_set_s32(sf_param_cell_t *cell, int32_t value);
SF_API sf_result_t sf_param_cell_set_u32(sf_param_cell_t *cell, uint32_t value);

SF_API sf_result_t sf_param_cell_copy(sf_param_cell_t *dst, const sf_param_cell_t *src);

#undef SF_PARAM_STATIC_ASSERT

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SF_PARAM_H */
