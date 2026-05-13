/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_matbin.h
 * @brief MATBIN — Elden Ring / Armored Core VI material binary (.matbin).
 *
 * Magic: "MAB\0". Always little-endian. Version field is the 32-bit int
 * immediately after the magic and is always 2 in upstream-known files.
 *
 * A MATBIN binds a shader to a list of parameters and texture samplers. The
 * parameter list is heterogeneous: each entry carries one of eight
 * non-consecutive `ParamType` variants (Bool/Int/Int2/Float/Float2..Float5).
 * Values 1, 2, 3, 6, 7 are intentional gaps and MUST NOT be added.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/MATBIN.cs
 *     - class MATBIN              (ShaderPath, SourcePath, Key, Params, Samplers)
 *     - enum  ParamType : uint    (Bool=0, Int=4, Int2=5, Float=8, Float2..5=9..12)
 *     - class Param               (Name, Value, Key, Type)
 *     - class Sampler             (Type, Path, Key, Unk14)
 *
 * Survey evidence (.sisyphus/evidence/task-5-matbin-survey.txt) confirms
 * the eight-variant distribution across a 15103-entry corpus.
 */
#ifndef SF_MATBIN_H
#define SF_MATBIN_H

#include "sf_common.h"
#include "souls_formats/sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ─────────────────────────────────────────────────────────── */

typedef struct sf_matbin         sf_matbin_t;
typedef struct sf_matbin_param   sf_matbin_param_t;
typedef struct sf_matbin_sampler sf_matbin_sampler_t;

/* ── ParamType — 8 non-consecutive variants ───────────────────────────────── */

/**
 * Mirrors upstream MATBIN.ParamType (uint, file-format-defined).
 * The numeric values are NOT a dense range: 1, 2, 3, 6, 7 are intentionally
 * unused upstream and a reader that encounters them MUST fail rather than
 * silently extend the enum.
 *
 * Use the `_Static_assert` below as a drift guard.
 */
typedef enum sf_matbin_param_type {
    SF_MATBIN_PARAM_TYPE_BOOL   = 0,   /**< 1-byte boolean. */
    SF_MATBIN_PARAM_TYPE_INT    = 4,   /**< i32. */
    SF_MATBIN_PARAM_TYPE_INT2   = 5,   /**< i32[2]. */
    SF_MATBIN_PARAM_TYPE_FLOAT  = 8,   /**< f32. */
    SF_MATBIN_PARAM_TYPE_FLOAT2 = 9,   /**< f32[2]. */
    SF_MATBIN_PARAM_TYPE_FLOAT3 = 10,  /**< f32[3]; file stores 5 floats, extras discarded per upstream. */
    SF_MATBIN_PARAM_TYPE_FLOAT4 = 11,  /**< f32[4]. */
    SF_MATBIN_PARAM_TYPE_FLOAT5 = 12,  /**< f32[5]. */
} sf_matbin_param_type_t;

/* No _COUNT sentinel — the enum is intentionally non-consecutive. The single
 * tail-of-range assertion below catches accidental insertions before FLOAT5. */
_Static_assert(12 == SF_MATBIN_PARAM_TYPE_FLOAT5, "MATBIN ParamType drift");

/* ── Read / Write / Destroy ───────────────────────────────────────────────── */

/**
 * Parse a MATBIN from a contiguous in-memory blob.
 * On success, *out is a heap-owned object the caller must release with
 * sf_matbin_destroy(). `bytes` may be freed immediately after the call returns.
 */
SF_API sf_result_t sf_matbin_read_from_memory(sf_matbin_t        **out,
                                              const void          *bytes,
                                              size_t               size,
                                              const sf_allocator_t *alloc);

/**
 * Parse a MATBIN from a wide-character filesystem path.
 * Uses sf_istream internally; never calls stdio. `path` is a Win32 wide path.
 */
SF_API sf_result_t sf_matbin_read_from_path(sf_matbin_t        **out,
                                            const wchar_t       *path,
                                            const sf_allocator_t *alloc);

/**
 * Serialize a MATBIN to a fresh heap buffer.
 * On success, *out_bytes is owned by the caller (free via sf_free with `alloc`).
 */
SF_API sf_result_t sf_matbin_write_to_memory(const sf_matbin_t  *m,
                                             void              **out_bytes,
                                             size_t             *out_size,
                                             const sf_allocator_t *alloc);

/** Serialize a MATBIN to a wide-character filesystem path. */
SF_API sf_result_t sf_matbin_write_to_path(const sf_matbin_t  *m,
                                           const wchar_t       *path,
                                           const sf_allocator_t *alloc);

/** Release a MATBIN and all internally-owned strings / arrays. Safe on NULL. */
SF_API void sf_matbin_destroy(sf_matbin_t *m);

/* ── Top-level fields ─────────────────────────────────────────────────────── */

/** Network path to the shader source file. UTF-8. Lifetime: until destroy. */
SF_API const char *sf_matbin_shader_path(const sf_matbin_t *m);

/** Network path to the material source file (matxml or mtd). UTF-8. */
SF_API const char *sf_matbin_source_path(const sf_matbin_t *m);

/** Documentation identifier; opaque u32. */
SF_API uint32_t sf_matbin_key(const sf_matbin_t *m);

/* ── Params ───────────────────────────────────────────────────────────────── */

/** Number of parameters in the material. */
SF_API size_t sf_matbin_param_count(const sf_matbin_t *m);

/** Returns parameter `i`, or NULL if `i >= sf_matbin_param_count(m)`. */
SF_API const sf_matbin_param_t *sf_matbin_param(const sf_matbin_t *m, size_t i);

/** Parameter name. UTF-8. Lifetime: until destroy. */
SF_API const char *sf_matbin_param_name(const sf_matbin_param_t *p);

/** Parameter value variant tag. */
SF_API sf_matbin_param_type_t sf_matbin_param_type(const sf_matbin_param_t *p);

/** Documentation identifier; opaque u32. */
SF_API uint32_t sf_matbin_param_key(const sf_matbin_param_t *p);

/* ── Typed value accessors (8) ────────────────────────────────────────────── */
/* All accessors return SF_ERR_INVALID_ARG if `p` is NULL, `out` is NULL, or
 * the parameter's runtime type does not match the requested accessor. */

SF_API sf_result_t sf_matbin_param_value_bool  (const sf_matbin_param_t *p, bool    *out);
SF_API sf_result_t sf_matbin_param_value_int   (const sf_matbin_param_t *p, int32_t *out);
SF_API sf_result_t sf_matbin_param_value_int2  (const sf_matbin_param_t *p, int32_t  out[2]);
SF_API sf_result_t sf_matbin_param_value_float (const sf_matbin_param_t *p, float   *out);
SF_API sf_result_t sf_matbin_param_value_float2(const sf_matbin_param_t *p, float    out[2]);
SF_API sf_result_t sf_matbin_param_value_float3(const sf_matbin_param_t *p, float    out[3]);
SF_API sf_result_t sf_matbin_param_value_float4(const sf_matbin_param_t *p, float    out[4]);
SF_API sf_result_t sf_matbin_param_value_float5(const sf_matbin_param_t *p, float    out[5]);

/* ── Samplers ─────────────────────────────────────────────────────────────── */

/** Number of texture samplers used by the material. */
SF_API size_t sf_matbin_sampler_count(const sf_matbin_t *m);

/** Returns sampler `i`, or NULL if `i >= sf_matbin_sampler_count(m)`. */
SF_API const sf_matbin_sampler_t *sf_matbin_sampler(const sf_matbin_t *m, size_t i);

/** Sampler type (e.g. shader binding name). UTF-8. Lifetime: until destroy. */
SF_API const char *sf_matbin_sampler_type(const sf_matbin_sampler_t *s);

/** Optional network path to the texture; empty string if not specified
 *  (preserved verbatim from upstream bytes; never transformed). */
SF_API const char *sf_matbin_sampler_path(const sf_matbin_sampler_t *s);

/** Documentation identifier; opaque u32. */
SF_API uint32_t sf_matbin_sampler_key(const sf_matbin_sampler_t *s);

/** Unknown 2-float field at offset 0x14 of the sampler record; typically (0,0). */
SF_API sf_result_t sf_matbin_sampler_unk14(const sf_matbin_sampler_t *s, sf_vec2_t *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SF_MATBIN_H */
