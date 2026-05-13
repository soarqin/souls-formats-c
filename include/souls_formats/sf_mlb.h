/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MLB (3D resource list) public surface.
 *
 * Two variants:
 *   MLB_AC4 — used in AC4 and ACFA (ResourceType: Model=3, Texture=4)
 *   MLB_AC5 — used in ACV and ACVD (ResourceType: Model=4 only)
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/MLB/MLB_AC4.cs
 *   SoulsFormats/Formats/MLB/MLB_AC5.cs
 *   SoulsFormats/Formats/MLB/IMLB.cs
 *   SoulsFormats/Formats/MLB/IMlbResource.cs
 */

#ifndef SOULS_FORMATS_SF_MLB_H
#define SOULS_FORMATS_SF_MLB_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_mlb_ac4 sf_mlb_ac4_t;
typedef struct sf_mlb_ac5 sf_mlb_ac5_t;

typedef enum sf_mlb_ac4_resource_type {
    SF_MLB_AC4_RESOURCE_MODEL   = 3,
    SF_MLB_AC4_RESOURCE_TEXTURE = 4,
} sf_mlb_ac4_resource_type_t;
_Static_assert(SF_MLB_AC4_RESOURCE_MODEL == 3, "MLB_AC4 ResourceType drift (Model)");
_Static_assert(SF_MLB_AC4_RESOURCE_TEXTURE == 4, "MLB_AC4 ResourceType drift (Texture)");

typedef enum sf_mlb_ac5_resource_type {
    SF_MLB_AC5_RESOURCE_MODEL = 4,
} sf_mlb_ac5_resource_type_t;
_Static_assert(SF_MLB_AC5_RESOURCE_MODEL == 4, "MLB_AC5 ResourceType drift (Model)");

SF_API sf_result_t sf_mlb_ac4_create(sf_mlb_ac4_t **out, sf_mlb_ac4_resource_type_t type,
                                     bool is_animation, const sf_allocator_t *alloc);
SF_API void sf_mlb_ac4_destroy(sf_mlb_ac4_t *mlb);

SF_API sf_result_t sf_mlb_ac4_read_from_memory(sf_mlb_ac4_t **out, const void *bytes,
                                               size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_mlb_ac4_write_to_memory(const sf_mlb_ac4_t *mlb, void **out_bytes,
                                              size_t *out_size, const sf_allocator_t *alloc);

SF_API sf_mlb_ac4_resource_type_t sf_mlb_ac4_resource_type(const sf_mlb_ac4_t *mlb);
SF_API bool sf_mlb_ac4_is_animation(const sf_mlb_ac4_t *mlb);
SF_API size_t sf_mlb_ac4_resource_count(const sf_mlb_ac4_t *mlb);

SF_API sf_result_t sf_mlb_ac5_create(sf_mlb_ac5_t **out, sf_mlb_ac5_resource_type_t type,
                                     const sf_allocator_t *alloc);
SF_API void sf_mlb_ac5_destroy(sf_mlb_ac5_t *mlb);

SF_API sf_result_t sf_mlb_ac5_read_from_memory(sf_mlb_ac5_t **out, const void *bytes,
                                               size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_mlb_ac5_write_to_memory(const sf_mlb_ac5_t *mlb, void **out_bytes,
                                              size_t *out_size, const sf_allocator_t *alloc);

SF_API sf_mlb_ac5_resource_type_t sf_mlb_ac5_resource_type(const sf_mlb_ac5_t *mlb);
SF_API size_t sf_mlb_ac5_resource_count(const sf_mlb_ac5_t *mlb);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MLB_H */
