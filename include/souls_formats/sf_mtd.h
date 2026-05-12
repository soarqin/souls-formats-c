/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MTD material definition.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/MTD.cs
 *
 * An MTD pairs a shader path with a list of named parameters (Bool / Int /
 * Int2 / Float / Float[2-4]) and a list of texture slots ("g_DiffuseTexture",
 * "g_SpecularTexture", ...). Sekiro introduced "Extended" textures
 * (textureBlock.Version == 5) with an inline fixed path string plus a
 * trailing array of unknown floats; pre-Sekiro materials use Version == 3
 * and have neither (sf_mtd_texture_has_extended() returns false).
 *
 * The upstream block-based wire format (file / header / data / lists / param
 * / value / texture sub-blocks) is fully internal; consumers see a flat
 * shader-path + parameter-list + texture-list view.
 */

#ifndef SOULS_FORMATS_SF_MTD_H
#define SOULS_FORMATS_SF_MTD_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Opaque types
 *===========================================================================*/
typedef struct sf_mtd         sf_mtd_t;
typedef struct sf_mtd_param   sf_mtd_param_t;
typedef struct sf_mtd_texture sf_mtd_texture_t;

/*===========================================================================
 * Enums (mirror upstream MTD.cs)
 *===========================================================================*/

/** Mirrors upstream MTD.Param.ParamType.
 *  Values are upstream-declaration-order (Bool=0..Float4=6). */
typedef enum sf_mtd_param_type {
    SF_MTD_PARAM_TYPE_BOOL   = 0,
    SF_MTD_PARAM_TYPE_INT    = 1,
    SF_MTD_PARAM_TYPE_INT2   = 2,
    SF_MTD_PARAM_TYPE_FLOAT  = 3,
    SF_MTD_PARAM_TYPE_FLOAT2 = 4,
    SF_MTD_PARAM_TYPE_FLOAT3 = 5,
    SF_MTD_PARAM_TYPE_FLOAT4 = 6,
} sf_mtd_param_type_t;
_Static_assert(SF_MTD_PARAM_TYPE_FLOAT4 == 6, "MTD ParamType drift");

/** Mirrors upstream MTD.BlendMode (value of g_BlendMode param).
 *  Numeric values are file-format-defined and must not be changed. */
typedef enum sf_mtd_blend_mode {
    SF_MTD_BLEND_MODE_NORMAL       = 0,
    SF_MTD_BLEND_MODE_TEX_EDGE     = 1,
    SF_MTD_BLEND_MODE_BLEND        = 2,
    SF_MTD_BLEND_MODE_WATER        = 3,
    SF_MTD_BLEND_MODE_ADD          = 4,
    SF_MTD_BLEND_MODE_SUB          = 5,
    SF_MTD_BLEND_MODE_MUL          = 6,
    SF_MTD_BLEND_MODE_ADD_MUL      = 7,
    SF_MTD_BLEND_MODE_SUB_MUL      = 8,
    SF_MTD_BLEND_MODE_WATER_WAVE   = 9,
    SF_MTD_BLEND_MODE_LS_NORMAL    = 32,
    SF_MTD_BLEND_MODE_LS_TEX_EDGE  = 33,
    SF_MTD_BLEND_MODE_LS_BLEND     = 34,
    SF_MTD_BLEND_MODE_LS_WATER     = 35,
    SF_MTD_BLEND_MODE_LS_ADD       = 36,
    SF_MTD_BLEND_MODE_LS_SUB       = 37,
    SF_MTD_BLEND_MODE_LS_MUL       = 38,
    SF_MTD_BLEND_MODE_LS_ADD_MUL   = 39,
    SF_MTD_BLEND_MODE_LS_SUB_MUL   = 40,
    SF_MTD_BLEND_MODE_LS_WATER_WAVE = 41,
} sf_mtd_blend_mode_t;
_Static_assert(SF_MTD_BLEND_MODE_LS_WATER_WAVE == 41, "MTD BlendMode drift");

/** Mirrors upstream MTD.LightingType (value of g_LightingType param).
 *  Note the gap: HemEnvDifSpc == 3, no value 2 exists upstream. */
typedef enum sf_mtd_lighting_type {
    SF_MTD_LIGHTING_TYPE_NONE                = 0,
    SF_MTD_LIGHTING_TYPE_HEM_DIR_DIF_SPC_X3  = 1,
    SF_MTD_LIGHTING_TYPE_HEM_ENV_DIF_SPC     = 3,
} sf_mtd_lighting_type_t;
_Static_assert(SF_MTD_LIGHTING_TYPE_HEM_ENV_DIF_SPC == 3,
               "MTD LightingType drift");

/*===========================================================================
 * Read / write / destroy
 *===========================================================================*/
SF_API sf_result_t sf_mtd_read_from_memory(sf_mtd_t **out, const uint8_t *data,
                                           size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_mtd_read_from_stream(sf_mtd_t **out, sf_istream_t *stream,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_mtd_read_from_path(sf_mtd_t **out, const wchar_t *path,
                                         const sf_allocator_t *alloc);

SF_API sf_result_t sf_mtd_write_to_memory(const sf_mtd_t *mtd, uint8_t **out_data,
                                          size_t *out_size,
                                          const sf_allocator_t *alloc);
SF_API sf_result_t sf_mtd_write_to_stream(const sf_mtd_t *mtd, sf_ostream_t *stream,
                                          const sf_allocator_t *alloc);
SF_API sf_result_t sf_mtd_write_to_path(const sf_mtd_t *mtd, const wchar_t *path,
                                        const sf_allocator_t *alloc);

SF_API void sf_mtd_destroy(sf_mtd_t *mtd);

/*===========================================================================
 * Top-level field accessors
 *
 * Returned strings are UTF-8, NUL-terminated, and owned by the MTD; they
 * remain valid until sf_mtd_destroy() is called. NULL out-pointer for a
 * missing optional value is never returned — the underlying string is at
 * minimum the empty string "" on a successfully parsed MTD.
 *===========================================================================*/
SF_API const char *sf_mtd_shader_path(const sf_mtd_t *mtd);
SF_API const char *sf_mtd_description(const sf_mtd_t *mtd);

/*===========================================================================
 * Params
 *
 * Params are positional; ordering matches the on-disk order. The typed
 * value accessors return SF_OK on a type match and SF_ERR_INVALID_ARG on
 * mismatch (e.g. calling sf_mtd_param_value_int on a Float param).
 *
 * For vector types (Int2, Float2/3/4) the caller passes a buffer of the
 * appropriate length; sf_mtd_param_value_*() writes exactly the number of
 * components implied by sf_mtd_param_type().
 *===========================================================================*/
SF_API size_t                sf_mtd_param_count(const sf_mtd_t *mtd);
SF_API const sf_mtd_param_t *sf_mtd_param(const sf_mtd_t *mtd, size_t index);

SF_API const char           *sf_mtd_param_name(const sf_mtd_param_t *param);
SF_API sf_mtd_param_type_t   sf_mtd_param_type(const sf_mtd_param_t *param);

SF_API sf_result_t sf_mtd_param_value_bool(const sf_mtd_param_t *param,
                                           bool *out_value);
SF_API sf_result_t sf_mtd_param_value_int(const sf_mtd_param_t *param,
                                          int32_t *out_value);
SF_API sf_result_t sf_mtd_param_value_int2(const sf_mtd_param_t *param,
                                           int32_t out_values[2]);
SF_API sf_result_t sf_mtd_param_value_float(const sf_mtd_param_t *param,
                                            float *out_value);
SF_API sf_result_t sf_mtd_param_value_float2(const sf_mtd_param_t *param,
                                             float out_values[2]);
SF_API sf_result_t sf_mtd_param_value_float3(const sf_mtd_param_t *param,
                                             float out_values[3]);
SF_API sf_result_t sf_mtd_param_value_float4(const sf_mtd_param_t *param,
                                             float out_values[4]);

/*===========================================================================
 * Textures
 *
 * Textures are positional; ordering matches the on-disk order. The "type"
 * field is the shader sampler name (e.g. "g_DiffuseTexture",
 * "g_SpecularTexture"). UVNumber identifies which UV set in the FLVER
 * vertex data this sampler reads from; ShaderDataIndex is reserved.
 *
 * Sekiro Extended textures (textureBlock.Version == 5) additionally carry
 * an inline fixed texture path and an array of unknown floats; non-Extended
 * textures (Version == 3) report sf_mtd_texture_has_extended() == false,
 * sf_mtd_texture_path() == "" and sf_mtd_texture_unk_float_count() == 0.
 *===========================================================================*/
SF_API size_t                  sf_mtd_texture_count(const sf_mtd_t *mtd);
SF_API const sf_mtd_texture_t *sf_mtd_texture(const sf_mtd_t *mtd, size_t index);

SF_API const char *sf_mtd_texture_type(const sf_mtd_texture_t *texture);
SF_API int32_t     sf_mtd_texture_uv_number(const sf_mtd_texture_t *texture);
SF_API int32_t     sf_mtd_texture_shader_data_index(const sf_mtd_texture_t *texture);

/* Sekiro Extended texture fields (textureBlock.Version == 5). */
SF_API bool        sf_mtd_texture_has_extended(const sf_mtd_texture_t *texture);
SF_API const char *sf_mtd_texture_path(const sf_mtd_texture_t *texture);
SF_API size_t      sf_mtd_texture_unk_float_count(const sf_mtd_texture_t *texture);
SF_API float       sf_mtd_texture_unk_float(const sf_mtd_texture_t *texture, size_t index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MTD_H */
