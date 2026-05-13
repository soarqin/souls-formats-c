/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_nmb.h
 * @brief NMB — Dark Souls II Morpheme bundle container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Morpheme/NMB.cs
 *   SoulsFormats/Formats/Morpheme/MorphemeBundle/ C# sources
 *
 * NMB stores a sequence of Morpheme bundle records. The individual bundle
 * payloads are pointer-addressed Morpheme runtime structures, so this API keeps
 * each bundle payload opaque while exposing the top-level bundle type list.
 */

#ifndef SOULS_FORMATS_SF_NMB_H
#define SOULS_FORMATS_SF_NMB_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_nmb_bundle_type {
    SF_NMB_BUNDLE_INVALID = -1,
    SF_NMB_BUNDLE_SKELETON = 1,
    SF_NMB_BUNDLE_SKELETON_TO_ANIM_MAP = 2,
    SF_NMB_BUNDLE_DISCRETE_EVENT_TRACK = 3,
    SF_NMB_BUNDLE_DURATION_EVENT_TRACK = 4,
    SF_NMB_BUNDLE_CURVE_EVENT_TRACK = 5,
    SF_NMB_BUNDLE_CHARACTER_CONTROLLER_DEF = 7,
    SF_NMB_BUNDLE_NETWORK = 10,
    SF_NMB_BUNDLE_FILE_HEADER = 12,
    SF_NMB_BUNDLE_FILE_NAME_LOOKUP_TABLE = 13,
} sf_nmb_bundle_type_t;

_Static_assert((uint32_t)SF_NMB_BUNDLE_FILE_NAME_LOOKUP_TABLE == 13u,
               "sf_nmb_bundle_type_t drifted from upstream eBundleType");
_Static_assert((uint32_t)SF_NMB_BUNDLE_INVALID == UINT32_MAX,
               "sf_nmb_bundle_type_t invalid value drifted from upstream eBundleType");

typedef struct sf_nmb sf_nmb_t;
typedef struct sf_nmb_bundle sf_nmb_bundle_t;

SF_API sf_result_t sf_nmb_create(sf_nmb_t **out, const sf_allocator_t *alloc);
SF_API void sf_nmb_destroy(sf_nmb_t *nmb);

SF_API sf_result_t sf_nmb_read_from_memory(sf_nmb_t **out,
                                           const void *bytes,
                                           size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_nmb_write_to_memory(const sf_nmb_t *nmb,
                                          void **out_bytes,
                                          size_t *out_size,
                                          const sf_allocator_t *alloc);
SF_API bool sf_nmb_is(const void *bytes, size_t size);

SF_API size_t sf_nmb_bundle_count(const sf_nmb_t *nmb);
SF_API sf_nmb_bundle_t *sf_nmb_bundle_at(const sf_nmb_t *nmb, size_t i);
SF_API sf_nmb_bundle_type_t sf_nmb_bundle_type(const sf_nmb_bundle_t *bundle);
SF_API const void *sf_nmb_bundle_data(const sf_nmb_bundle_t *bundle, size_t *out_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_NMB_H */
