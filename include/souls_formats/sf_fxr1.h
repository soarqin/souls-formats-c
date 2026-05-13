/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_fxr1.h
 * @brief FXR1 — Dark Souls / Dark Souls Remastered particle effects.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FXR1/ (all C# files)
 *
 * FXR1 stores a pointer-addressed graph of FXNode / FXActionData / FXField /
 * FXModifier objects. The graph has many legacy-only concrete node types, so
 * this public surface intentionally preserves the graph and metadata tables as
 * opaque binary sections while exposing the file-level header fields that
 * upstream represents directly on FXR1.
 */

#ifndef SOULS_FORMATS_SF_FXR1_H
#define SOULS_FORMATS_SF_FXR1_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_fxr1 sf_fxr1_t;

SF_API sf_result_t sf_fxr1_create(sf_fxr1_t **out, const sf_allocator_t *alloc);
SF_API void sf_fxr1_destroy(sf_fxr1_t *fxr);

SF_API bool sf_fxr1_is(const void *bytes, size_t size);

SF_API sf_result_t sf_fxr1_read_from_memory(const void *bytes, size_t size,
                                            sf_fxr1_t **out,
                                            const sf_allocator_t *alloc);
SF_API sf_result_t sf_fxr1_write_to_memory(const sf_fxr1_t *fxr,
                                           void **out_bytes,
                                           size_t *out_size,
                                           const sf_allocator_t *alloc);

SF_API bool sf_fxr1_big_endian(const sf_fxr1_t *fxr);
SF_API bool sf_fxr1_wide(const sf_fxr1_t *fxr);
SF_API int32_t sf_fxr1_unk1(const sf_fxr1_t *fxr);
SF_API int32_t sf_fxr1_unk2(const sf_fxr1_t *fxr);

SF_API void sf_fxr1_set_big_endian(sf_fxr1_t *fxr, bool big_endian);
SF_API void sf_fxr1_set_wide(sf_fxr1_t *fxr, bool wide);
SF_API void sf_fxr1_set_unk1(sf_fxr1_t *fxr, int32_t unk1);
SF_API void sf_fxr1_set_unk2(sf_fxr1_t *fxr, int32_t unk2);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FXR1_H */
