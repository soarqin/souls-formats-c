/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TPF (texture pack) container.
 *
 * The TPF format stores game textures (DDS-format on PC, headerless DDS or
 * GNF on consoles) with optional per-texture DCP_EDGE compression.
 * Extension: .tpf, .tpf.dcx
 *
 * Scope (v1):
 *   - Read + write PC TPFs end-to-end (round-trip).
 *   - Read + parse PS3 / Xbox360 / PS4 / Xbone / PS5 metadata.
 *   - Per-texture DCP_EDGE compression auto-decompresses on read and
 *     re-wraps on write when flags1 == 2 or flags1 == 3.
 *   - DX10 cubemap dwCaps2 fix on PC platform only (matches upstream).
 *
 * Out of scope (deferred / extension):
 *   - Console Headerizer (Xbox360 / Xbone / PS3 / PS4 / PS5 wire wrap).
 *     Only the PC pass-through Headerizer path is implemented; calling
 *     the Headerizer on a non-PC platform returns SF_ERR_UNSUPPORTED_VERSION.
 *   - DDS pixel decoding.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/TPF/TPF.cs
 *   SoulsFormats/Formats/TPF/Headerizer.cs (PC pass-through only)
 */

#ifndef SOULS_FORMATS_SF_TPF_H
#define SOULS_FORMATS_SF_TPF_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Platform enum
 *
 * Values match upstream `TPF.TPFPlatform` (TPF.cs:476). The on-disk byte
 * is identical for PC/Xbox360/PS3/PS4/Xbone/PS5; Switch is encoded as PS4 (4)
 * and disambiguated heuristically. v1 implements the PC code path fully and
 * ingests metadata for the others.
 *===========================================================================*/
typedef enum sf_tpf_platform {
    SF_TPF_PLATFORM_PC      = 0,
    SF_TPF_PLATFORM_XBOX360 = 1,
    SF_TPF_PLATFORM_PS3     = 2,
    SF_TPF_PLATFORM_PS4     = 4,
    SF_TPF_PLATFORM_XBOX1   = 5,
    SF_TPF_PLATFORM_PS5     = 8,
    SF_TPF_PLATFORM_UNKNOWN = 0xFF,
} sf_tpf_platform_t;

typedef struct sf_tpf         sf_tpf_t;
typedef struct sf_tpf_texture sf_tpf_texture_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/** Create an empty TPF configured for PC, encoding=1 (UTF-16), flag2=3
 *  (matches upstream's parameterless `new TPF()` constructor). */
SF_API sf_result_t sf_tpf_create (sf_tpf_t **out, const sf_allocator_t *a);

/** Free the TPF and every texture buffer it owns. NULL-safe. */
SF_API void        sf_tpf_destroy(sf_tpf_t *b);

/*===========================================================================
 * Read / Write
 *===========================================================================*/

SF_API sf_result_t sf_tpf_read_from_path  (sf_tpf_t **out, const wchar_t *path,
                                           const sf_allocator_t *a);
SF_API sf_result_t sf_tpf_read_from_memory(sf_tpf_t **out, const uint8_t *data,
                                           size_t size, const sf_allocator_t *a);

SF_API sf_result_t sf_tpf_write_to_path  (const sf_tpf_t *b, const wchar_t *path);
SF_API sf_result_t sf_tpf_write_to_memory(const sf_tpf_t *b, uint8_t **out,
                                          size_t *out_size, const sf_allocator_t *a);

/*===========================================================================
 * Texture list accessors
 *===========================================================================*/

SF_API size_t                  sf_tpf_texture_count(const sf_tpf_t *b);
SF_API const sf_tpf_texture_t *sf_tpf_get_texture  (const sf_tpf_t *b, size_t idx);

/** Append a copy of `tex` to the texture list. The TPF deep-copies the
 *  texture's name and bytes; caller buffers may be freed afterwards. */
SF_API sf_result_t sf_tpf_add_texture  (sf_tpf_t *b, const sf_tpf_texture_t *tex);

/** Remove the texture at `idx`. Returns SF_ERR_OUT_OF_RANGE if absent. */
SF_API sf_result_t sf_tpf_remove_texture(sf_tpf_t *b, size_t idx);

/*===========================================================================
 * TPF properties
 *===========================================================================*/

SF_API sf_tpf_platform_t sf_tpf_get_platform(const sf_tpf_t *b);
SF_API uint8_t           sf_tpf_get_encoding(const sf_tpf_t *b);
SF_API uint8_t           sf_tpf_get_flag2   (const sf_tpf_t *b);

SF_API void              sf_tpf_set_platform(sf_tpf_t *b, sf_tpf_platform_t p);
SF_API void              sf_tpf_set_encoding(sf_tpf_t *b, uint8_t enc);
SF_API void              sf_tpf_set_flag2   (sf_tpf_t *b, uint8_t v);

/*===========================================================================
 * Texture lifecycle (heap-owned, deep-copy semantics)
 *===========================================================================*/

/** Create an empty texture (Name=NULL, zero bytes). */
SF_API sf_result_t sf_tpf_texture_create(sf_tpf_texture_t **out,
                                         const sf_allocator_t *a);

/** Free a texture. NULL-safe. */
SF_API void        sf_tpf_texture_destroy(sf_tpf_texture_t *t);

/*===========================================================================
 * Texture accessors
 *===========================================================================*/

SF_API const char *sf_tpf_texture_get_name        (const sf_tpf_texture_t *t);
SF_API uint8_t     sf_tpf_texture_get_format      (const sf_tpf_texture_t *t);
SF_API uint8_t     sf_tpf_texture_get_flags1      (const sf_tpf_texture_t *t);
SF_API uint8_t     sf_tpf_texture_get_flags2      (const sf_tpf_texture_t *t);
SF_API uint8_t     sf_tpf_texture_get_mipmap_count(const sf_tpf_texture_t *t);
SF_API bool        sf_tpf_texture_get_cubemap     (const sf_tpf_texture_t *t);
SF_API const uint8_t *sf_tpf_texture_get_bytes    (const sf_tpf_texture_t *t,
                                                   size_t *out_size);

/*===========================================================================
 * Texture mutators
 *
 * `set_bytes` deep-copies the input buffer; caller may free immediately.
 * Passing data=NULL/size=0 clears the buffer.
 *===========================================================================*/

SF_API sf_result_t sf_tpf_texture_set_name  (sf_tpf_texture_t *t, const char *utf8);
SF_API sf_result_t sf_tpf_texture_set_bytes (sf_tpf_texture_t *t, const uint8_t *data,
                                             size_t size);
SF_API void        sf_tpf_texture_set_format       (sf_tpf_texture_t *t, uint8_t v);
SF_API void        sf_tpf_texture_set_flags1       (sf_tpf_texture_t *t, uint8_t v);
SF_API void        sf_tpf_texture_set_flags2       (sf_tpf_texture_t *t, uint8_t v);
SF_API void        sf_tpf_texture_set_mipmap_count (sf_tpf_texture_t *t, uint8_t v);
SF_API void        sf_tpf_texture_set_cubemap      (sf_tpf_texture_t *t, bool v);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_TPF_H */
