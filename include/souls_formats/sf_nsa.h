/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_nsa.h
 * @brief NSA — Dark Souls II Morpheme animation.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Morpheme/NSA/ C# sources
 *
 * NSA animation data is split across pointer-addressed static, dynamic, and
 * root-motion segments. This API preserves the full byte stream verbatim and
 * exposes only the inexpensive top-level counts needed for identification and
 * inventory.
 */

#ifndef SOULS_FORMATS_SF_NSA_H
#define SOULS_FORMATS_SF_NSA_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_nsa sf_nsa_t;

SF_API sf_result_t sf_nsa_create(sf_nsa_t **out, const sf_allocator_t *alloc);
SF_API void sf_nsa_destroy(sf_nsa_t *nsa);

SF_API sf_result_t sf_nsa_read_from_memory(sf_nsa_t **out,
                                           const void *bytes,
                                           size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_nsa_write_to_memory(const sf_nsa_t *nsa,
                                          void **out_bytes,
                                          size_t *out_size,
                                          const sf_allocator_t *alloc);
SF_API bool sf_nsa_is(const void *bytes, size_t size);

SF_API uint32_t sf_nsa_frame_count(const sf_nsa_t *nsa);
SF_API uint32_t sf_nsa_static_translation_count(const sf_nsa_t *nsa);
SF_API uint32_t sf_nsa_static_rotation_count(const sf_nsa_t *nsa);
SF_API uint32_t sf_nsa_dynamic_translation_count(const sf_nsa_t *nsa);
SF_API uint32_t sf_nsa_dynamic_rotation_count(const sf_nsa_t *nsa);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_NSA_H */
