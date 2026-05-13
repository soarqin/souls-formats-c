/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — shared MSB surface.
 *
 * Common abstractions for the Sekiro MSBS, Elden Ring MSBE, and AC6 MSBVI
 * format families.
 */

#ifndef SOULS_FORMATS_SF_MSB_H
#define SOULS_FORMATS_SF_MSB_H

#include "sf_common.h"

#ifdef __cplusplus
#define _Static_assert static_assert
#endif

#include "sf_math.h"

#ifdef __cplusplus
#undef _Static_assert
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_msb_model_kind {
    SF_MSB_MODEL_MAP_PIECE = 0,
    SF_MSB_MODEL_OBJECT = 1,
    SF_MSB_MODEL_CHARACTER = 2,
    SF_MSB_MODEL_PLAYER = 3,
    SF_MSB_MODEL_COLLISION = 4,
    SF_MSB_MODEL_NAVMESH = 5,
    SF_MSB_MODEL_OTHER = UINT32_MAX,
} sf_msb_model_kind_t;

enum { SF_MSB_MODEL_KIND_COUNT_ = 7 };

#if defined(__cplusplus)
static_assert(SF_MSB_MODEL_KIND_COUNT_ == 7, "sf_msb_model_kind_t drift");
#else
_Static_assert(SF_MSB_MODEL_KIND_COUNT_ == 7, "sf_msb_model_kind_t drift");
#endif

typedef enum sf_msb_event_kind {
    SF_MSB_EVENT_LIGHT = 0,
    SF_MSB_EVENT_SOUND = 1,
    SF_MSB_EVENT_SFX = 2,
    SF_MSB_EVENT_WIND = 3,
    SF_MSB_EVENT_TREASURE = 4,
    SF_MSB_EVENT_GENERATOR = 5,
    SF_MSB_EVENT_MESSAGE = 6,
    SF_MSB_EVENT_OBJ_ACT = 7,
    SF_MSB_EVENT_SPAWN_POINT = 8,
    SF_MSB_EVENT_MAP_OFFSET = 9,
    SF_MSB_EVENT_NAVMESH = 10,
    SF_MSB_EVENT_ENVIRONMENT = 11,
    SF_MSB_EVENT_PSEUDO_MULTIPLAYER = 12,
    SF_MSB_EVENT_PATROL_INFO = 13,
    SF_MSB_EVENT_PLATOON_INFO = 14,
    SF_MSB_EVENT_MOUNT_JUMP = 15,
    SF_MSB_EVENT_NPC_INFO_STAY = 16,
    SF_MSB_EVENT_OTHER = UINT32_MAX,
} sf_msb_event_kind_t;

enum { SF_MSB_EVENT_KIND_COUNT_ = 17 };

#if defined(__cplusplus)
static_assert(SF_MSB_EVENT_KIND_COUNT_ == 17, "sf_msb_event_kind_t drift");
#else
_Static_assert(SF_MSB_EVENT_KIND_COUNT_ == 17, "sf_msb_event_kind_t drift");
#endif

typedef enum sf_msb_region_kind {
    SF_MSB_REGION_POINT = 0,
    SF_MSB_REGION_CIRCLE = 1,
    SF_MSB_REGION_SPHERE = 2,
    SF_MSB_REGION_CYLINDER = 3,
    SF_MSB_REGION_RECT = 4,
    SF_MSB_REGION_BOX = 5,
    SF_MSB_REGION_OTHER = UINT32_MAX,
} sf_msb_region_kind_t;

enum { SF_MSB_REGION_KIND_COUNT_ = 7 };

#if defined(__cplusplus)
static_assert(SF_MSB_REGION_KIND_COUNT_ == 7, "sf_msb_region_kind_t drift");
#else
_Static_assert(SF_MSB_REGION_KIND_COUNT_ == 7, "sf_msb_region_kind_t drift");
#endif

typedef enum sf_msb_part_kind {
    SF_MSB_PART_MAP_PIECE = 0,
    SF_MSB_PART_OBJECT = 1,
    SF_MSB_PART_CHARACTER = 2,
    SF_MSB_PART_PLAYER = 3,
    SF_MSB_PART_COLLISION = 4,
    SF_MSB_PART_DUMMY_OBJECT = 5,
    SF_MSB_PART_DUMMY_CHARACTER = 6,
    SF_MSB_PART_CONNECT_COLLISION = 7,
    SF_MSB_PART_ASSET = 8,
    SF_MSB_PART_OTHER = UINT32_MAX,
} sf_msb_part_kind_t;

enum { SF_MSB_PART_KIND_COUNT_ = 10 };

#if defined(__cplusplus)
static_assert(SF_MSB_PART_KIND_COUNT_ == 10, "sf_msb_part_kind_t drift");
#else
_Static_assert(SF_MSB_PART_KIND_COUNT_ == 10, "sf_msb_part_kind_t drift");
#endif

typedef sf_vec3_t sf_msb_point3_t;

typedef struct sf_msb_transform {
    sf_vec3_t position;
    sf_vec3_t rotation;
    sf_vec3_t scale;
} sf_msb_transform_t;

typedef struct sf_msb_entry sf_msb_entry_t;
typedef struct sf_msb_model sf_msb_model_t;
typedef struct sf_msb_event sf_msb_event_t;
typedef struct sf_msb_region sf_msb_region_t;
typedef struct sf_msb_part sf_msb_part_t;
typedef struct sf_msb_route sf_msb_route_t;
typedef struct sf_msb_layer sf_msb_layer_t;

SF_API sf_result_t sf_msb_entry_get_name(const sf_msb_entry_t *e, char **out_name);

SF_API sf_msb_part_kind_t sf_msb_part_get_kind(const sf_msb_part_t *p);
SF_API sf_result_t sf_msb_part_get_name(const sf_msb_part_t *p, char **out);
SF_API sf_result_t sf_msb_part_get_transform(const sf_msb_part_t *p, sf_msb_transform_t *out);
SF_API int32_t sf_msb_part_get_model_index(const sf_msb_part_t *p);

SF_API sf_msb_model_kind_t sf_msb_model_get_kind(const sf_msb_model_t *m);
SF_API sf_result_t sf_msb_model_get_name(const sf_msb_model_t *m, char **out);

SF_API sf_msb_event_kind_t sf_msb_event_get_kind(const sf_msb_event_t *e);
SF_API sf_result_t sf_msb_event_get_name(const sf_msb_event_t *e, char **out);

SF_API sf_msb_region_kind_t sf_msb_region_get_kind(const sf_msb_region_t *r);
SF_API sf_result_t sf_msb_region_get_name(const sf_msb_region_t *r, char **out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MSB_H */
