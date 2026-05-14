/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMDEF public surface.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs
 *   SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs
 *   SoulsFormats/Formats/PARAM/ParamUtil.cs
 */

#ifndef SOULS_FORMATS_SF_PARAMDEF_H
#define SOULS_FORMATS_SF_PARAMDEF_H

#include "sf_common.h"
#include "sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_paramdef sf_paramdef_t;
typedef struct sf_paramdef_field sf_paramdef_field_t;

#if defined(__cplusplus)
#define SF_PARAMDEF_STATIC_ASSERT static_assert
#else
#define SF_PARAMDEF_STATIC_ASSERT _Static_assert
#endif

typedef enum sf_paramdef_def_type {
    SF_PARAMDEF_DEF_TYPE_S8 = 0,
    SF_PARAMDEF_DEF_TYPE_U8,
    SF_PARAMDEF_DEF_TYPE_S16,
    SF_PARAMDEF_DEF_TYPE_U16,
    SF_PARAMDEF_DEF_TYPE_S32,
    SF_PARAMDEF_DEF_TYPE_U32,
    SF_PARAMDEF_DEF_TYPE_S64,
    SF_PARAMDEF_DEF_TYPE_U64,
    SF_PARAMDEF_DEF_TYPE_B32,
    SF_PARAMDEF_DEF_TYPE_F32,
    SF_PARAMDEF_DEF_TYPE_ANGLE32,
    SF_PARAMDEF_DEF_TYPE_F64,
    SF_PARAMDEF_DEF_TYPE_DUMMY8,
    SF_PARAMDEF_DEF_TYPE_FIXSTR,
    SF_PARAMDEF_DEF_TYPE_FIXSTR_W,
} sf_paramdef_def_type_t;
SF_PARAMDEF_STATIC_ASSERT(SF_PARAMDEF_DEF_TYPE_FIXSTR_W + 1 == 15,
                          "DefType count must be 15");

typedef enum sf_paramdef_edit_flags {
    SF_PARAMDEF_EDIT_FLAGS_NONE = 0,
    SF_PARAMDEF_EDIT_FLAGS_WRAP = 1,
    SF_PARAMDEF_EDIT_FLAGS_LOCK = 4,
} sf_paramdef_edit_flags_t;
SF_PARAMDEF_STATIC_ASSERT(SF_PARAMDEF_EDIT_FLAGS_LOCK == 4, "EditFlags LOCK must be 4");

#define SF_PARAMDEF_FORMAT_VERSION_BASIC 0
#define SF_PARAMDEF_FORMAT_VERSION_101 101
#define SF_PARAMDEF_FORMAT_VERSION_102 102
#define SF_PARAMDEF_FORMAT_VERSION_103 103
#define SF_PARAMDEF_FORMAT_VERSION_104 104
#define SF_PARAMDEF_FORMAT_VERSION_106 106
#define SF_PARAMDEF_FORMAT_VERSION_201 201
#define SF_PARAMDEF_FORMAT_VERSION_202 202
#define SF_PARAMDEF_FORMAT_VERSION_203 203
SF_PARAMDEF_STATIC_ASSERT(SF_PARAMDEF_FORMAT_VERSION_203 == 203,
                          "FormatVersion 203 must be stable");

typedef struct sf_paramdef_default_value {
    sf_paramdef_def_type_t type;
    union {
        int8_t s8;
        uint8_t u8;
        int16_t s16;
        uint16_t u16;
        int32_t s32;
        uint32_t u32;
        int64_t s64;
        uint64_t u64;
        uint32_t b32;
        float f32;
        float angle32;
        double f64;
    } v;
} sf_paramdef_default_value_t;

SF_API sf_result_t sf_paramdef_read_from_memory(sf_paramdef_t **out, const uint8_t *data,
                                                size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_paramdef_read_from_stream(sf_paramdef_t **out, sf_istream_t *stream,
                                                const sf_allocator_t *alloc);
SF_API sf_result_t sf_paramdef_read_from_path(sf_paramdef_t **out, const wchar_t *path,
                                              const sf_allocator_t *alloc);

SF_API sf_result_t sf_paramdef_read_xml_from_memory(sf_paramdef_t **out, const char *xml,
                                                    size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_paramdef_read_xml_from_path(sf_paramdef_t **out, const wchar_t *path,
                                                  const sf_allocator_t *alloc);

SF_API sf_result_t sf_paramdef_write_to_memory(const sf_paramdef_t *paramdef, uint8_t **out,
                                               size_t *out_size,
                                               const sf_allocator_t *alloc);
SF_API sf_result_t sf_paramdef_write_to_stream(const sf_paramdef_t *paramdef,
                                               sf_ostream_t *stream,
                                               const sf_allocator_t *alloc);
SF_API sf_result_t sf_paramdef_write_to_path(const sf_paramdef_t *paramdef,
                                             const wchar_t *path,
                                             const sf_allocator_t *alloc);

SF_API void sf_paramdef_destroy(sf_paramdef_t *paramdef);

SF_API int16_t sf_paramdef_get_data_version(const sf_paramdef_t *paramdef);
SF_API const char *sf_paramdef_get_param_type(const sf_paramdef_t *paramdef);
SF_API bool sf_paramdef_is_big_endian(const sf_paramdef_t *paramdef);
SF_API bool sf_paramdef_is_unicode(const sf_paramdef_t *paramdef);
SF_API int16_t sf_paramdef_get_format_version(const sf_paramdef_t *paramdef);
SF_API bool sf_paramdef_is_version_aware(const sf_paramdef_t *paramdef);
SF_API size_t sf_paramdef_get_field_count(const sf_paramdef_t *paramdef);
SF_API const sf_paramdef_field_t *sf_paramdef_get_field(const sf_paramdef_t *paramdef,
                                                        size_t index);
SF_API int32_t sf_paramdef_get_row_size(const sf_paramdef_t *paramdef);

/* Extension: Paramdex XML only; binary paramdefs return -1. */
SF_API int32_t sf_paramdef_get_index(const sf_paramdef_t *paramdef);

SF_API const char *sf_paramdef_field_get_display_name(const sf_paramdef_field_t *field);
SF_API const char *sf_paramdef_field_get_internal_name(const sf_paramdef_field_t *field);
SF_API const char *sf_paramdef_field_get_description(const sf_paramdef_field_t *field);
SF_API sf_paramdef_def_type_t sf_paramdef_field_get_display_type(
    const sf_paramdef_field_t *field);
SF_API const char *sf_paramdef_field_get_display_format(const sf_paramdef_field_t *field);
SF_API sf_paramdef_default_value_t sf_paramdef_field_get_default_value(
    const sf_paramdef_field_t *field);
SF_API sf_paramdef_default_value_t sf_paramdef_field_get_minimum(
    const sf_paramdef_field_t *field);
SF_API sf_paramdef_default_value_t sf_paramdef_field_get_maximum(
    const sf_paramdef_field_t *field);
SF_API sf_paramdef_default_value_t sf_paramdef_field_get_increment(
    const sf_paramdef_field_t *field);
SF_API sf_paramdef_edit_flags_t sf_paramdef_field_get_edit_flags(
    const sf_paramdef_field_t *field);
SF_API int32_t sf_paramdef_field_get_byte_count(const sf_paramdef_field_t *field);
SF_API int32_t sf_paramdef_field_get_bit_size(const sf_paramdef_field_t *field);
SF_API int32_t sf_paramdef_field_get_array_length(const sf_paramdef_field_t *field);

/* Extension: Paramdex XML only; binary paramdefs return 0. */
SF_API int32_t sf_paramdef_field_get_sort_id(const sf_paramdef_field_t *field);

SF_API uint64_t sf_paramdef_field_get_first_regulation_version(
    const sf_paramdef_field_t *field);
SF_API uint64_t sf_paramdef_field_get_removed_regulation_version(
    const sf_paramdef_field_t *field);

SF_API size_t sf_param_util_get_value_size(sf_paramdef_def_type_t type);
SF_API bool sf_param_util_is_bit_type(sf_paramdef_def_type_t type);
SF_API int sf_param_util_get_bit_limit(sf_paramdef_def_type_t type);

#undef SF_PARAMDEF_STATIC_ASSERT

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_PARAMDEF_H */
