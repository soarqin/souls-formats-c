/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — DRB (Dialogue/Resource Block) public surface.
 *
 * UI configuration format used by FromSoftware titles up to Dark Souls 2,
 * including Armored Core For Answer, Demon's Souls, Dark Souls, and Dark
 * Souls Remastered. The full upstream implementation parses 18+ blocks
 * (STR, TEXI, SHPR, CTPR, ANIP, INTP, SCDP, SHAP, CTRL, ANIK, ANIO, ANIM,
 * SCDK, SCDO, SCDL, DLGO, DLG, plus DRB/END null blocks).
 *
 * This C port is **read-only** and intentionally simplified: it parses the
 * file envelope (magic + endian detection), the STR string table and the
 * TEXI texture list. All other blocks are header-validated then skipped so
 * the file may be classified and its texture/dialog inventory queried
 * without modelling the full UI graph.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/DRB/DRB.cs
 *   SoulsFormats/Formats/DRB/Texture.cs
 *   SoulsFormats/Formats/DRB/Dlg.cs
 */

#ifndef SOULS_FORMATS_SF_DRB_H
#define SOULS_FORMATS_SF_DRB_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supported on-disk DRB layouts. The version is not stored in the file,
 * the caller must select it based on the source game. Mirrors upstream
 * DRB.DRBVersion. */
typedef enum sf_drb_version {
    SF_DRB_VERSION_ARMORED_CORE_FOR_ANSWER = 0,
    SF_DRB_VERSION_DARK_SOULS              = 1,
    SF_DRB_VERSION_DARK_SOULS_REMASTERED   = 2,
} sf_drb_version_t;

typedef struct sf_drb sf_drb_t;

/* Construct an empty in-memory DRB. The instance remembers the allocator
 * and uses it for every internal allocation and the eventual destroy. */
SF_API sf_result_t sf_drb_create(sf_drb_t **out, sf_drb_version_t version,
                                 bool big_endian, const sf_allocator_t *alloc);

/* Release a DRB instance. NULL-safe. */
SF_API void sf_drb_destroy(sf_drb_t *drb);

/* Parse a DRB from a memory buffer. The buffer is read-only and may be
 * released as soon as this call returns. `version` selects the on-disk
 * layout (not encoded in the file). */
SF_API sf_result_t sf_drb_read_from_memory(sf_drb_t **out, const void *bytes,
                                           size_t size, sf_drb_version_t version,
                                           const sf_allocator_t *alloc);

/* Quick magic-only detection: true iff `bytes` starts with "DRB\0" or
 * "\0BRD" (little-/big-endian). */
SF_API bool sf_drb_is(const void *bytes, size_t size);

/* Format-level accessors. */
SF_API sf_drb_version_t sf_drb_version(const sf_drb_t *drb);
SF_API bool             sf_drb_big_endian(const sf_drb_t *drb);

/* Texture names declared in the TEXI block. `out_name` points to a
 * NUL-terminated UTF-8 string owned by `drb`; it remains valid until
 * sf_drb_destroy() is called. */
SF_API size_t      sf_drb_texture_count(const sf_drb_t *drb);
SF_API sf_result_t sf_drb_get_texture_name(const sf_drb_t *drb, size_t index,
                                           const char **out_name);

/* Dialog (DLG) names. Same ownership rules as texture names. */
SF_API size_t      sf_drb_dlg_count(const sf_drb_t *drb);
SF_API sf_result_t sf_drb_get_dlg_name(const sf_drb_t *drb, size_t index,
                                       const char **out_name);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_DRB_H */
