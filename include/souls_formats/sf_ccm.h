/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — CCM (font layout) public surface.
 *
 * A font layout file used in DeS, DS1, DS2, and DS3; determines the texture
 * used for each different character code.
 *
 * Three on-disk variants exist, switched on the leading version word:
 *   - DemonsSouls (0x100):    big-endian; code groups; UVs as floats.
 *   - DarkSouls1  (0x10001):  little-endian; code groups; UVs as floats.
 *   - DarkSouls2  (0x20000):  little-endian; texture regions; UVs derived
 *                             from (x1,y1,x2,y2) integer pixel coords.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/CCM.cs
 */

#ifndef SOULS_FORMATS_SF_CCM_H
#define SOULS_FORMATS_SF_CCM_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_ccm_version {
    SF_CCM_VERSION_DEMONS_SOULS = 0x100,
    SF_CCM_VERSION_DARK_SOULS_1 = 0x10001,
    SF_CCM_VERSION_DARK_SOULS_2 = 0x20000
} sf_ccm_version_t;

_Static_assert(SF_CCM_VERSION_DEMONS_SOULS == 0x100,    "CCM version drift");
_Static_assert(SF_CCM_VERSION_DARK_SOULS_1 == 0x10001,  "CCM version drift");
_Static_assert(SF_CCM_VERSION_DARK_SOULS_2 == 0x20000,  "CCM version drift");

typedef struct sf_ccm sf_ccm_t;

typedef struct sf_ccm_glyph {
    int32_t code;
    float uv1_x, uv1_y;
    float uv2_x, uv2_y;
    int16_t pre_space;
    int16_t width;
    int16_t advance;
    int16_t tex_index;
} sf_ccm_glyph_t;

SF_API sf_result_t sf_ccm_create(sf_ccm_t **out, sf_ccm_version_t version,
                                 const sf_allocator_t *alloc);
SF_API void sf_ccm_destroy(sf_ccm_t *ccm);

SF_API sf_result_t sf_ccm_read_from_memory(sf_ccm_t **out, const void *bytes, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_ccm_write_to_memory(const sf_ccm_t *ccm, void **out_bytes,
                                          size_t *out_size, const sf_allocator_t *alloc);

SF_API sf_ccm_version_t sf_ccm_version(const sf_ccm_t *ccm);

SF_API int16_t sf_ccm_full_width(const sf_ccm_t *ccm);
SF_API void    sf_ccm_set_full_width(sf_ccm_t *ccm, int16_t v);

SF_API int16_t sf_ccm_tex_width(const sf_ccm_t *ccm);
SF_API void    sf_ccm_set_tex_width(sf_ccm_t *ccm, int16_t v);

SF_API int16_t sf_ccm_tex_height(const sf_ccm_t *ccm);
SF_API void    sf_ccm_set_tex_height(sf_ccm_t *ccm, int16_t v);

SF_API int16_t sf_ccm_unk0e(const sf_ccm_t *ccm);
SF_API void    sf_ccm_set_unk0e(sf_ccm_t *ccm, int16_t v);

SF_API uint8_t sf_ccm_unk1c(const sf_ccm_t *ccm);
SF_API void    sf_ccm_set_unk1c(sf_ccm_t *ccm, uint8_t v);

SF_API uint8_t sf_ccm_unk1d(const sf_ccm_t *ccm);
SF_API void    sf_ccm_set_unk1d(sf_ccm_t *ccm, uint8_t v);

SF_API uint8_t sf_ccm_tex_count(const sf_ccm_t *ccm);
SF_API void    sf_ccm_set_tex_count(sf_ccm_t *ccm, uint8_t v);

SF_API size_t sf_ccm_glyph_count(const sf_ccm_t *ccm);
SF_API sf_result_t sf_ccm_get_glyph(const sf_ccm_t *ccm, size_t index, sf_ccm_glyph_t *out);
SF_API sf_result_t sf_ccm_set_glyph(sf_ccm_t *ccm, sf_ccm_glyph_t glyph);
SF_API sf_result_t sf_ccm_find_glyph(const sf_ccm_t *ccm, int32_t code, sf_ccm_glyph_t *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_CCM_H */
