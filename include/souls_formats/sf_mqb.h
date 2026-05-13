/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MQB (MovieSequencer Binary) public surface.
 *
 * A cutscene definition format dating back to at least Armored Core V.
 * Extension: .mqb. Big- or little-endian; version-specific header sizes
 * and varint widths (DS2Scholar uses 64-bit varints; all others use 32).
 *
 * Tree structure:
 *   MQB
 *    ├─ Resources[]          name, parent_index, path, parameters[]
 *    └─ Cuts[]               name, duration, timelines[]
 *         └─ Timelines[]     resource_index?, events[], parameters[]
 *              └─ Events[]   id, start_frame, duration, parameters[], transforms[]
 *
 * Parameters carry typed values (one of DataType variants) plus optional
 * sequences of keyed points. The wire layout interleaves header data with
 * an offset-back-patched trailer for sequences/points, so all writing goes
 * through the library; users only manipulate the in-memory tree.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/MQB/MQB.cs
 *   SoulsFormats/Formats/MQB/Resource.cs
 *   SoulsFormats/Formats/MQB/Cut.cs
 *   SoulsFormats/Formats/MQB/Timeline.cs
 *   SoulsFormats/Formats/MQB/Event.cs
 *   SoulsFormats/Formats/MQB/Transform.cs
 *   SoulsFormats/Formats/MQB/Parameter.cs
 */

#ifndef SOULS_FORMATS_SF_MQB_H
#define SOULS_FORMATS_SF_MQB_H

#include "sf_common.h"
#include "souls_formats/sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Version enum — wire uint32 in the MQB header.
 *
 * Mirrors upstream MQB.MQBVersion (MQB.cs:12-18).
 *===========================================================================*/
typedef enum sf_mqb_version {
    SF_MQB_VERSION_DARK_SOULS_2         = 0x94,
    SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR = 0xCA,
    SF_MQB_VERSION_BLOODBORNE           = 0xCB,
    SF_MQB_VERSION_DARK_SOULS_3         = 0xCC,
} sf_mqb_version_t;

_Static_assert(SF_MQB_VERSION_DARK_SOULS_2         == 0x94, "MQB version drift (DS2)");
_Static_assert(SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR == 0xCA, "MQB version drift (DS2 Scholar)");
_Static_assert(SF_MQB_VERSION_BLOODBORNE           == 0xCB, "MQB version drift (BB)");
_Static_assert(SF_MQB_VERSION_DARK_SOULS_3         == 0xCC, "MQB version drift (DS3)");

/*===========================================================================
 * Parameter data type — wire uint32 in each Parameter header.
 *
 * Mirrors upstream MQB.Parameter.DataType (Parameter.cs:17-86).
 * UShort (5) is reserved in the enum but never observed in the wild.
 *===========================================================================*/
typedef enum sf_mqb_param_type {
    SF_MQB_PARAM_TYPE_BOOL      = 1,
    SF_MQB_PARAM_TYPE_SBYTE     = 2,
    SF_MQB_PARAM_TYPE_BYTE      = 3,
    SF_MQB_PARAM_TYPE_SHORT     = 4,
    SF_MQB_PARAM_TYPE_INT       = 6,
    SF_MQB_PARAM_TYPE_UINT      = 7,
    SF_MQB_PARAM_TYPE_FLOAT     = 8,
    SF_MQB_PARAM_TYPE_STRING    = 10,
    SF_MQB_PARAM_TYPE_CUSTOM    = 11,
    SF_MQB_PARAM_TYPE_COLOR     = 13,
    SF_MQB_PARAM_TYPE_INT_COLOR = 17,
    SF_MQB_PARAM_TYPE_VECTOR    = 18,
} sf_mqb_param_type_t;

typedef sf_mqb_param_type_t sf_mqb_data_type_t;

#define SF_MQB_DATA_TYPE_BOOL      SF_MQB_PARAM_TYPE_BOOL
#define SF_MQB_DATA_TYPE_SBYTE     SF_MQB_PARAM_TYPE_SBYTE
#define SF_MQB_DATA_TYPE_BYTE      SF_MQB_PARAM_TYPE_BYTE
#define SF_MQB_DATA_TYPE_SHORT     SF_MQB_PARAM_TYPE_SHORT
#define SF_MQB_DATA_TYPE_INT       SF_MQB_PARAM_TYPE_INT
#define SF_MQB_DATA_TYPE_UINT      SF_MQB_PARAM_TYPE_UINT
#define SF_MQB_DATA_TYPE_FLOAT     SF_MQB_PARAM_TYPE_FLOAT
#define SF_MQB_DATA_TYPE_STRING    SF_MQB_PARAM_TYPE_STRING
#define SF_MQB_DATA_TYPE_CUSTOM    SF_MQB_PARAM_TYPE_CUSTOM
#define SF_MQB_DATA_TYPE_COLOR     SF_MQB_PARAM_TYPE_COLOR
#define SF_MQB_DATA_TYPE_INT_COLOR SF_MQB_PARAM_TYPE_INT_COLOR
#define SF_MQB_DATA_TYPE_VECTOR    SF_MQB_PARAM_TYPE_VECTOR

_Static_assert(SF_MQB_PARAM_TYPE_BOOL      == 1,  "MQB DataType drift (Bool)");
_Static_assert(SF_MQB_PARAM_TYPE_SBYTE     == 2,  "MQB DataType drift (SByte)");
_Static_assert(SF_MQB_PARAM_TYPE_BYTE      == 3,  "MQB DataType drift (Byte)");
_Static_assert(SF_MQB_PARAM_TYPE_SHORT     == 4,  "MQB DataType drift (Short)");
_Static_assert(SF_MQB_PARAM_TYPE_INT       == 6,  "MQB DataType drift (Int)");
_Static_assert(SF_MQB_PARAM_TYPE_UINT      == 7,  "MQB DataType drift (UInt)");
_Static_assert(SF_MQB_PARAM_TYPE_FLOAT     == 8,  "MQB DataType drift (Float)");
_Static_assert(SF_MQB_PARAM_TYPE_STRING    == 10, "MQB DataType drift (String)");
_Static_assert(SF_MQB_PARAM_TYPE_CUSTOM    == 11, "MQB DataType drift (Custom)");
_Static_assert(SF_MQB_PARAM_TYPE_COLOR     == 13, "MQB DataType drift (Color)");
_Static_assert(SF_MQB_PARAM_TYPE_INT_COLOR == 17, "MQB DataType drift (IntColor)");
_Static_assert(SF_MQB_PARAM_TYPE_VECTOR    == 18, "MQB DataType drift (Vector)");

/*===========================================================================
 * sf_mqb_transform_t — POD for the per-event keyframe transform.
 *
 * Mirrors upstream MQB.Transform (Transform.cs:8-62). One float frame
 * stamp plus nine Vector3 fields (translation/rotation/scale and six
 * unknown auxiliary vectors). 1 + 27 = 28 floats = 112 bytes.
 *===========================================================================*/
typedef struct sf_mqb_transform {
    float     frame;
    sf_vec3_t translation;
    sf_vec3_t unk10;
    sf_vec3_t unk1c;
    sf_vec3_t rotation;
    sf_vec3_t unk34;
    sf_vec3_t unk40;
    sf_vec3_t scale;
    sf_vec3_t unk58;
    sf_vec3_t unk64;
} sf_mqb_transform_t;

_Static_assert(sizeof(sf_mqb_transform_t) == 112,
               "sf_mqb_transform_t layout mismatch (expected 112 bytes)");

/*===========================================================================
 * Opaque forward declarations
 *===========================================================================*/
typedef struct sf_mqb           sf_mqb_t;
typedef struct sf_mqb_resource  sf_mqb_resource_t;
typedef struct sf_mqb_cut       sf_mqb_cut_t;
typedef struct sf_mqb_timeline  sf_mqb_timeline_t;
typedef struct sf_mqb_event     sf_mqb_event_t;
typedef struct sf_mqb_parameter sf_mqb_parameter_t;
typedef struct sf_mqb_sequence  sf_mqb_sequence_t;
typedef struct sf_mqb_point     sf_mqb_point_t;

/*===========================================================================
 * Lifecycle and I/O
 *===========================================================================*/

/* Allocate an empty MQB. @version selects the on-wire encoding; @big_endian
 * controls the endian marker (note: DS2Scholar is little-endian only,
 * upstream applies no constraint on the others). */
SF_API sf_result_t sf_mqb_create(sf_mqb_t **out, sf_mqb_version_t version,
                                 bool big_endian, const sf_allocator_t *alloc);

/* Free an MQB and every nested object it owns. NULL-safe. */
SF_API void sf_mqb_destroy(sf_mqb_t *m);

/* Parse an MQB from an in-memory buffer. Endian is detected from the wire
 * marker byte; version selects the rest of the header layout. */
SF_API sf_result_t sf_mqb_read_from_memory(sf_mqb_t **out, const void *bytes,
                                           size_t size, const sf_allocator_t *alloc);

/* Serialise to a fresh heap-allocated buffer. Caller frees with
 * sf_free(alloc, *out_bytes). */
SF_API sf_result_t sf_mqb_write_to_memory(const sf_mqb_t *m, void **out_bytes,
                                          size_t *out_size,
                                          const sf_allocator_t *alloc);

/* Returns true if @bytes starts with the four-byte ASCII magic "MQB ".
 * At least 4 bytes are required, mirroring upstream's Is() guard. */
SF_API bool sf_mqb_is(const void *bytes, size_t size);

/*===========================================================================
 * MQB-level accessors
 *===========================================================================*/
SF_API sf_mqb_version_t sf_mqb_version    (const sf_mqb_t *m);
SF_API void             sf_mqb_set_version(sf_mqb_t *m, sf_mqb_version_t v);
SF_API bool             sf_mqb_big_endian (const sf_mqb_t *m);
SF_API void             sf_mqb_set_big_endian(sf_mqb_t *m, bool be);

/* Returns a borrowed UTF-8 pointer (never NULL — empty MQB has ""). */
SF_API const char *sf_mqb_name(const sf_mqb_t *m);
SF_API sf_result_t sf_mqb_set_name(sf_mqb_t *m, const char *utf8);

SF_API float       sf_mqb_framerate(const sf_mqb_t *m);
SF_API void        sf_mqb_set_framerate(sf_mqb_t *m, float v);

/* Optional resource directory string (UTF-16 on the wire, UTF-8 on this
 * boundary). May be empty. Borrowed pointer. */
SF_API const char *sf_mqb_resource_directory(const sf_mqb_t *m);
SF_API sf_result_t sf_mqb_set_resource_directory(sf_mqb_t *m, const char *utf8);

/*===========================================================================
 * Resource collection
 *===========================================================================*/
SF_API size_t              sf_mqb_resource_count(const sf_mqb_t *m);
SF_API sf_mqb_resource_t  *sf_mqb_resource_at   (const sf_mqb_t *m, size_t i);
SF_API sf_result_t         sf_mqb_add_resource  (sf_mqb_t *m,
                                                 sf_mqb_resource_t **out);

SF_API const char *sf_mqb_resource_name        (const sf_mqb_resource_t *r);
SF_API sf_result_t sf_mqb_resource_set_name    (sf_mqb_resource_t *r,
                                                const char *utf8);
SF_API int32_t     sf_mqb_resource_parent_index(const sf_mqb_resource_t *r);
SF_API void        sf_mqb_resource_set_parent_index(sf_mqb_resource_t *r, int32_t v);
SF_API int32_t     sf_mqb_resource_unk48       (const sf_mqb_resource_t *r);
SF_API void        sf_mqb_resource_set_unk48   (sf_mqb_resource_t *r, int32_t v);

/* Resource path (UTF-16 on the wire). NULL means "no path entry" — its slot
 * is recorded as offset 0 in the path table. */
SF_API const char *sf_mqb_resource_path        (const sf_mqb_resource_t *r);
SF_API sf_result_t sf_mqb_resource_set_path    (sf_mqb_resource_t *r,
                                                const char *utf8_or_null);

SF_API size_t               sf_mqb_resource_parameter_count(const sf_mqb_resource_t *r);
SF_API sf_mqb_parameter_t  *sf_mqb_resource_parameter_at   (const sf_mqb_resource_t *r,
                                                            size_t i);
SF_API sf_result_t          sf_mqb_resource_add_parameter  (sf_mqb_resource_t *r,
                                                            sf_mqb_parameter_t **out);

/*===========================================================================
 * Cut collection
 *===========================================================================*/
SF_API size_t          sf_mqb_cut_count(const sf_mqb_t *m);
SF_API sf_mqb_cut_t   *sf_mqb_cut_at   (const sf_mqb_t *m, size_t i);
SF_API sf_result_t     sf_mqb_add_cut  (sf_mqb_t *m, sf_mqb_cut_t **out);

SF_API const char *sf_mqb_cut_name        (const sf_mqb_cut_t *c);
SF_API sf_result_t sf_mqb_cut_set_name    (sf_mqb_cut_t *c, const char *utf8);
SF_API int32_t     sf_mqb_cut_unk44       (const sf_mqb_cut_t *c);
SF_API void        sf_mqb_cut_set_unk44   (sf_mqb_cut_t *c, int32_t v);
SF_API int32_t     sf_mqb_cut_duration    (const sf_mqb_cut_t *c);
SF_API void        sf_mqb_cut_set_duration(sf_mqb_cut_t *c, int32_t v);

SF_API size_t              sf_mqb_cut_timeline_count(const sf_mqb_cut_t *c);
SF_API sf_mqb_timeline_t  *sf_mqb_cut_timeline_at   (const sf_mqb_cut_t *c, size_t i);
SF_API sf_result_t         sf_mqb_cut_add_timeline  (sf_mqb_cut_t *c,
                                                     sf_mqb_timeline_t **out);

/*===========================================================================
 * Timeline collection
 *===========================================================================*/
SF_API int32_t          sf_mqb_timeline_unk10    (const sf_mqb_timeline_t *t);
SF_API void             sf_mqb_timeline_set_unk10(sf_mqb_timeline_t *t, int32_t v);

SF_API size_t           sf_mqb_timeline_event_count(const sf_mqb_timeline_t *t);
SF_API sf_mqb_event_t  *sf_mqb_timeline_event_at   (const sf_mqb_timeline_t *t, size_t i);
SF_API sf_result_t      sf_mqb_timeline_add_event  (sf_mqb_timeline_t *t,
                                                    sf_mqb_event_t **out);

SF_API size_t               sf_mqb_timeline_parameter_count(const sf_mqb_timeline_t *t);
SF_API sf_mqb_parameter_t  *sf_mqb_timeline_parameter_at   (const sf_mqb_timeline_t *t,
                                                            size_t i);
SF_API sf_result_t          sf_mqb_timeline_add_parameter  (sf_mqb_timeline_t *t,
                                                            sf_mqb_parameter_t **out);

/*===========================================================================
 * Event accessors
 *===========================================================================*/
SF_API int32_t sf_mqb_event_id            (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_id        (sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_resource_index(const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_resource_index(sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_unk08         (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_unk08     (sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_start_frame   (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_start_frame(sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_duration      (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_duration  (sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_unk14         (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_unk14     (sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_unk18         (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_unk18     (sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_unk1c         (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_unk1c     (sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_unk20         (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_unk20     (sf_mqb_event_t *e, int32_t v);
SF_API int32_t sf_mqb_event_unk28         (const sf_mqb_event_t *e);
SF_API void    sf_mqb_event_set_unk28     (sf_mqb_event_t *e, int32_t v);

SF_API size_t               sf_mqb_event_parameter_count(const sf_mqb_event_t *e);
SF_API sf_mqb_parameter_t  *sf_mqb_event_parameter_at   (const sf_mqb_event_t *e,
                                                         size_t i);
SF_API sf_result_t          sf_mqb_event_add_parameter  (sf_mqb_event_t *e,
                                                         sf_mqb_parameter_t **out);

SF_API size_t      sf_mqb_event_transform_count(const sf_mqb_event_t *e);
SF_API sf_result_t sf_mqb_event_get_transform  (const sf_mqb_event_t *e, size_t i,
                                                sf_mqb_transform_t *out);
SF_API sf_result_t sf_mqb_event_add_transform  (sf_mqb_event_t *e,
                                                sf_mqb_transform_t t);

/*===========================================================================
 * Parameter accessors
 *
 * Values are typed: each setter validates the parameter's current type,
 * each getter returns SF_ERR_INVALID_ARG on type mismatch. Setting the
 * type via sf_mqb_parameter_set_type does NOT migrate the stored value —
 * callers must immediately set the value to the matching variant.
 *===========================================================================*/
SF_API const char         *sf_mqb_parameter_name        (const sf_mqb_parameter_t *p);
SF_API sf_result_t         sf_mqb_parameter_set_name    (sf_mqb_parameter_t *p,
                                                         const char *utf8);
SF_API sf_mqb_data_type_t  sf_mqb_parameter_type        (const sf_mqb_parameter_t *p);
SF_API void                sf_mqb_parameter_set_type    (sf_mqb_parameter_t *p,
                                                         sf_mqb_data_type_t t);
SF_API int32_t             sf_mqb_parameter_member_count(const sf_mqb_parameter_t *p);
SF_API void                sf_mqb_parameter_set_member_count(sf_mqb_parameter_t *p,
                                                              int32_t v);

/* Typed value setters (validate parameter type). */
SF_API sf_result_t sf_mqb_parameter_set_bool   (sf_mqb_parameter_t *p, bool v);
SF_API sf_result_t sf_mqb_parameter_set_sbyte  (sf_mqb_parameter_t *p, int8_t v);
SF_API sf_result_t sf_mqb_parameter_set_byte   (sf_mqb_parameter_t *p, uint8_t v);
SF_API sf_result_t sf_mqb_parameter_set_short  (sf_mqb_parameter_t *p, int16_t v);
SF_API sf_result_t sf_mqb_parameter_set_int    (sf_mqb_parameter_t *p, int32_t v);
SF_API sf_result_t sf_mqb_parameter_set_uint   (sf_mqb_parameter_t *p, uint32_t v);
SF_API sf_result_t sf_mqb_parameter_set_float  (sf_mqb_parameter_t *p, float v);
SF_API sf_result_t sf_mqb_parameter_set_string (sf_mqb_parameter_t *p, const char *utf8);
SF_API sf_result_t sf_mqb_parameter_set_custom (sf_mqb_parameter_t *p,
                                                const void *bytes, size_t size);
SF_API sf_result_t sf_mqb_parameter_set_color  (sf_mqb_parameter_t *p, sf_color_t c);
SF_API sf_result_t sf_mqb_parameter_set_int_color(sf_mqb_parameter_t *p,
                                                  int32_t r, int32_t g,
                                                  int32_t b, int32_t a);
SF_API sf_result_t sf_mqb_parameter_set_vec2   (sf_mqb_parameter_t *p, sf_vec2_t v);
SF_API sf_result_t sf_mqb_parameter_set_vec3   (sf_mqb_parameter_t *p, sf_vec3_t v);
SF_API sf_result_t sf_mqb_parameter_set_vec4   (sf_mqb_parameter_t *p, sf_vec4_t v);

/* Typed value getters. Return SF_ERR_INVALID_ARG if @p has a different type. */
SF_API sf_result_t sf_mqb_parameter_get_bool   (const sf_mqb_parameter_t *p, bool *out);
SF_API sf_result_t sf_mqb_parameter_get_sbyte  (const sf_mqb_parameter_t *p, int8_t *out);
SF_API sf_result_t sf_mqb_parameter_get_byte   (const sf_mqb_parameter_t *p, uint8_t *out);
SF_API sf_result_t sf_mqb_parameter_get_short  (const sf_mqb_parameter_t *p, int16_t *out);
SF_API sf_result_t sf_mqb_parameter_get_int    (const sf_mqb_parameter_t *p, int32_t *out);
SF_API sf_result_t sf_mqb_parameter_get_uint   (const sf_mqb_parameter_t *p, uint32_t *out);
SF_API sf_result_t sf_mqb_parameter_get_float  (const sf_mqb_parameter_t *p, float *out);
SF_API sf_result_t sf_mqb_parameter_get_string (const sf_mqb_parameter_t *p,
                                                const char **out_utf8);
SF_API sf_result_t sf_mqb_parameter_get_custom (const sf_mqb_parameter_t *p,
                                                const void **out_bytes,
                                                size_t *out_size);
SF_API sf_result_t sf_mqb_parameter_get_color  (const sf_mqb_parameter_t *p,
                                                sf_color_t *out);
SF_API sf_result_t sf_mqb_parameter_get_int_color(const sf_mqb_parameter_t *p,
                                                  int32_t *r, int32_t *g,
                                                  int32_t *b, int32_t *a);
SF_API sf_result_t sf_mqb_parameter_get_vec2   (const sf_mqb_parameter_t *p,
                                                sf_vec2_t *out);
SF_API sf_result_t sf_mqb_parameter_get_vec3   (const sf_mqb_parameter_t *p,
                                                sf_vec3_t *out);
SF_API sf_result_t sf_mqb_parameter_get_vec4   (const sf_mqb_parameter_t *p,
                                                sf_vec4_t *out);

/*===========================================================================
 * Sequence collection (animated parameter values)
 *===========================================================================*/
SF_API size_t              sf_mqb_parameter_sequence_count(const sf_mqb_parameter_t *p);
SF_API sf_mqb_sequence_t  *sf_mqb_parameter_sequence_at   (const sf_mqb_parameter_t *p,
                                                           size_t i);
SF_API sf_result_t         sf_mqb_parameter_add_sequence  (sf_mqb_parameter_t *p,
                                                           sf_mqb_sequence_t **out);

SF_API sf_mqb_data_type_t sf_mqb_sequence_value_type    (const sf_mqb_sequence_t *s);
SF_API void               sf_mqb_sequence_set_value_type(sf_mqb_sequence_t *s,
                                                         sf_mqb_data_type_t t);
SF_API int32_t            sf_mqb_sequence_point_type    (const sf_mqb_sequence_t *s);
SF_API void               sf_mqb_sequence_set_point_type(sf_mqb_sequence_t *s, int32_t v);
SF_API int32_t            sf_mqb_sequence_value_index   (const sf_mqb_sequence_t *s);
SF_API void               sf_mqb_sequence_set_value_index(sf_mqb_sequence_t *s, int32_t v);

SF_API size_t           sf_mqb_sequence_point_count(const sf_mqb_sequence_t *s);
SF_API sf_mqb_point_t  *sf_mqb_sequence_point_at   (const sf_mqb_sequence_t *s, size_t i);
SF_API sf_result_t      sf_mqb_sequence_add_point  (sf_mqb_sequence_t *s,
                                                    sf_mqb_point_t **out);

/* Sequence Point: typed value (one of Byte/Float/UInt) + three trailers. */
SF_API sf_result_t sf_mqb_point_set_byte (sf_mqb_point_t *p, uint8_t v);
SF_API sf_result_t sf_mqb_point_set_float(sf_mqb_point_t *p, float v);
SF_API sf_result_t sf_mqb_point_set_uint (sf_mqb_point_t *p, uint32_t v);
SF_API sf_result_t sf_mqb_point_get_byte (const sf_mqb_point_t *p, uint8_t *out);
SF_API sf_result_t sf_mqb_point_get_float(const sf_mqb_point_t *p, float *out);
SF_API sf_result_t sf_mqb_point_get_uint (const sf_mqb_point_t *p, uint32_t *out);
SF_API int32_t     sf_mqb_point_unk08    (const sf_mqb_point_t *p);
SF_API void        sf_mqb_point_set_unk08(sf_mqb_point_t *p, int32_t v);
SF_API float       sf_mqb_point_unk10    (const sf_mqb_point_t *p);
SF_API void        sf_mqb_point_set_unk10(sf_mqb_point_t *p, float v);
SF_API float       sf_mqb_point_unk14    (const sf_mqb_point_t *p);
SF_API void        sf_mqb_point_set_unk14(sf_mqb_point_t *p, float v);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MQB_H */
