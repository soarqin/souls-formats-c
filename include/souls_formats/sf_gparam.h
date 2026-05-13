// Upstream: GPARAM.cs
/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_GPARAM_H
#define SOULS_FORMATS_SF_GPARAM_H

#include "sf_common.h"
#include "sf_math.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_gparam_version {
    SF_GPARAM_VERSION_V2 = 2,
    SF_GPARAM_VERSION_V3 = 3,
    SF_GPARAM_VERSION_V5 = 5,
    SF_GPARAM_VERSION_V6 = 6,
} sf_gparam_version_t;

_Static_assert(SF_GPARAM_VERSION_V2 == 2, "GPARAM version drift (V2)");
_Static_assert(SF_GPARAM_VERSION_V6 == 6, "GPARAM version drift (V6)");

typedef enum sf_gparam_field_type {
    SF_GPARAM_FIELD_TYPE_SBYTE = 1,
    SF_GPARAM_FIELD_TYPE_SHORT = 2,
    SF_GPARAM_FIELD_TYPE_INT = 3,
    SF_GPARAM_FIELD_TYPE_LONG = 4,
    SF_GPARAM_FIELD_TYPE_BYTE = 5,
    SF_GPARAM_FIELD_TYPE_USHORT = 6,
    SF_GPARAM_FIELD_TYPE_UINT = 7,
    SF_GPARAM_FIELD_TYPE_ULONG = 8,
    SF_GPARAM_FIELD_TYPE_FLOAT = 9,
    SF_GPARAM_FIELD_TYPE_DOUBLE = 10,
    SF_GPARAM_FIELD_TYPE_BOOL = 11,
    SF_GPARAM_FIELD_TYPE_VEC2 = 12,
    SF_GPARAM_FIELD_TYPE_VEC3 = 13,
    SF_GPARAM_FIELD_TYPE_VEC4 = 14,
    SF_GPARAM_FIELD_TYPE_COLOR = 15,
    SF_GPARAM_FIELD_TYPE_STRING = 16,
} sf_gparam_field_type_t;

_Static_assert(SF_GPARAM_FIELD_TYPE_SBYTE == 1, "GPARAM field type drift (SBYTE)");
_Static_assert(SF_GPARAM_FIELD_TYPE_STRING == 16, "GPARAM field type drift (STRING)");

/*  Tagged-union value POD. Mirrors upstream FieldValue<T> (GPARAM.cs:912):
 *    public int   Id    { get; set; }    // signed int32
 *    public float Unk04 { get; set; }    // single-precision float (V>=V5)
 *    public T     Value { get; set; }    // discriminated by Field's type
 *  The active union member is selected by `type`. The unk04 field is read
 *  from / written to the wire only for V>=V5; for V3 it is left zero. */
typedef struct sf_gparam_value {
    sf_gparam_field_type_t type;
    int32_t id;
    float unk04;
    union {
        int8_t   as_sbyte;
        int16_t  as_short;
        int32_t  as_int;
        int64_t  as_long;
        uint8_t  as_byte;
        uint16_t as_ushort;
        uint32_t as_uint;
        uint64_t as_ulong;
        float    as_float;
        double   as_double;
        int8_t   as_bool;
        sf_vec2_t as_vec2;
        sf_vec3_t as_vec3;
        sf_vec4_t as_vec4;
        sf_color_t as_color;
        const char *as_string;
    } v;
} sf_gparam_value_t;

_Static_assert(sizeof(sf_gparam_value_t) <= 32, "value POD too large");

typedef struct sf_gparam sf_gparam_t;
typedef struct sf_gparam_param sf_gparam_param_t;
typedef struct sf_gparam_field sf_gparam_field_t;
typedef struct sf_gparam_unk_param_extra sf_gparam_unk_param_extra_t;

SF_API sf_result_t sf_gparam_read_from_memory(sf_gparam_t **out, const void *bytes, size_t size,
                                              const sf_allocator_t *alloc);
SF_API sf_result_t sf_gparam_write_to_buffer(const sf_gparam_t *gparam, void **out_bytes,
                                             size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_gparam_destroy(sf_gparam_t *gparam);

SF_API sf_gparam_version_t sf_gparam_get_version(const sf_gparam_t *gparam);
SF_API bool sf_gparam_get_unk0d(const sf_gparam_t *gparam);
SF_API int32_t sf_gparam_get_count14(const sf_gparam_t *gparam);
SF_API float sf_gparam_get_unk40(const sf_gparam_t *gparam);
SF_API float sf_gparam_get_unk50(const sf_gparam_t *gparam);
SF_API const uint8_t *sf_gparam_get_data30(const sf_gparam_t *gparam, size_t *out_size);

SF_API size_t sf_gparam_param_count(const sf_gparam_t *gparam);
SF_API const sf_gparam_param_t *sf_gparam_get_param(const sf_gparam_t *gparam, size_t index);

SF_API const char *sf_gparam_param_get_key(const sf_gparam_param_t *param);
SF_API const char *sf_gparam_param_get_name(const sf_gparam_param_t *param);
SF_API size_t sf_gparam_param_field_count(const sf_gparam_param_t *param);
SF_API const sf_gparam_field_t *sf_gparam_param_get_field(const sf_gparam_param_t *param,
                                                          size_t index);
SF_API size_t sf_gparam_param_comment_count(const sf_gparam_param_t *param);
SF_API const char *sf_gparam_param_get_comment(const sf_gparam_param_t *param, size_t index);

SF_API const char *sf_gparam_field_get_key(const sf_gparam_field_t *field);
SF_API const char *sf_gparam_field_get_name(const sf_gparam_field_t *field);
SF_API sf_gparam_field_type_t sf_gparam_field_get_type(const sf_gparam_field_t *field);
SF_API size_t sf_gparam_field_value_count(const sf_gparam_field_t *field);
SF_API sf_gparam_value_t sf_gparam_field_get_value(const sf_gparam_field_t *field, size_t index);

SF_API size_t sf_gparam_unk_param_extra_count(const sf_gparam_t *gparam);
SF_API const sf_gparam_unk_param_extra_t *sf_gparam_get_unk_param_extra(const sf_gparam_t *gparam,
                                                                        size_t index);

SF_API int32_t sf_gparam_unk_param_extra_get_unk00(const sf_gparam_unk_param_extra_t *extra);
SF_API size_t sf_gparam_unk_param_extra_id_count(const sf_gparam_unk_param_extra_t *extra);
SF_API int32_t sf_gparam_unk_param_extra_get_id(const sf_gparam_unk_param_extra_t *extra,
                                                 size_t index);
SF_API int32_t sf_gparam_unk_param_extra_get_unk0c(const sf_gparam_unk_param_extra_t *extra);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_GPARAM_H */
