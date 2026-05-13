/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMDBP / DBPPARAM public surface.
 *
 * PARAMDBP describes the schema (field types, names, ranges) for AC-specific
 * DBP params. DBPPARAM holds the actual data and requires a PARAMDBP to be
 * applied before cells can be read.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/PARAM/PARAMDBP/PARAMDBP.cs
 *   SoulsFormats/Formats/PARAM/PARAMDBP/DBPPARAM.cs
 *   SoulsFormats/Formats/PARAM/ParamDbpUtil.cs
 */

#ifndef SOULS_FORMATS_SF_PARAMDBP_H
#define SOULS_FORMATS_SF_PARAMDBP_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_paramdbp sf_paramdbp_t;
typedef struct sf_dbpparam sf_dbpparam_t;

typedef enum sf_dbp_type {
    SF_DBP_TYPE_S8  = 0,
    SF_DBP_TYPE_U8  = 1,
    SF_DBP_TYPE_S16 = 2,
    SF_DBP_TYPE_U16 = 3,
    SF_DBP_TYPE_S32 = 4,
    SF_DBP_TYPE_U32 = 5,
    SF_DBP_TYPE_F32 = 6,
} sf_dbp_type_t;
_Static_assert(SF_DBP_TYPE_F32 == 6, "DbpType drift");

typedef union sf_dbp_value {
    int8_t   s8;
    uint8_t  u8;
    int16_t  s16;
    uint16_t u16;
    int32_t  s32;
    uint32_t u32;
    float    f32;
} sf_dbp_value_t;

typedef struct sf_paramdbp_field {
    sf_dbp_type_t type;
    char *display_name;
    char *display_format;
    sf_dbp_value_t default_val;
    sf_dbp_value_t increment;
    sf_dbp_value_t minimum;
    sf_dbp_value_t maximum;
} sf_paramdbp_field_t;

typedef struct sf_dbpparam_cell {
    const sf_paramdbp_field_t *field;
    sf_dbp_value_t value;
} sf_dbpparam_cell_t;

SF_API sf_result_t sf_paramdbp_create(sf_paramdbp_t **out, bool big_endian,
                                      const sf_allocator_t *alloc);
SF_API void sf_paramdbp_destroy(sf_paramdbp_t *dbp);

SF_API sf_result_t sf_paramdbp_read_from_memory(sf_paramdbp_t **out, const void *bytes,
                                                size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_paramdbp_write_to_memory(const sf_paramdbp_t *dbp, void **out_bytes,
                                               size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_paramdbp_is_big_endian(const sf_paramdbp_t *dbp);
SF_API size_t sf_paramdbp_field_count(const sf_paramdbp_t *dbp);
SF_API const sf_paramdbp_field_t *sf_paramdbp_get_field(const sf_paramdbp_t *dbp, size_t index);
SF_API sf_result_t sf_paramdbp_add_field(sf_paramdbp_t *dbp, const sf_paramdbp_field_t *field);
SF_API int sf_paramdbp_calculate_param_size(const sf_paramdbp_t *dbp);

SF_API sf_result_t sf_dbpparam_create(sf_dbpparam_t **out, const sf_allocator_t *alloc);
SF_API void sf_dbpparam_destroy(sf_dbpparam_t *param);

SF_API sf_result_t sf_dbpparam_read_from_memory(sf_dbpparam_t **out, const void *bytes,
                                                size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_dbpparam_write_to_memory(const sf_dbpparam_t *param, void **out_bytes,
                                               size_t *out_size, const sf_allocator_t *alloc);

SF_API sf_result_t sf_dbpparam_apply_paramdbp(sf_dbpparam_t *param, const sf_paramdbp_t *dbp);
SF_API bool sf_dbpparam_is_applied(const sf_dbpparam_t *param);
SF_API size_t sf_dbpparam_cell_count(const sf_dbpparam_t *param);
SF_API const sf_dbpparam_cell_t *sf_dbpparam_get_cell(const sf_dbpparam_t *param, size_t index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_PARAMDBP_H */
