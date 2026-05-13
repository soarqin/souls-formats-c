/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TAE (Time Act Editor) public surface.
 *
 * TAE controls when timed events fire during character / object / map animations.
 * v1 scope: SDT format only (version 0x1000D), covering Sekiro + Elden Ring +
 * Elden Ring Nightreign + Armored Core VI. Older formats (DS1/SOTFS/DS3/BB/DES/
 * DESR) are part of v2; their enumerators are exposed so saved files can be
 * detected and rejected with SF_ERR_UNSUPPORTED_VERSION rather than silently
 * mis-parsed.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/TAE/TAE.cs
 *   SoulsFormats/Formats/TAE/Animation.cs
 *   SoulsFormats/Formats/TAE/Event.cs
 *   SoulsFormats/Formats/TAE/EventGroup.cs
 *
 * Design adaptations from upstream (see docs/api-mapping/extensions.md):
 *   - The upstream per-event-type schema subsystem is OUT-of-scope for v1.
 *     Event parameters are therefore exposed as raw little-endian byte
 *     payloads; callers that need typed field access wire up their own
 *     interpretation layer on top of these bytes.
 *   - AnimMiniHeader is represented as a tagged union (the upstream abstract
 *     class hierarchy with Standard + ImportOtherAnim subclasses).
 *   - BigEndian byte switching is NOT exposed: SDT files are always 64-bit LE.
 */

#ifndef SOULS_FORMATS_SF_TAE_H
#define SOULS_FORMATS_SF_TAE_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Opaque forward declarations
 *
 * All TAE aggregate types are opaque to public API consumers; access is via
 * the `sf_tae_*` accessor functions further down this header.
 *===========================================================================*/
typedef struct sf_tae             sf_tae_t;
typedef struct sf_tae_animation   sf_tae_animation_t;
typedef struct sf_tae_event       sf_tae_event_t;
typedef struct sf_tae_event_group sf_tae_event_group_t;
typedef struct sf_tae_template    sf_tae_template_t;

/*===========================================================================
 * TAEFormat — mirrors upstream TAE.cs:20-51 (`public enum TAEFormat`).
 *
 * Only SDT (Sekiro + Elden Ring family) is implemented in v1; the other
 * enumerators are reserved so the reader can produce a stable diagnostic on
 * unsupported files and so callers can pattern-match on the result.
 *===========================================================================*/
typedef enum sf_tae_format {
    SF_TAE_FORMAT_DS1   = 0, /* v2 — Dark Souls 1 */
    SF_TAE_FORMAT_SOTFS = 1, /* v2 — Dark Souls II: Scholar of the First Sin */
    SF_TAE_FORMAT_DS3   = 2, /* v2 — Dark Souls III (alias: Bloodborne) */
    SF_TAE_FORMAT_SDT   = 3, /* v1 IN-scope — Sekiro + Elden Ring + Nightreign + AC6 */
    SF_TAE_FORMAT_DES   = 4, /* v2 — Demon's Souls */
    SF_TAE_FORMAT_DESR  = 5, /* v2 — Demon's Souls Remastered */
} sf_tae_format_t;

_Static_assert(SF_TAE_FORMAT_DS1 == 0, "TAEFormat drift (DS1)");
_Static_assert(SF_TAE_FORMAT_SDT == 3, "TAEFormat drift (SDT)");
_Static_assert(SF_TAE_FORMAT_DESR == 5, "TAEFormat drift (DESR)");

/*===========================================================================
 * AnimMiniHeader type discriminator — mirrors Animation.cs:15-26
 * (`public enum MiniHeaderType : uint`).
 *===========================================================================*/
typedef enum sf_tae_anim_mini_header_type {
    SF_TAE_MINI_HEADER_STANDARD          = 0,
    SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM = 1,
} sf_tae_anim_mini_header_type_t;

_Static_assert(SF_TAE_MINI_HEADER_STANDARD == 0, "MiniHeaderType drift (STANDARD)");
_Static_assert(SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM == 1,
               "MiniHeaderType drift (IMPORT_OTHER_ANIM)");

/*===========================================================================
 * AnimMiniHeader payload — POD value types for the two known variants.
 *
 * Standard (Animation.cs:51-147): three boolean flags plus an optional HKX
 * source animation id. When `imports_hkx` is true the engine pulls motion
 * data from the animation whose id equals `import_hkx_source_anim_id`.
 *
 * ImportOtherAnim (Animation.cs:152-217): wholesale import of motion data
 * + events from another animation. `unknown` defaults to -1 in upstream.
 *===========================================================================*/
typedef struct sf_tae_anim_mini_header_standard {
    bool    is_loop_by_default;
    bool    allow_delay_load;
    bool    imports_hkx;
    int32_t import_hkx_source_anim_id;
} sf_tae_anim_mini_header_standard_t;

typedef struct sf_tae_anim_mini_header_import {
    int32_t import_from_anim_id;
    int32_t unknown; /* default -1 in upstream */
} sf_tae_anim_mini_header_import_t;

/*===========================================================================
 * AnimMiniHeader tagged union.
 *
 * `type` selects which arm of `payload` is live. `is_null_header` mirrors
 * upstream `AnimMiniHeader.IsNullHeader` and indicates the entry was present
 * on disk but unused; callers should treat it as a no-op header.
 *===========================================================================*/
typedef struct sf_tae_anim_mini_header {
    sf_tae_anim_mini_header_type_t type;
    bool                           is_null_header;
    union {
        sf_tae_anim_mini_header_standard_t standard;
        sf_tae_anim_mini_header_import_t   import_other;
    } payload;
} sf_tae_anim_mini_header_t;

/*===========================================================================
 * TAE root API
 *===========================================================================*/

/* Read a TAE blob from an in-memory buffer. The caller retains ownership of
 * `bytes`. On success, `*out` is a heap-allocated handle to be destroyed
 * via sf_tae_destroy(). */
SF_API sf_result_t sf_tae_read_from_memory(sf_tae_t **out, const void *bytes, size_t size,
                                           const sf_allocator_t *a);

/* Serialise a TAE handle back to a freshly-allocated buffer owned by the
 * caller (free via sf_free(a, *out_bytes)). */
SF_API sf_result_t sf_tae_write_to_memory(const sf_tae_t *t, void **out_bytes,
                                          size_t *out_size, const sf_allocator_t *a);

/* Release a TAE handle and every Animation / Event / EventGroup it owns. */
SF_API void sf_tae_destroy(sf_tae_t *t);

/* Detected format of the file this handle was loaded from. v1 only ever
 * produces SF_TAE_FORMAT_SDT on read; other values are reserved. */
SF_API sf_tae_format_t sf_tae_format(const sf_tae_t *t);

/* TAE.ID — file identifier. */
SF_API int32_t sf_tae_id(const sf_tae_t *t);

/* TAE.SkeletonName — UTF-8, owned by the handle. Stable until destroy. */
SF_API const char *sf_tae_skeleton_name(const sf_tae_t *t);

/* TAE.SibName — UTF-8, owned by the handle. Stable until destroy. */
SF_API const char *sf_tae_sib_name(const sf_tae_t *t);

/* TAE.EventBank — selects which event-template bank this TAE refers to.
 * Returned as int64 because upstream stores it as `long`. */
SF_API int64_t sf_tae_event_bank(const sf_tae_t *t);

/* Animation list accessors. */
SF_API size_t                    sf_tae_animation_count(const sf_tae_t *t);
SF_API const sf_tae_animation_t *sf_tae_animation(const sf_tae_t *t, size_t i);

/*===========================================================================
 * Animation accessors
 *===========================================================================*/

/* Animation.ID — 64-bit identifier (upstream `long`). */
SF_API int64_t sf_tae_animation_id(const sf_tae_animation_t *a);

/* Animation.MiniHeader — pointer is stable for the lifetime of the parent
 * TAE handle. */
SF_API const sf_tae_anim_mini_header_t *
sf_tae_animation_mini_header(const sf_tae_animation_t *a);

/* Animation.Events list accessors. */
SF_API size_t                sf_tae_animation_event_count(const sf_tae_animation_t *a);
SF_API const sf_tae_event_t *sf_tae_animation_event(const sf_tae_animation_t *a, size_t i);

/* Animation.EventGroups list accessors. */
SF_API size_t sf_tae_animation_event_group_count(const sf_tae_animation_t *a);
SF_API const sf_tae_event_group_t *
sf_tae_animation_event_group(const sf_tae_animation_t *a, size_t i);

/*===========================================================================
 * Event accessors
 *
 * Event parameters are exposed as raw little-endian byte payloads. A typed
 * accessor layer that would interpret the payload as named fields is
 * intentionally OUT-of-scope for v1.
 *===========================================================================*/

/* Event.StartTime / Event.EndTime — seconds, single precision. */
SF_API float sf_tae_event_start_time(const sf_tae_event_t *e);
SF_API float sf_tae_event_end_time(const sf_tae_event_t *e);

/* Event.Type — event-type integer; semantics depend on EventBank. */
SF_API int32_t sf_tae_event_type(const sf_tae_event_t *e);

/* Event.ParameterBytes — pointer + size to the raw parameter blob. The
 * returned pointer is owned by the parent TAE handle and stable until
 * destroy; `*out_size` receives the byte count. */
SF_API const uint8_t *sf_tae_event_parameters(const sf_tae_event_t *e, size_t *out_size);

/*===========================================================================
 * EventGroup accessors
 *
 * Members are stored as 32-bit indices back into the parent Animation's
 * Events list (upstream `EventGroup.indices`).
 *===========================================================================*/

/* EventGroup.GroupType — see upstream EventGroup.cs:13-58 for known
 * meanings on SDT (0 / 16 / 128 / 192). */
SF_API int32_t sf_tae_event_group_type(const sf_tae_event_group_t *g);

/* EventGroup membership. */
SF_API size_t  sf_tae_event_group_member_count(const sf_tae_event_group_t *g);
SF_API int32_t sf_tae_event_group_member(const sf_tae_event_group_t *g, size_t i);

/* Apply a template to this TAE, populating event parameter names and types.
 *
 * Mirrors upstream TAE.cs:117-138 `ApplyTemplate(Template template, bool strict)`.
 *
 * `validate_bank`: if true (mirrors upstream ValidateEventBank=true), the
 *   template must contain a bank matching sf_tae_event_bank(t). If false,
 *   any bank containing the event type is used.
 * `strict`: if true, returns SF_ERR_BAD_MAGIC when an event type is not
 *   found in the template. If false, silently skips unknown event types.
 *
 * Returns SF_ERR_BAD_MAGIC if:
 *   - template game != TAE format
 *   - validate_bank=true and event bank not found in template
 *   - strict=true and an event type is not found in the template
 */
SF_API sf_result_t sf_tae_apply_template(sf_tae_t *t,
                                         const sf_tae_template_t *tmpl,
                                         bool validate_bank,
                                         bool strict);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_TAE_H */
